//
// Created by WolverinDEV on 29/07/2020.
//

#include <utility>
#include <ThreadPool/ThreadHelper.h>
#include <log/LogUtils.h>

#include "./ServerCommandExecutor.h"
#include "protocol/PacketDecoder.h"
#include "protocol/RawCommand.h"
#include "src/client/voice/VoiceClientConnection.h"

using namespace ts;
using namespace ts::server;
using namespace ts::command;

namespace ts::server {
    struct ServerCommandQueueInner {
        spin_mutex pending_commands_lock{};
        command::ReassembledCommand* pending_commands_head{nullptr};
        command::ReassembledCommand** pending_commands_tail{&pending_commands_head};
        bool has_command_handling_scheduled{false};

        ~ServerCommandQueueInner() {
            auto head = this->pending_commands_head;
            while(head) {
                auto cmd = head->next_command;
                ReassembledCommand::free(head);
                head = cmd;
            }
        }

        void reset() {
            std::unique_lock pc_lock{this->pending_commands_lock};
            auto head = std::exchange(this->pending_commands_head, nullptr);
            this->pending_commands_tail = &this->pending_commands_head;
            this->has_command_handling_scheduled = false;
            pc_lock.unlock();

            while(head) {
                auto cmd = head->next_command;
                ReassembledCommand::free(head);
                head = cmd;
            }
        }

        /**
         * @param command The target command to enqueue.
         *                Ownership will be taken.
         * @returns `true` if command handling has already been schedules and `false` if not
         */
        bool enqueue(ReassembledCommand *command){
            std::lock_guard pc_lock{this->pending_commands_lock};
            *this->pending_commands_tail = command;
            this->pending_commands_tail = &command->next_command;

            return std::exchange(this->has_command_handling_scheduled, true);
        }

        ReassembledCommand* pop_command(bool& more_pending) {
            std::lock_guard pc_lock{this->pending_commands_lock};
            auto result = this->pending_commands_head;

            if(!result) {
                more_pending = false;
                this->has_command_handling_scheduled = false;
            } else if(result->next_command) {
                more_pending = true;
                this->pending_commands_head = result->next_command;
            } else {
                /* We assume true here since we might get new commands while the handler itself is still handling our current result. */
                more_pending = true;
                this->pending_commands_head = nullptr;
                this->pending_commands_tail = &this->pending_commands_head;
            }

            return result;
        }
    };
}

bool ServerCommandHandler::execute_handling() {
    bool more_pending;
    std::unique_ptr<ReassembledCommand, void(*)(ReassembledCommand*)> pending_command{nullptr, ReassembledCommand::free};
    while(true) {
        pending_command.reset(this->inner->pop_command(more_pending));
        if(!pending_command) {
            break;
        }

        try {
            auto result = this->handle_command(std::string_view{pending_command->command(), pending_command->length()});
            if(!result) {
                /* flush all commands */
                this->inner->reset();
                more_pending = false;
                break;
            }
        } catch (std::exception& ex) {
            logCritical(LOG_GENERAL, "Exception reached command execution root! {}",ex.what());
        }

        break; /* Maybe handle more than one command? Maybe some kind of time limit? */
    }

    return more_pending;
}

ServerCommandQueue::ServerCommandQueue(std::shared_ptr<ServerCommandExecutor> executor, std::unique_ptr<ServerCommandHandler> command_handler) :
                                       executor{std::move(executor)},
                                       command_handler{command_handler.release()} {
    assert(this->command_handler);
    this->inner = std::make_shared<ServerCommandQueueInner>();
    this->command_handler->inner = this->inner;
}

ServerCommandQueue::~ServerCommandQueue() = default;

void ServerCommandQueue::reset() {
    this->inner->reset();
}

void ServerCommandQueue::enqueue_command_string(const std::string_view &buffer) {
    auto command = ReassembledCommand::allocate(buffer.length());
    memcpy(command->command(), buffer.data(), command->length());
    this->enqueue_command_execution(command);
}

void ServerCommandQueue::enqueue_command_execution(ReassembledCommand *command) {
    assert(!command->next_command);

    bool command_handling_scheduled = this->inner->enqueue(command);
    if(!command_handling_scheduled) {
        this->executor->enqueue_handler(this->command_handler);
    }
}

#if 0
void ServerCommandQueue::execute_handle_command_packets(const std::chrono::system_clock::time_point& /* scheduled */) {
    if(!this->client->getServer() || this->client->connectionState() >= ConnectionState::DISCONNECTING) {
        return;
    }

    std::unique_ptr<ReassembledCommand, void(*)(ReassembledCommand*)> pending_command{nullptr, ReassembledCommand::free};
    while(true) {
        {
            std::lock_guard pc_lock{this->pending_commands_lock};
            pending_command.reset(this->pending_commands_head);
            if(!pending_command) {
                this->has_command_handling_scheduled = false;
                return;
            } else if(pending_command->next_command) {
                this->pending_commands_head = pending_command->next_command;
            } else {
                this->pending_commands_head = nullptr;
                this->pending_commands_tail = &this->pending_commands_head;
            }
        }

        auto startTime = std::chrono::system_clock::now();
        try {
            this->client->handlePacketCommand(pipes::buffer_view{pending_command->command(), pending_command->length()});
        } catch (std::exception& ex) {
            logCritical(this->client->getServerId(), "{} Exception reached command execution root! {}", CLIENT_STR_LOG_PREFIX_(this->client), ex.what());
        }

        auto end = std::chrono::system_clock::now();
        if(end - startTime > std::chrono::milliseconds(10)) {
            logError(this->client->getServerId(),
                     "{} Handling of command packet needs more than 10ms ({}ms)",
                     CLIENT_STR_LOG_PREFIX_(this->client),
                     duration_cast<std::chrono::milliseconds>(end - startTime).count()
            );
        }

        break; /* Maybe handle more than one command? Maybe some kind of time limit? */
    }

    auto voice_server = this->client->getVoiceServer();
    if(voice_server) {
        voice_server->schedule_command_handling(client);
    }
}
#endif

ServerCommandExecutor::ServerCommandExecutor(size_t threads) : thread_count_{threads} {
    this->threads_.reserve(threads);

    for(size_t index{0}; index < threads; index++) {
        auto& thread = this->threads_.emplace_back(&ServerCommandExecutor::thread_entry_point, this);
        threads::name(thread, "cmd executor " + std::to_string(index + 1));
    }
}
ServerCommandExecutor::~ServerCommandExecutor() {
    this->shutdown();
}

void ServerCommandExecutor::shutdown() {
    {
        std::lock_guard handler_lock{this->handler_mutex};
        this->handler_shutdown = true;
        this->handler_notify.notify_all();
    }

    for(auto& thread : this->threads_) {
        threads::save_join(thread, true);
    }
}

void ServerCommandExecutor::enqueue_handler(const std::shared_ptr<ServerCommandHandler> &handler) {
    std::lock_guard handler_lock{this->handler_mutex};
    if(handler->next_handler || this->handler_tail == &handler->next_handler) {
        /* handler already scheduled */
        return;
    }

    if(handler->executing || handler->reschedule) {
        /* Handle is currently executing */
        handler->reschedule = true;
        return;
    }

    *this->handler_tail = handler;
    this->handler_tail = &handler->next_handler;

    this->handler_notify.notify_one();
}

void ServerCommandExecutor::thread_entry_point(void *ptr_this) {
    reinterpret_cast<ServerCommandExecutor*>(ptr_this)->executor();
}

void ServerCommandExecutor::executor() {
    std::unique_lock handler_lock{this->handler_mutex};
    while(!this->handler_shutdown) {
        this->handler_notify.wait(handler_lock, [&]{
            return this->handler_shutdown || this->handler_head != nullptr;
        });

        if(this->handler_shutdown) {
            break;
        }

        if(!this->handler_head) {
            continue;
        }

        auto executor = this->handler_head;
        if(executor->next_handler) {
            this->handler_head = executor->next_handler;
        } else {
            this->handler_head = nullptr;
            this->handler_tail = &this->handler_head;
        }

        executor->executing = true;
        executor->reschedule = false;
        executor->next_handler = nullptr;
        handler_lock.unlock();

        auto reschedule = executor->execute_handling();

        handler_lock.lock();
        reschedule |= std::exchange(executor->reschedule, false);
        executor->executing = false;

        if(reschedule) {
            assert(!executor->next_handler);

            *this->handler_tail = executor;
            this->handler_tail = &executor->next_handler;

            /* No need to notify anybody since we'll be executing him again */
        }
    }
}