#pragma once

#include <chrono>
#include <functional>
#include <mutex>
#include <atomic>
#include <future>
#include <cassert>

namespace ts {
    typedef uint32_t task_id;
    typedef std::function<void(const std::string& /* task name */, const std::exception_ptr& /* exception */)> task_exception_handler;

    /**
     * A basic task executor & scheduler for one time and repeating tasks
     * Note: All methods are thread save or it's specified otherwise
     */
    class task_executor {
        public:
            task_executor(size_t /* num threads */, const std::string& /* thread prefix */);
            ~task_executor();

            void set_exception_handler(task_exception_handler);

            /**
             * Note: This method is not thread save.
             * @returns `true` if all actions have been successfully shut down
             */
            bool shutdown(const std::chrono::system_clock::time_point & /* timeout */);

            /**
             * Cancel a task. If the task is currently executing it will not block.
             * @returns `true` if the task has been found and `false` if the task isn't known.
             */
            bool cancel_task(task_id /* task id */);

            /**
             * Cancel a task with the possibility to wait until it has finished.
             */
            std::future<void> cancel_task_joinable(task_id /* task id */);

            /**
             * @returns `true` if the task has successfully be enqueued for scheduling.
             */
            bool schedule(task_id& /* task handle */, std::string /* name */, std::function<void()> /* callback */);

            /**
             * @returns `true` if the task has successfully be enqueued for repeating scheduling.
             */
            bool schedule_repeating(task_id& /* task handle */,
                                    std::string /* name */,
                                    std::chrono::nanoseconds /* interval */,
                                    std::function<void(const std::chrono::system_clock::time_point& /* last scheduled */)> /* callback */);

            void print_statistics(const std::function<void(const std::string &)>& /* print function */, bool /* print task list */);
        private:
            struct task;
            struct task_recurring;
            struct task_context;
            struct executor_context;

            enum struct task_cancel_result {
                not_found,
                pending_canceled,
                running_canceled,
            };

            std::vector<std::shared_ptr<executor_context>> executors{};
            std::shared_ptr<task_context> task_context;

            /**
             * Enqueue the task into the task queue.
             * Attention:
             * 1. The task context mutex must be hold by the caller
             * 2. The task should not be enqueued already
             */
            void enqueue_recurring_task(task_recurring* /* task */);
            [[nodiscard]] task_cancel_result internal_cancel_task(std::future<void>* /* future */, task_id /* task id */);

            static void executor(std::shared_ptr<executor_context> /* context shared pointer */);
            static void abort_exception_handler(const std::string& /* task name */, const std::exception_ptr& /* exception */);
    };

    /**
     * Helper class for tasks which could be executed multiple times.
     * It will avoid execution stacking while the task is executing.
     * The task will never be executed twice, only sequential.
     * Note: If the `multi_shot_task` handle gets deleted no enqueued tasks will be executed.
     */
    struct multi_shot_task {
        public:
            explicit multi_shot_task() {}

            multi_shot_task(std::shared_ptr<task_executor> executor, std::string task_name, std::function<void()> callback)
                : inner{std::make_shared<execute_inner>(std::move(executor), std::move(task_name), std::move(callback))} {
                std::weak_ptr weak_inner{this->inner};
                this->inner->callback_wrapper = [weak_inner]{
                    auto callback_inner = weak_inner.lock();
                    if(!callback_inner) {
                        return;
                    }

                    auto result = callback_inner->schedule_kind.exchange(2);
                    assert(result == 1);
                    (void) result;

                    try {
                        (callback_inner->callback)();
                        execute_finished(&*callback_inner);
                    } catch (...) {
                        execute_finished(&*callback_inner);
                        std::rethrow_exception(std::current_exception());
                    }
                };
            }

            multi_shot_task(const multi_shot_task&) = default;
            multi_shot_task(multi_shot_task&& other) = default;

            inline multi_shot_task& operator=(const multi_shot_task& other) {
                this->inner = other.inner;
                return *this;
            }

            inline multi_shot_task& operator=(multi_shot_task&& other) {
                this->inner = std::move(other.inner);
                return *this;
            }

            /**
             * @returns `true` if the task has successfully be enqueued or is already enqueued
             *           and `false` if the `schedule` call failed or we have no task.
             */
            inline bool enqueue() {
                auto& inner_ = this->inner;

                if(!inner_) {
                    return false;
                }

                {
                    //CAS loop: https://preshing.com/20150402/you-can-do-any-kind-of-atomic-read-modify-write-operation/

                    uint8_t current_state = inner_->schedule_kind.load();
                    uint8_t new_state;
                    do {
                        switch(current_state) {
                            case 0:
                                /* no execute has been scheduled */
                                new_state = 1;
                                break;

                            case 1:
                            case 3:
                                /* an execute is already scheduled */
                                return true;

                            case 2:
                                /* we're already executing now but we need a new execute */
                                new_state = 3;
                                return true;

                            default:
                                assert(false);
                                return false;
                        }
                    } while(!inner_->schedule_kind.compare_exchange_weak(current_state, new_state, std::memory_order_relaxed, std::memory_order_relaxed));
                }

                task_id task_id_;
                auto result = inner_->executor->schedule(task_id_, inner_->task_name, inner->callback_wrapper);
                if(!result) {
                    /*
                     * Task isn't scheduled any more. We failed to schedule it.
                     * Note: The task might got rescheduled again so may more than only one schedule attempt fail
                     *       in total.
                     */
                    inner_->schedule_kind = 0;
                    return false;
                }

                return true;
            }
        private:
            struct execute_inner {
                explicit execute_inner(std::shared_ptr<task_executor> executor, std::string name, std::function<void()> callback) noexcept
                    : task_name{std::move(name)}, executor{std::move(executor)}, callback{std::move(callback)} {}

                std::string task_name;
                std::shared_ptr<task_executor> executor;

                std::function<void()> callback;
                std::function<void()> callback_wrapper;

                /**
                 *  `0` not scheduled
                 *  `1` scheduled
                 *  `2` executing
                 *  `3` executing with reschedule
                 */
                std::atomic<uint8_t> schedule_kind{0};
            };

            std::shared_ptr<execute_inner> inner{};

            inline static void execute_finished(execute_inner* inner) {
                auto current_state = inner->schedule_kind.load();
                uint8_t new_state;
                do {
                    switch(current_state) {
                        case 0:
                        case 1:
                            assert(false);
                            return;

                        case 2:
                            new_state = 0;
                            break;

                        case 3:
                            new_state = 1;
                            break;

                        default:
                            assert(false);
                            return;
                    };
                } while(!inner->schedule_kind.compare_exchange_weak(current_state, new_state, std::memory_order_relaxed, std::memory_order_relaxed));

                if(new_state == 1) {
                    /* a reschedule was requested */

                    task_id task_id_;
                    if(!inner->executor->schedule(task_id_, inner->task_name, inner->callback_wrapper)) {
                        /*
                         * Task isn't scheduled any more. We failed to schedule it.
                         * Note: The task might got rescheduled again so may more than only one schedule attempt fail
                         *       in total.
                         */
                        inner->schedule_kind = 0;
                    }
                }
            }
    };
}
