//
// Created by WolverinDEV on 21/02/2021.
//

#include "./task_executor.h"
#include "./threads.h"
#include <thread>
#include <cassert>
#include <iostream>
#include <algorithm>
#include <optional>
#include <condition_variable>

using std::chrono::system_clock;
using namespace ts;

struct task_executor::task {
    task_id id{0};
    std::string name{};
    std::function<void()> callback{};

    /* will be set to true if the task has been canceled but is executing right now */
    bool canceled{false};
    std::optional<std::promise<void>> finish_callback{};

    task* next{nullptr};
};

struct task_executor::task_recurring {
    task_id id{0};
    std::string name{};

    bool shutdown{false};
    std::optional<std::promise<void>> finish_callback{};

    std::chrono::nanoseconds interval{};
    std::chrono::system_clock::time_point last_invoked{};
    std::chrono::system_clock::time_point scheduled_invoke{};

    std::function<void(const std::chrono::system_clock::time_point& /* last executed */)> callback{};

    task_recurring* next{nullptr};
};

struct task_executor::task_context {
    std::mutex mutex{};
    std::condition_variable notify{};

    bool shutdown{false};
    task_id id_index{1};

    size_t task_count{};
    task* task_head{nullptr};
    task** task_tail{&this->task_head};

    size_t task_recurring_count{};
    task_recurring* task_recurring_head{nullptr};

    task_exception_handler exception_handler{task_executor::abort_exception_handler};
};

struct task_executor::executor_context {
    task_executor* handle{nullptr};
    std::thread thread_handle{};

    std::shared_ptr<struct task_context> task_context{};

    /**
     * Must be accessed while holding the task_context.mutex and shall never be changed except for the executor.
     * Lifetime will be granted while holding the lock.
     */
    task* executing_task{nullptr};
    task_recurring* executing_recurring_task{nullptr};
};

task_executor::task_executor(size_t num_threads, const std::string &thread_prefix) {
    this->task_context = std::make_shared<struct task_context>();

    this->executors.reserve(num_threads);
    for(size_t index{0}; index < num_threads; index++) {
        auto handle = std::make_shared<executor_context>();
        handle->handle = this;
        handle->task_context = this->task_context;
        handle->thread_handle = std::thread(task_executor::executor, handle);
        if(!thread_prefix.empty()) {
            threads::name(handle->thread_handle, thread_prefix + std::to_string(index + 1));
        }
        this->executors.push_back(std::move(handle));
    }
}

task_executor::~task_executor() {
    {
        std::lock_guard task_lock{this->task_context->mutex};
        for(auto& thread : this->executors) {
            if(!thread->executing_recurring_task) {
                continue;
            }

            /* TODO: Log error */
            thread->executing_recurring_task->shutdown = true;
        }

        while(this->task_context->task_head) {
            auto task = std::exchange(this->task_context->task_head, this->task_context->task_head->next);
            delete task;
        }
        this->task_context->task_tail = &this->task_context->task_head;
        this->task_context->task_count = 0;

        while(this->task_context->task_recurring_head) {
            auto task = std::exchange(this->task_context->task_recurring_head, this->task_context->task_recurring_head->next);
            delete task;
        }
        this->task_context->task_recurring_count = 0;
    }

    for(auto& thread : this->executors) {
        if(thread->thread_handle.joinable()) {
            /* TODO: Print an error */
            thread->thread_handle.detach();
        }
    }
}

void task_executor::set_exception_handler(task_exception_handler handler) {
    std::lock_guard task_lock{this->task_context->mutex};
    this->task_context->exception_handler = std::move(handler);
}

void task_executor::abort_exception_handler(const std::string &task, const std::exception_ptr &exception) {
    std::string message{};
    try {
        std::rethrow_exception(exception);
    } catch (const std::exception& ex) {
        message = "std::exception::what() -> " + std::string{ex.what()};
    } catch(...) {
        message = "unknown exception";
    }

    std::cerr << "task_executor encountered an exception while executing task " << task << ": " << message << std::endl;
    abort();
}

bool task_executor::shutdown(const std::chrono::system_clock::time_point &timeout) {
    {
        std::lock_guard task_lock{this->task_context->mutex};
        this->task_context->shutdown = true;
        this->task_context->notify.notify_all();
    }

    for(auto& thread : this->executors) {
        if(timeout.time_since_epoch().count() > 0) {
            auto now = system_clock::now();
            if(now > timeout) {
                /* failed to join all executors */
                return false;
            }

            if(!threads::timed_join(thread->thread_handle, timeout - now)) {
                /* thread failed to join */
                return false;
            }
        } else {
            threads::save_join(thread->thread_handle, false);
        }
    }

    return true;
}

bool task_executor::schedule(task_id &task_id, std::string task_name, std::function<void()> callback) {
    auto& task_context_ = this->task_context;

    auto task = std::make_unique<task_executor::task>();
    task->name = std::move(task_name);
    task->callback = std::move(callback);

    std::lock_guard task_lock{task_context_->mutex};
    if(task_context_->shutdown) {
        return false;
    }

    task->id = task_context_->id_index++;
    if(!task->id) {
        /* the task ids wrapped around I guess */
        task->id = task_context_->id_index++;
    }
    task_id = task->id;

    auto task_ptr = task.release();
    task_context_->task_count++;
    *task_context_->task_tail = task_ptr;
    task_context_->task_tail = &task_ptr->next;
    task_context_->notify.notify_one();
    return true;
}

bool task_executor::schedule_repeating(task_id &task_id, std::string task_name, std::chrono::nanoseconds interval,
                                       std::function<void(const std::chrono::system_clock::time_point &)> callback) {
    auto& task_context_ = this->task_context;

    auto task = std::make_unique<task_executor::task_recurring>();
    task->name = std::move(task_name);
    task->callback = std::move(callback);
    task->interval = std::move(interval);
    task->scheduled_invoke = std::chrono::system_clock::now();

    std::lock_guard task_lock{task_context_->mutex};
    if(task_context_->shutdown) {
        return false;
    }

    task->id = task_context_->id_index++;
    if(!task->id) {
        /* the task ids wrapped around I guess */
        task->id = task_context_->id_index++;
    }
    task_id = task->id;

    task_context_->task_recurring_count++;
    this->enqueue_recurring_task(task.release());
    task_context_->notify.notify_one();
    return true;
}

void task_executor::enqueue_recurring_task(task_recurring *task) {
    auto& task_context_ = this->task_context;

    if(!task_context_->task_recurring_head) {
        /* No tasks pending. We could easily enqueue the task. */
        task->next = nullptr;
        task_context_->task_recurring_head = task;
    } else {
        /* Find the correct insert spot */
        task_recurring* previous_task{nullptr};
        task_recurring* next_task{task_context_->task_recurring_head};
        while(true) {
            if(next_task->scheduled_invoke > task->scheduled_invoke) {
                break;
            }

            previous_task = next_task;
            next_task = next_task->next;

            if(!next_task) {
                /* We reached the queue end. */
                break;
            }
        }

        task->next = next_task;
        if(!previous_task) {
            /* we're inserting the task as head */
            assert(next_task == task_context_->task_recurring_head);
            task_context_->task_recurring_head = task;
        } else {
            previous_task->next = task;
        }
    }
}

task_executor::task_cancel_result task_executor::internal_cancel_task(std::future<void> *future, task_id task_id) {
    auto& task_context_ = this->task_context;

    std::unique_lock task_lock{task_context->mutex};
    /* 1. Search for a pending normal task */
    {
        task* previous_task{nullptr};
        task* current_task{task_context_->task_head};
        while(current_task) {
            if(current_task->id == task_id) {
                /* We found our task. Just remove and delete it. */
                if(previous_task) {
                    previous_task->next = current_task->next;
                } else {
                    assert(task_context_->task_head == current_task);
                    if(current_task->next) {
                        assert(task_context_->task_tail != &current_task->next);
                        task_context_->task_head = current_task->next;
                    } else {
                        assert(task_context_->task_tail == &current_task->next);
                        task_context_->task_head = nullptr;
                        task_context_->task_tail = &task_context_->task_head;
                    }
                }

                assert(task_context_->task_count > 0);
                task_context_->task_count--;
                task_lock.unlock();

                delete current_task;
                return task_cancel_result::pending_canceled;
            }

            previous_task = current_task;
            current_task = current_task->next;
        }
    }

    /* 2. Search for a pending recurring task */
    {
        task_recurring* previous_task{nullptr};
        task_recurring* current_task{task_context_->task_recurring_head};
        while(current_task) {
            if(current_task->id == task_id) {
                /* We found our task. Just remove and delete it. */
                if(previous_task) {
                    previous_task->next = current_task->next;
                } else {
                    assert(task_context_->task_recurring_head == current_task);
                    task_context_->task_recurring_head = current_task->next;
                }

                assert(task_context_->task_recurring_count > 0);
                task_context_->task_recurring_count--;
                task_lock.unlock();

                delete current_task;
                return task_cancel_result::pending_canceled;
            }

            previous_task = current_task;
            current_task = current_task->next;
        }
    }

    /* 3. Our task does not seem to pend anywhere. May it already gets executed. */
    for(auto& executor : this->executors) {
        if(executor->executing_task && executor->executing_task->id == task_id) {
            auto task_handle = executor->executing_task;
            if(task_handle->canceled) {
                /* task has already been canceled */
                return task_cancel_result::not_found;
            }

            task_handle->canceled = true;
            if(future) {
                assert(!task_handle->finish_callback.has_value());
                task_handle->finish_callback = std::make_optional(std::promise<void>{});
                *future = task_handle->finish_callback->get_future();
            }

            /*
             * It gets executed right now.
             * The task itself will be deleted by the executor.
             * Note: No need to decrease the task count here since it has already been
             *       decreased when receiving the task by the executor.
             */
            return task_cancel_result::running_canceled;
        }

        if(executor->executing_recurring_task && executor->executing_recurring_task->id == task_id) {
            auto task_handle = executor->executing_recurring_task;
            if(task_handle->shutdown) {
                /* the task has already been canceled */
                return task_cancel_result::not_found;
            }

            /*
             * It gets executed right now.
             * Setting shutdown flag to prevent rescheduling.
             * The task will be deleted by the executor itself.
             */
            task_handle->shutdown = true;
            if(future) {
                assert(!task_handle->finish_callback.has_value());
                task_handle->finish_callback = std::make_optional(std::promise<void>{});
                *future = task_handle->finish_callback->get_future();
            }

            assert(task_context_->task_recurring_count > 0);
            task_context_->task_recurring_count--;
            return task_cancel_result::running_canceled;
        }
    }

    return task_cancel_result::not_found;
}

bool task_executor::cancel_task(task_id task_id) {
    switch (this->internal_cancel_task(nullptr, task_id)) {
        case task_cancel_result::pending_canceled:
        case task_cancel_result::running_canceled:
            return true;

        case task_cancel_result::not_found:
            return false;

        default:
            assert(false);
            return false;
    }
}

std::future<void> task_executor::cancel_task_joinable(task_id task_id) {
    std::future<void> result{};
    switch (this->internal_cancel_task(&result, task_id)) {
        case task_cancel_result::pending_canceled:
            /* May throw an exception? */

        case task_cancel_result::not_found: {
            std::promise<void> promise{};
            promise.set_value();
            return promise.get_future();
        }

        case task_cancel_result::running_canceled:
            return result;

        default:
            assert(false);
            return result;
    }
}

void task_executor::executor(std::shared_ptr<executor_context> executor_context) {
    auto& task_context = executor_context->task_context;

    std::unique_lock task_lock{task_context->mutex};
    while(true) {
        assert(task_lock.owns_lock());
        if(task_context->shutdown) {
            break;
        }

        if(task_context->task_head) {
            auto task = task_context->task_head;
            if(task->next) {
                assert(task_context->task_tail != &task->next);
                task_context->task_head = task->next;
            } else {
                assert(task_context->task_tail == &task->next);
                task_context->task_head = nullptr;
                task_context->task_tail = &task_context->task_head;
            }

            assert(task_context->task_count > 0);
            task_context->task_count--;

            executor_context->executing_task = task;
            task_lock.unlock();

            try {
                task->callback();
            } catch (...) {
                auto exception = std::current_exception();

                task_lock.lock();
                auto handler = task_context->exception_handler;
                task_lock.unlock();

                handler(task->name, exception);
            }

            task_lock.lock();
            executor_context->executing_task = nullptr;

            if(task->finish_callback.has_value()) {
                task->finish_callback->set_value();
            }
            delete task;
            continue;
        }

        auto execute_timestamp = std::chrono::system_clock::now();
        std::chrono::system_clock::time_point next_timestamp{};
        if(task_context->task_recurring_head) {
            if(task_context->task_recurring_head->scheduled_invoke <= execute_timestamp) {
                auto task = task_context->task_recurring_head;
                task_context->task_recurring_head = task->next;

                executor_context->executing_recurring_task = task;
                task_lock.unlock();

                try {
                    task->callback(task->last_invoked);
                } catch (...) {
                    auto exception = std::current_exception();

                    task_lock.lock();
                    auto handler = task_context->exception_handler;
                    task_lock.unlock();

                    handler(task->name, exception);
                }

                task->last_invoked = execute_timestamp;

                auto expected_next_invoke = execute_timestamp + std::chrono::duration_cast<std::chrono::system_clock::duration>(task->interval);
                task->scheduled_invoke = std::max(std::chrono::system_clock::now(), expected_next_invoke);

                task_lock.lock();
                executor_context->executing_recurring_task = nullptr;
                if(task->shutdown) {
                    if(task->finish_callback) {
                        task->finish_callback->set_value();
                    }

                    delete task;
                } else {
                    executor_context->handle->enqueue_recurring_task(task);
                }
                continue;
            } else {
                next_timestamp = task_context->task_recurring_head->scheduled_invoke;
            }
        }

        if(next_timestamp.time_since_epoch().count() > 0) {
            task_context->notify.wait_until(task_lock, next_timestamp);
        } else {
            task_context->notify.wait(task_lock);
        }
    }
}

void task_executor::print_statistics(const std::function<void(const std::string &)>& println, bool print_task_list) {
    println("Executor count: " + std::to_string(this->executors.size()));
    std::lock_guard task_lock{this->task_context->mutex};
    if(print_task_list) {
        println("Tasks (" + std::to_string(this->task_context->task_count) + "):");
        {
            auto head = this->task_context->task_head;
            while(head) {
                println(" - " + head->name);
                head = head->next;
            }
        }
        println("Recurring task count (" + std::to_string(this->task_context->task_recurring_count) + "):");
        {
            auto head = this->task_context->task_recurring_head;
            while(head) {
                println(" - " + head->name);
                head = head->next;
            }
        }
    } else {
        println("Task count: " + std::to_string(this->task_context->task_count));
        println("Recurring task count: " + std::to_string(this->task_context->task_recurring_count));
    }
}