#pragma once

#include <mutex>
#include <vector>
#include <cassert>
#include <algorithm>
#include <condition_variable>
#include <thread>

#if !defined(NDEBUG) or 1
    #define DEBUG_RW_MUTEX
    #define rw_mutex_assert(arg) assert(arg)
#else
    #define rw_mutex_assert(arg)
#endif

namespace ts {
    enum rw_action_result {
        success = 0,
        resource_error = -1,
        timeout = -2,
        would_deadlock = -3,
    };

    template <bool write_preferred_>
    struct rw_mutex_options {
        constexpr static auto write_preferred{write_preferred_};
    };


    namespace impl {
        struct mutex_action_validator {
            public:
                inline void try_exclusive_lock_acquire();
                inline void try_shared_lock_acquire();

                inline void exclusive_lock_acquire();
                inline void shared_lock_acquire();


                inline void try_exclusive_lock_release();
                inline void try_shared_lock_release();

                inline void exclusive_lock_release();
                inline void shared_lock_release();


                inline void try_shared_lock_upgrade();
                inline void try_exclusive_lock_downgrade();

                inline void shared_lock_upgrade();
                inline void exclusive_lock_downgrade();

            private:
                std::thread::id exclusive_lock{};
                bool exclusive_lock_upgraded{false};
                std::vector<std::thread::id> shared_lockers{};
        };

        struct dummy_action_validator {
            inline void try_exclusive_lock_acquire() {}
            inline void try_shared_lock_acquire() {}

            inline void exclusive_lock_acquire() {}
            inline void shared_lock_acquire() {}


            inline void try_exclusive_lock_release() {}
            inline void try_shared_lock_release() {}

            inline void exclusive_lock_release() {}
            inline void shared_lock_release() {}


            inline void try_shared_lock_upgrade() {}
            inline void try_exclusive_lock_downgrade() {}

            inline void shared_lock_upgrade() {}
            inline void exclusive_lock_downgrade() {}
        };

        template <typename options_t, typename action_validator_t>
        class rw_mutex_impl {
            public:
                rw_mutex_impl() = default;
                ~rw_mutex_impl();

                template <bool flag>
                rw_mutex_impl(const rw_mutex_impl<options_t, action_validator_t>&) = delete;

                template <bool flag>
                rw_mutex_impl&operator=(const rw_mutex_impl<options_t, action_validator_t>&) = delete;

                /**
                 * Acquire the write lock
                 */
                inline void lock();

                /**
                 * Acquire the write lock with a timeout
                 */
                [[nodiscard]] inline std::cv_status lock_until(const std::chrono::system_clock::time_point& timeout);

                /**
                 * Acquire the write lock with a timeout
                 */
                template <typename duration_t>
                [[nodiscard]] inline std::cv_status lock_for(const duration_t& duration) {
                    auto now = std::chrono::system_clock::now();
                    now += duration;
                    return this->lock_until(now);
                }

                /**
                 * Acquire the read lock
                 */
                inline void lock_shared();

                /**
                 * Acquire the read lock with a timeout
                 */
                [[nodiscard]] inline std::cv_status lock_shared_until(const std::chrono::system_clock::time_point& timeout);

                /**
                 * Acquire the read lock with a timeout
                 */
                template <typename duration_t>
                [[nodiscard]] inline std::cv_status lock_shared_for(const duration_t& duration) {
                    auto now = std::chrono::system_clock::now();
                    now += duration;
                    return this->lock_shared_until(now);
                }

                /**
                 * Release the write lock
                 */
                inline void unlock();

                /**
                 * Release the read lock
                 */
                inline void unlock_shared();

                 /**
                  * Upgrade from a shared lock to an exclusive lock.
                  * Could fail due to the following reasons:
                  *  would_deadlock: Another thread already tried to upgrade the lock and we're not supposed to release the shared lock
                  *  resource_error: Another thread already claimed the write lock while we're waiting
                  *
                  *  Note: This will cause a deadlock if the lock has been locked shared twice
                  */
                 [[nodiscard]] inline rw_action_result upgrade_lock();

                 /**
                  * Upgrade from a shared lock to an exclusive lock.
                  * Could fail due to the following reasons:
                  *  would_deadlock: Another thread already tried to upgrade the lock and we're not supposed to release the shared lock
                  *  resource_error: Another thread already claimed the write lock while we're waiting
                  *  timeout: If the action could not be performed within the given time frame
                  *  Note: This will cause a deadlock if the lock has been locked shared twice
                  */
                [[nodiscard]] inline rw_action_result upgrade_lock_until(const std::chrono::system_clock::time_point& timeout);

                /**
                 *  Upgrade from a shared lock to an exclusive lock with an timeout.
                 *  For return codes see upgrade_lock_until.
                 */
                template <typename duration_t>
                [[nodiscard]] inline rw_action_result upgrade_lock_for(const duration_t& duration) {
                    auto now = std::chrono::system_clock::now();
                    now += duration;
                    return this->upgrade_lock_until(now);
                }

                 /**
                  * Downgrade from an exclusive lock to a shared lock.
                  * No other thread could lock the lock exclusive until unlock_shared() has been called.
                  */
                 inline void downgrade_lock();
            private:
                action_validator_t action_validator_{};

                std::mutex status_lock_{};
                std::condition_variable upgrade_update_{};
                std::condition_variable write_update_{};
                std::condition_variable read_update_{};

                bool write_locked{false};
                bool upgrade_pending{false}, write_lock_upgraded{false};
                uint32_t write_lock_pending{0};
                uint32_t read_lock_pending{0};
                uint32_t read_lock_count{0};
        };
    }

    typedef impl::rw_mutex_impl<rw_mutex_options<true>, impl::mutex_action_validator> rw_safe_mutex;
    typedef impl::rw_mutex_impl<rw_mutex_options<true>, impl::mutex_action_validator> rw_unsafe_mutex;
    typedef rw_safe_mutex rw_mutex;

    struct rw_lock_defered_t {};
    struct rw_lock_shared_t {};
    struct rw_lock_exclusive_t {};

    constexpr rw_lock_defered_t rw_lock_defered{};
    constexpr rw_lock_shared_t rw_lock_shared{};
    constexpr rw_lock_exclusive_t rw_lock_exclusive{};

    template <typename lock_t>
    struct rwshared_lock {
        public:
            explicit rwshared_lock() {
                this->lock_type_ = unlocked;
                this->lock_ = nullptr;
            }
            explicit rwshared_lock(lock_t& lock) : rwshared_lock{lock, rw_lock_shared} {}

            explicit rwshared_lock(lock_t& lock, const rw_lock_defered_t&) : lock_{&lock} {
                this->lock_type_ = unlocked;
            }

            explicit rwshared_lock(lock_t& lock, const rw_lock_shared_t&) : lock_{&lock} {
                this->lock_->lock_shared();
                this->lock_type_ = locked_shared;
            }

            explicit rwshared_lock(lock_t& lock, const rw_lock_exclusive_t&) : lock_{&lock} {
                this->lock_->lock();
                this->lock_type_ = locked_exclusive;
            }

            ~rwshared_lock() {
                if(this->lock_type_ == locked_shared) {
                    this->lock_->unlock_shared();
                } else if(this->lock_type_ == locked_exclusive) {
                    this->lock_->unlock();
                }
            }

            rwshared_lock(const rwshared_lock&) = delete;
            rwshared_lock&operator=(const rwshared_lock&) = delete;
            rwshared_lock&operator=(rwshared_lock&&) noexcept = default;

            /* state testers */
            [[nodiscard]] inline bool exclusive_locked() const {
                return this->lock_type_ == locked_exclusive;
            }

            [[nodiscard]] inline bool shared_locked() const {
                return this->lock_type_ == locked_shared;
            }

            /* basic lock functions */
            inline void lock_shared() {
                rw_mutex_assert(this->lock_type_ == unlocked);
                this->lock_->lock_shared();
                this->lock_type_ = locked_shared;
            }

            [[nodiscard]] inline std::cv_status lock_shared_until(const std::chrono::system_clock::time_point& timeout) {
                rw_mutex_assert(this->lock_type_ == unlocked);
                if(auto error = this->lock_->lock_shared_until(timeout); error == std::cv_status::timeout)
                    return error;
                this->lock_type_ = locked_shared;
                return std::cv_status::no_timeout;
            }

            inline void unlock_shared() {
                rw_mutex_assert(this->lock_type_ == locked_shared);
                this->lock_->unlock_shared();
                this->lock_type_ = unlocked;
            }

            inline void lock_exclusive() {
                rw_mutex_assert(this->lock_type_ == unlocked);
                this->lock_->lock();
                this->lock_type_ = locked_exclusive;
            }

            [[nodiscard]] inline std::cv_status lock_exclusive_until(const std::chrono::system_clock::time_point& timeout) {
                rw_mutex_assert(this->lock_type_ == unlocked);
                if(auto error = this->lock_->lock_until(timeout); error == std::cv_status::timeout)
                    return error;
                this->lock_type_ = locked_exclusive;
                return std::cv_status::no_timeout;
            }

            inline void unlock_exclusive() {
                rw_mutex_assert(this->lock_type_ == locked_exclusive);
                this->lock_->unlock();
                this->lock_type_ = unlocked;
            }

            /* upgrade/downgrade functions */
            [[nodiscard]] inline rw_action_result upgrade_lock() {
                rw_mutex_assert(this->lock_type_ == locked_shared);
                auto err = this->lock_->upgrade_lock();
                if(err != rw_action_result::success) return err;

                this->lock_type_ = locked_exclusive;
                return rw_action_result::success;
            }

            [[nodiscard]] inline rw_action_result upgrade_lock_until(const std::chrono::system_clock::time_point& timeout) {
                rw_mutex_assert(this->lock_type_ == locked_shared);
                auto err = this->lock_->upgrade_lock_until(timeout);
                if(err != rw_action_result::success) return err;

                this->lock_type_ = locked_exclusive;
                return rw_action_result::success;
            }

            inline void downgrade_lock() {
                rw_mutex_assert(this->lock_type_ == locked_exclusive);
                this->lock_->downgrade_lock();
                this->lock_type_ = locked_shared;
            }

            /* auto lock to the target state */
            [[nodiscard]] inline rw_action_result auto_lock_exclusive() {
                if(this->lock_type_ == unlocked)
                    this->lock_exclusive();
                else if(this->lock_type_ == locked_shared)
                    return this->upgrade_lock();
                return rw_action_result::success;
            }

            [[nodiscard]] inline rw_action_result auto_lock_exclusive_until(const std::chrono::system_clock::time_point& timeout) {
                if(this->lock_type_ == unlocked)
                    this->lock_exclusive_until(timeout);
                else if(this->lock_type_ == locked_shared)
                    return this->upgrade_lock_until(timeout);
                return rw_action_result::success;
            }

            inline void auto_lock_shared() {
                if(this->lock_type_ == unlocked)
                    this->lock_shared();
                else if(this->lock_type_ == locked_exclusive)
                    this->downgrade_lock();
            }

            [[nodiscard]] inline std::cv_status auto_lock_shared_until(const std::chrono::system_clock::time_point& timeout) {
                if(this->lock_type_ == unlocked)
                    return this->lock_shared_until(timeout);
                else if(this->lock_type_ == locked_exclusive)
                    this->downgrade_lock_until();
                return std::cv_status::no_timeout;
            }

            void auto_unlock() {
                if(this->lock_type_ == locked_shared)
                    this->unlock_shared();
                else if(this->lock_type_ == locked_exclusive)
                    this->unlock_exclusive();
            }

            [[nodiscard]] inline auto mutex() { return this->lock_; }
        private:
            enum {
                unlocked,
                locked_shared,
                locked_exclusive
            } lock_type_;

            lock_t* lock_;
    };
}

/* the implementation */
namespace ts {
#if __cplusplus > 201703L and 0
    #define unlikely_annotation [[unlikely]]
#else
    #define unlikely_annotation
#endif

    template <typename options_t, typename action_validator_t>
#ifndef DEBUG_RW_MUTEX
    impl::rw_mutex_impl<options_t, action_validator_t>::~rw_mutex_impl() = default;
#else
    impl::rw_mutex_impl<options_t, action_validator_t>::~rw_mutex_impl() {
        rw_mutex_assert(!this->write_locked);
        rw_mutex_assert(!this->upgrade_pending);
        rw_mutex_assert(this->write_lock_pending == 0);
        rw_mutex_assert(this->read_lock_pending == 0);
        rw_mutex_assert(this->read_lock_count == 0);
    }
#endif

    template <typename options_t, typename action_validator_t>
    void impl::rw_mutex_impl<options_t, action_validator_t>::lock() {
        std::unique_lock slock{this->status_lock_};
        this->action_validator_.try_exclusive_lock_acquire();

        this->write_lock_pending++;
        acquire: {
            while (this->read_lock_count > 0) unlikely_annotation
                this->write_update_.wait(slock);

            while (this->write_locked) unlikely_annotation
                this->write_update_.wait(slock);

            /*
             * Even when write_preferred is enabled, another thread which wants to read lock claims the mutex
             * and read locks the lock before the write could be acquired.
             */
            if(this->read_lock_count > 0) /* this could be changed while waiting for the write lock to unlock */
                goto acquire;
        }

        this->write_locked = true;
        this->write_lock_pending--;
        this->action_validator_.exclusive_lock_acquire();
    }

    template <typename options_t, typename action_validator_t>
    std::cv_status impl::rw_mutex_impl<options_t, action_validator_t>::lock_until(const std::chrono::system_clock::time_point& timeout) {
        std::unique_lock slock{this->status_lock_};
        this->action_validator_.try_exclusive_lock_acquire();
        this->write_lock_pending++;
        acquire: {
            while(this->read_lock_count > 0) unlikely_annotation
                if(auto err = this->write_update_.wait_until(slock, timeout); err != std::cv_status::no_timeout) {
                    assert(this->write_lock_pending > 0);
                    this->write_lock_pending--;
                    return std::cv_status::timeout;
                }

            while(this->write_locked) unlikely_annotation
                if(auto err = this->write_update_.wait_until(slock, timeout); err != std::cv_status::no_timeout) {
                    assert(this->write_lock_pending > 0);
                    this->write_lock_pending--;
                    return std::cv_status::timeout;
                }

            /*
             * Even when write_preferred is enabled, another thread which wants to read lock claims the mutex
             * and read locks the lock before the write could be acquired.
             */
            if(this->read_lock_count > 0) /* this could be changed while waiting for the write lock to unlock */
                goto acquire;
        }

        this->write_locked = true;
        this->write_lock_pending--;
        this->action_validator_.exclusive_lock_acquire();
    }

    template <typename options_t, typename action_validator_t>
    void impl::rw_mutex_impl<options_t, action_validator_t>::lock_shared() {
        std::unique_lock slock{this->status_lock_};
        this->action_validator_.try_shared_lock_acquire();
        this->read_lock_pending++;
        while(this->write_locked) unlikely_annotation
            this->read_update_.wait(slock);

        this->read_lock_count++;
        this->read_lock_pending--;
        this->action_validator_.shared_lock_acquire();
    }

    template <typename options_t, typename action_validator_t>
    std::cv_status impl::rw_mutex_impl<options_t, action_validator_t>::lock_shared_until(const std::chrono::system_clock::time_point& timeout) {
        std::unique_lock slock{this->status_lock_};
        this->action_validator_.try_shared_lock_acquire();
        this->read_lock_pending++;
        while(this->write_locked) unlikely_annotation
            if(auto err = this->read_update_.wait_until(slock, timeout); err != std::cv_status::no_timeout) {
                rw_mutex_assert(this->read_lock_pending > 0);
                this->read_lock_pending--;
                return std::cv_status::timeout;
            }

        this->read_lock_count++;
        this->read_lock_pending--;
        this->action_validator_.shared_lock_acquire();
    }

    template <typename options_t, typename action_validator_t>
    void impl::rw_mutex_impl<options_t, action_validator_t>::unlock() {
        std::lock_guard slock{this->status_lock_};
        this->action_validator_.try_exclusive_lock_release();
        rw_mutex_assert(this->write_locked);

        this->write_locked = false;
        if(this->write_lock_upgraded) unlikely_annotation {
            rw_mutex_assert(this->read_lock_count == 1); /* is was a upgraded lock */
            this->read_lock_count--;
            this->write_lock_upgraded = false;
        } else {
            rw_mutex_assert(this->read_lock_count == 0);
        }

        if(options_t::write_preferred) {
            if(this->write_lock_pending > 0)
                this->write_update_.notify_one();
            else if(this->read_lock_pending > 0)
                this->read_update_.notify_one();
        } else {
            if(this->read_lock_pending > 0)
                this->read_update_.notify_all();
            else if(this->write_lock_pending > 0)
                this->write_update_.notify_one(); /* only one thread could write at the time */
        }
        this->action_validator_.exclusive_lock_release();
    }

    template <typename options_t, typename action_validator_t>
    void impl::rw_mutex_impl<options_t, action_validator_t>::unlock_shared() {
        std::lock_guard slock{this->status_lock_};
        this->action_validator_.try_shared_lock_release();
        rw_mutex_assert(!this->write_locked);
        rw_mutex_assert(this->read_lock_count > 0);

        this->read_lock_count--;
        if(this->read_lock_count == 0) {
            if(this->write_lock_pending > 0) {
                /* notify all writers that we're ready to lock */
                rw_mutex_assert(!this->upgrade_pending);
                this->write_update_.notify_one(); /* only one thread could write at the time */
            }
        } else if(this->read_lock_count == 1) unlikely_annotation {
            if(this->upgrade_pending) {
                this->write_locked = true;
                this->upgrade_pending = false;
                this->upgrade_update_.notify_one();
            }
        }
        this->action_validator_.shared_lock_release();
    }

    template <typename options_t, typename action_validator_t>
    rw_action_result impl::rw_mutex_impl<options_t, action_validator_t>::upgrade_lock() {
        std::unique_lock slock{this->status_lock_};
        this->action_validator_.try_shared_lock_upgrade();
        rw_mutex_assert(!this->write_locked);
        rw_mutex_assert(this->read_lock_count > 0);

        if(this->upgrade_pending)
            return rw_action_result::would_deadlock;
        if(this->write_locked)
            return rw_action_result::resource_error;

        this->upgrade_pending = true;
        if(this->read_lock_count > 1) {
            while(this->read_lock_count > 1) unlikely_annotation
                this->upgrade_update_.wait(slock);

            /* whoever allowed us to upgrade should have done that already */
            rw_mutex_assert(this->write_locked);
            rw_mutex_assert(!this->upgrade_pending);
        }
        this->upgrade_pending = false;
        this->write_lock_upgraded = true;
        this->write_locked = true;

        this->action_validator_.shared_lock_upgrade();
        return rw_action_result::success;
    }

    template <typename options_t, typename action_validator_t>
    rw_action_result impl::rw_mutex_impl<options_t, action_validator_t>::upgrade_lock_until(const std::chrono::system_clock::time_point& timeout) {
        std::unique_lock slock{this->status_lock_};
        this->action_validator_.try_shared_lock_upgrade();
        rw_mutex_assert(!this->write_locked);
        rw_mutex_assert(this->read_lock_count > 0);

        if(this->upgrade_pending)
            return rw_action_result::would_deadlock;
        if(this->write_locked)
            return rw_action_result::resource_error;

        this->upgrade_pending = true;
        if(this->read_lock_count > 1) {
            while(this->read_lock_count > 1) unlikely_annotation
                if(auto err = this->upgrade_update_.wait_until(slock, timeout); err != std::cv_status::no_timeout) {
                    this->upgrade_pending = false;
                    return rw_action_result::timeout;
                }

            /* whoever allowed us to upgrade should have done that already */
            rw_mutex_assert(this->write_locked);
            rw_mutex_assert(!this->upgrade_pending);
        }
        this->upgrade_pending = false;
        this->write_lock_upgraded = true;
        this->write_locked = true;
        this->action_validator_.shared_lock_upgrade();

        return rw_action_result::success;
    }

    template <typename options_t, typename action_validator_t>
    void impl::rw_mutex_impl<options_t, action_validator_t>::downgrade_lock() {
        std::unique_lock slock{this->status_lock_};
        this->action_validator_.try_exclusive_lock_downgrade();
        rw_mutex_assert(this->write_locked);

        if(this->write_lock_upgraded) {
            rw_mutex_assert(this->read_lock_count == 1); /* our own thread */
        } else unlikely_annotation {
            this->read_lock_count++;
        }

        this->write_lock_upgraded = false;
        this->write_locked = false;
        this->read_update_.notify_all();
        this->action_validator_.exclusive_lock_downgrade();
    }

    /* action validator */
    /* shared lock part */
    void impl::mutex_action_validator::try_shared_lock_acquire() {
        if(this->exclusive_lock == std::this_thread::get_id())
            throw std::logic_error{"mutex has been locked in exclusive lock by current thread"};
    }

    void impl::mutex_action_validator::shared_lock_acquire() {
        this->shared_lockers.push_back(std::this_thread::get_id());
    }

    void impl::mutex_action_validator::try_shared_lock_upgrade() {
        if(this->exclusive_lock == std::this_thread::get_id())
            throw std::logic_error{"mutex has been upgraded by current thread"};

        auto current_id = std::this_thread::get_id();
        auto count = std::count_if(this->shared_lockers.begin(), this->shared_lockers.end(), [&](const auto& id) {
            return id == current_id;
        });

        if(count == 0)
            throw std::logic_error{"upgrade not possible because shared mutex is not locked by current thread"};
        else if(count > 1)
            throw std::logic_error{"upgrade not possible because shared mutex is locked more than once by current thread"};
    }

    void impl::mutex_action_validator::shared_lock_upgrade() {
        this->exclusive_lock_upgraded = true;
        this->exclusive_lock = std::this_thread::get_id();
    }

    void impl::mutex_action_validator::try_shared_lock_release() {
        if(this->exclusive_lock == std::this_thread::get_id())
            throw std::logic_error{this->exclusive_lock_upgraded ? "mutex has been upgraded" : "mutex has been locked in exclusive mode, not in shared mode"};

        auto it = std::find(this->shared_lockers.begin(), this->shared_lockers.end(), std::this_thread::get_id());
        if(it == this->shared_lockers.end())
            throw std::logic_error{"mutex not locked by current thread"};
    }

    void impl::mutex_action_validator::shared_lock_release() {
        auto it = std::find(this->shared_lockers.begin(), this->shared_lockers.end(), std::this_thread::get_id());
        rw_mutex_assert(it != this->shared_lockers.end()); /* this should never happen (try_shared_lock_release has been called before) */
        this->shared_lockers.erase(it);
    }

    /* exclusive lock part */
    void impl::mutex_action_validator::try_exclusive_lock_acquire() {
        if(this->exclusive_lock == std::this_thread::get_id())
            throw std::logic_error{"mutex has been exclusive locked by current thread"};
    }

    void impl::mutex_action_validator::exclusive_lock_acquire() {
        this->exclusive_lock = std::this_thread::get_id();
        this->exclusive_lock_upgraded = false;
    }

    void impl::mutex_action_validator::try_exclusive_lock_downgrade() {
        if(this->exclusive_lock != std::this_thread::get_id())
            throw std::logic_error{"mutex hasn't been locked in exclusive mode by this thread"};
    }

    void impl::mutex_action_validator::exclusive_lock_downgrade() {
        if(!this->exclusive_lock_upgraded) {
            this->shared_lockers.push_back(std::this_thread::get_id());
        } else {
            /* internal state error check */
            rw_mutex_assert(std::find(this->shared_lockers.begin(), this->shared_lockers.end(), std::this_thread::get_id()) != this->shared_lockers.end());
        }

        this->exclusive_lock_upgraded = false;
        this->exclusive_lock = std::thread::id{};
    }

    void impl::mutex_action_validator::try_exclusive_lock_release() {
        if(this->exclusive_lock != std::this_thread::get_id())
            throw std::logic_error{"mutex hasn't been locked in exclusive mode by this thread"};
    }

    void impl::mutex_action_validator::exclusive_lock_release() {
        if(this->exclusive_lock_upgraded) {
            auto it = std::find(this->shared_lockers.begin(), this->shared_lockers.end(), std::this_thread::get_id());
            assert(it != this->shared_lockers.end());
            this->shared_lockers.erase(it);
        }
        this->exclusive_lock_upgraded = false;
        this->exclusive_lock = std::thread::id{};
    }
}
#undef rw_mutex_assert