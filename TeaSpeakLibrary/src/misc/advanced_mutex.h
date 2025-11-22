#pragma once

#include <cassert>
#include <thread>
#include <mutex>
#include <shared_mutex>
#include <atomic>
#include <map>

namespace std {
    template<class T, class Lock>
    struct lock_guarded {
        Lock l;
        T *t;

        T *operator->() &&{ return t; }

        template<class Arg>
        auto operator[](Arg &&arg) && -> decltype(std::declval<T &>()[std::declval<Arg>()]) {
            return (*t)[std::forward<Arg>(arg)];
        }

        T &operator*() &&{ return *t; }
    };

    template<class T, class Lock>
    struct lock_guarded_shared {
        Lock l;
        std::shared_ptr<T> t;

        T *operator->() &&{ return t.operator->(); }

        template<class Arg>
        auto operator[](Arg &&arg) && -> decltype(std::declval<T &>()[std::declval<Arg>()]) {
            return (*t)[std::forward<Arg>(arg)];
        }

        T &operator*() &&{ return *t; }

        operator bool() {
            return !!t;
        }

        bool operator !() {
            return !t;
        }
    };

    constexpr struct emplace_t { } emplace{};

    template<class T, class M>
    struct observer_locked {
        public:
            observer_locked(observer_locked &&o) : t(std::move(o.t)), m(std::move(o.m)) {}
            observer_locked(observer_locked const &o) : t(o.t), m(o.m) {}

            observer_locked(M lock,T entry) : m(std::forward<M>(lock)), t(std::forward<T>(entry)) {}

            observer_locked() = default;
            ~observer_locked() = default;

            T operator->() {
                return t;
            }

            T const operator->() const {
                return t;
            }

            T get() { return this->t; }
            T const get() const { return this->t; }

            template<class F>
            std::result_of_t<F(T &)> operator->*(F &&f) {
                return std::forward<F>(f)(t);
            }

            template<class F>
            std::result_of_t<F(T const &)> operator->*(F &&f) const {
                return std::forward<F>(f)(t);
            }

            observer_locked &operator=(observer_locked &&o) {
                this->m = std::move(o.m);
                this->t = std::move(o.t);
                return *this;
            }

            observer_locked &operator=(observer_locked const &o) {
                this->m = o.m;
                this->t = o.t;
                return *this;
            }

            observer_locked &reset() {
                observer_locked empty(M(),NULL);
                *this = empty;
                return *this;
            }
        private:
            M m;
            T t;
    };

    template<class T>
    struct mutex_guarded {
            lock_guarded<T, std::unique_lock<std::mutex>> get_locked() {
                return {std::unique_lock{m}, &t};
            }

            lock_guarded<T const, std::unique_lock<std::mutex>> get_locked() const {
                return {{m}, &t};
            }

            lock_guarded<T, std::unique_lock<std::mutex>> operator->() {
                return get_locked();
            }

            lock_guarded<T const, std::unique_lock<std::mutex>> operator->() const {
                return get_locked();
            }

            template<class F>
            std::result_of_t<F(T &)> operator->*(F &&f) {
                return std::forward<F>(f)(*get_locked());
            }

            template<class F>
            std::result_of_t<F(T const &)> operator->*(F &&f) const {
                return std::forward<F>(f)(*get_locked());
            }

            template<class...Args>
            mutex_guarded(emplace_t, Args &&...args) : t(std::forward<Args>(args)...) {}

            mutex_guarded(mutex_guarded &&o) : t(std::move(*o.get_locked())) {}

            mutex_guarded(mutex_guarded const &o) : t(*o.get_locked()) {}

            mutex_guarded() = default;

            ~mutex_guarded() = default;

            mutex_guarded &operator=(mutex_guarded &&o) {
                T tmp = std::move(o.get_locked());
                *get_locked() = std::move(tmp);
                return *this;
            }

            mutex_guarded &operator=(mutex_guarded const &o) {
                T tmp = o.get_locked();
                *get_locked() = std::move(tmp);
                return *this;
            }

        private:
            std::mutex m;
            T t;
    };

    /*
     * ATTENTION: Not sure how/why, but this causes a double rwlock unlock sometimes (when used with std::unique_lock)
     * Do not use this!
    class shared_recursive_mutex {
            std::shared_mutex handle;
        public:
            void lock(void) {
                std::thread::id this_id = std::this_thread::get_id();
                if (owner == this_id) {
                    // recursive locking
                    ++count;
                } else {
                    // normal locking
                    if (shared_counts->count(this_id)) {//Already shared locked, write lock is not available
#ifdef WIN32
                        throw std::logic_error("resource_deadlock_would_occur");
#else
                        __throw_system_error(int(errc::resource_deadlock_would_occur));
#endif
                    }
                    handle.lock(); //Now wait until everyone else has finished
                    owner = this_id;
                    count = 1;
                }
            }

            void unlock(void) {
                std::thread::id this_id = std::this_thread::get_id();
                assert(this_id == this->owner);

                if (count > 1) {
                    // recursive unlocking
                    count--;
                } else {
                    // normal unlocking
                    owner = this_id;
                    count = 0;
                    handle.unlock();
                }
            }

            void lock_shared() {
                std::thread::id this_id = std::this_thread::get_id();
                if(this->owner == this_id) {
#ifdef WIN32
                    throw std::logic_error("resource_deadlock_would_occur");
#else
                    __throw_system_error(int(errc::resource_deadlock_would_occur));
#endif
                }

                if (shared_counts->count(this_id)) {
                    ++(shared_counts.get_locked()[this_id]);
                } else {
                    handle.lock_shared();
                    shared_counts.get_locked()[this_id] = 1;
                }
            }

            void unlock_shared() {
                std::thread::id this_id = std::this_thread::get_id();
                auto it = shared_counts->find(this_id);
                if (it->second > 1) {
                    --(it->second);
                } else {
                    shared_counts->erase(it);
                    handle.unlock_shared();
                }
            }

            bool try_lock() {
                std::thread::id this_id = std::this_thread::get_id();
                if (owner == this_id) {
                    // recursive locking
                    ++count;
                    return true;
                } else {
                    // normal locking
                    if (shared_counts->count(this_id)){ //Already shared locked, write lock is not available
#ifdef WIN32
                        throw std::logic_error("resource_deadlock_would_occur");
#else
                        __throw_system_error(int(errc::resource_deadlock_would_occur));
#endif
                    }

                    if(!handle.try_lock()) return false;

                    owner = this_id;
                    count = 1;
                    return true;
                }
            }

            bool try_lock_shared() {
                std::thread::id this_id = std::this_thread::get_id();
                if(this->owner == this_id){
#ifdef WIN32
                    throw std::logic_error("resource_deadlock_would_occur");
#else
                    __throw_system_error(int(errc::resource_deadlock_would_occur));
#endif
                }

                if (shared_counts->count(this_id)) {
                    ++(shared_counts.get_locked()[this_id]);
                } else {
                    if(!handle.try_lock_shared()) return false;

                    shared_counts.get_locked()[this_id] = 1;
                }
                return true;
            }

        private:
            std::atomic<std::thread::id> owner;
            std::atomic<std::size_t> count;
            mutex_guarded<std::map<std::thread::id, std::size_t>> shared_counts;
    };
     */

    /**
     * Test if a `std::shared_mutex` is unique locked by trying to lock it in shared mode.
     * @tparam T
     * @param mutex
     * @return
     */
    template <typename T>
    inline bool mutex_locked(T& mutex) {
        /* std::shared_mutex can be recursively unique locked?? */
        return true;
        try {
            std::unique_lock<T> lock_try(mutex, try_to_lock); /* should throw EDEADLK */
            return false;
        } catch(const std::system_error& ex) {
            return ex.code() == errc::resource_deadlock_would_occur;
        }
    }

    /**
     * Test if a `std::shared_mutex` is shared (or unique) locked by try locking it in unique mode
     * @tparam T
     * @param mutex
     * @return
     */
    template <typename T>
    inline bool mutex_shared_locked(T& mutex) {
        return true;
        //return mutex_locked(mutex);
    }
}