#pragma once

#include <misc/spin_mutex.h>
#include <pipes/buffer.h>
#include <EventLoop.h>

namespace ts::server {
    class VoiceClient;
}

namespace ts::command {
    struct ReassembledCommand;
}

namespace ts::server {
    class ServerCommandExecutor;
    class ServerCommandQueue;
    struct ServerCommandQueueInner;

    class ServerCommandHandler {
            friend class ServerCommandQueue;
            friend class ServerCommandExecutor;

        public:
            ServerCommandHandler() = default;
            virtual ~ServerCommandHandler() = default;

        protected:
            /**
             * Handle a command.
             * @returns `false` if all other commands should be dropped and no further command handling should be done.
             *          `true` on success.
             */
            virtual bool handle_command(const std::string_view& /* raw command */) = 0;

        private:
            std::shared_ptr<ServerCommandQueueInner> inner{nullptr};

            /* locked by ServerCommandExecutor::handler_mutex */
            std::shared_ptr<ServerCommandHandler> next_handler{nullptr};
            bool executing{false};
            bool reschedule{false};

            /**
             * @returns `true` if more commands need to be handled and `false` if all commands have been handled.
             */
            bool execute_handling();
    };

    class ServerCommandQueue {
        public:
            explicit ServerCommandQueue(std::shared_ptr<ServerCommandExecutor> /* executor */, std::unique_ptr<ServerCommandHandler> /* command handler */);
            ~ServerCommandQueue();

            void reset();

            void enqueue_command_string(const std::string_view& /* payload */);
            /* Attention: The method will take ownership of the command */
            void enqueue_command_execution(command::ReassembledCommand*);
        private:
            std::shared_ptr<ServerCommandExecutor> executor{};
            std::shared_ptr<ServerCommandHandler> command_handler{};
            std::shared_ptr<ServerCommandQueueInner> inner;
    };

    class ServerCommandExecutor {
            friend class ServerCommandQueue;
        public:
            explicit ServerCommandExecutor(size_t /* threads */);
            ~ServerCommandExecutor();

            [[nodiscard]] inline auto thread_count() const { return this->thread_count_; }
            void shutdown();

        protected:
            void enqueue_handler(const std::shared_ptr<ServerCommandHandler>& /* handler */);

        private:
            size_t thread_count_;
            std::vector<std::thread> threads_{};

            std::mutex handler_mutex{};
            std::condition_variable handler_notify{};
            std::shared_ptr<ServerCommandHandler> handler_head{nullptr};
            std::shared_ptr<ServerCommandHandler>* handler_tail{&this->handler_head};
            bool handler_shutdown{false};

            static void thread_entry_point(void* /* this ptr */);
            void executor();
    };
}