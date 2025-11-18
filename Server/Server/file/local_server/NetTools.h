#pragma once

#include <cstddef>
#include <chrono>
#include <mutex>
#include <cassert>
#include <array>

#include <misc/spin_mutex.h>
#include <numeric>

namespace ts::server::file::networking {
    struct NetworkThrottle {
        constexpr static auto kThrottleTimespanMs{250};

        typedef uint8_t span_t;

        static NetworkThrottle kNoThrottle;

        ssize_t max_bytes{0};

        span_t current_index{0};
        size_t bytes_send{0};

        mutable spin_mutex mutex{};

        inline bool increase_bytes(size_t bytes) {
            auto current_ms = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
            auto current_span = (span_t) (current_ms / kThrottleTimespanMs);

            std::lock_guard slock{this->mutex};
            if(this->current_index != current_span) {
                this->current_index = current_span;
                this->bytes_send = bytes;
            } else {
                this->bytes_send += bytes;
            }
            return this->max_bytes > 0 && this->bytes_send >= this->max_bytes;
        }

        inline void set_max_bandwidth(ssize_t bytes_per_second) {
            std::lock_guard slock{this->mutex};
            if(bytes_per_second <= 0)
                this->max_bytes = -1;
            else
                this->max_bytes = bytes_per_second * kThrottleTimespanMs / 1000;
        }

        [[nodiscard]] inline bool should_throttle(timeval& next_timestamp) {
            if(this->max_bytes <= 0) return false;

            auto current_ms = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
            auto current_span = (span_t) (current_ms / kThrottleTimespanMs);

            std::lock_guard slock{this->mutex};
            if(this->max_bytes <= 0) return false; /* we've to test here again, else out arithmetic will fail */
            if(this->current_index != current_span) return false;
            if(this->bytes_send < this->max_bytes) return false;

            next_timestamp.tv_usec = (kThrottleTimespanMs - current_ms % kThrottleTimespanMs) * 1000;
            next_timestamp.tv_sec =  next_timestamp.tv_usec / 1000000;
            next_timestamp.tv_usec -= next_timestamp.tv_sec * 1000000;
            return true;
        }

        [[nodiscard]] inline size_t bytes_left() const {
            if(this->max_bytes <= 0) return (size_t) -1;

            auto current_ms = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
            auto current_span = (span_t) (current_ms / kThrottleTimespanMs);

            std::lock_guard slock{this->mutex};
            if(this->max_bytes <= 0) return false; /* we've to test here again, else out arithmetic will fail */
            if(this->current_index != current_span) return this->max_bytes;
            if(this->bytes_send < this->max_bytes) return this->max_bytes - this->bytes_send;
            return 0;
        }

        [[nodiscard]] inline std::chrono::milliseconds expected_writing_time(size_t bytes) const {
            std::lock_guard slock{this->mutex};

            if(this->max_bytes <= 0) return std::chrono::milliseconds{0};
            return std::chrono::seconds{bytes / (this->max_bytes * (1000 / kThrottleTimespanMs))};
        }
    };

    struct DualNetworkThrottle {
        NetworkThrottle *left, *right;

        explicit DualNetworkThrottle(NetworkThrottle* left, NetworkThrottle* right) : left{left}, right{right} {
            assert(left);
            assert(right);
        }

        [[nodiscard]] inline size_t bytes_left() const {
            return std::min(this->left->bytes_left(), this->right->bytes_left());
        }

        [[nodiscard]] inline std::chrono::milliseconds expected_writing_time(size_t bytes) const {
            return std::max(this->left->expected_writing_time(bytes), this->right->expected_writing_time(bytes));
        }

        [[nodiscard]] inline bool should_throttle(timeval& next_timestamp) const {
            bool throttle = false;
            timeval right_timestamp{};
            throttle |= this->left->should_throttle(next_timestamp);
            throttle |= this->right->should_throttle(right_timestamp);
            if(!throttle) return false;

            if(right_timestamp.tv_sec > next_timestamp.tv_sec || (right_timestamp.tv_sec == next_timestamp.tv_sec && right_timestamp.tv_usec > next_timestamp.tv_usec)) {
                next_timestamp = right_timestamp;
            }
            return true;
        }

        inline bool increase_bytes(size_t bytes) {
            bool result = false;
            result |= this->left->increase_bytes(bytes);
            result |= this->right->increase_bytes(bytes);
            return result;
        }
    };

    struct TransferStatistics {
        constexpr static auto kMeasureTimespanMs{1000};
        constexpr static auto kAverageTimeCount{60};
        typedef uint8_t span_t;

        size_t total_bytes{0};
        size_t delta_bytes{0}; /* used for statistics propagation */

        span_t span_index{0};
        size_t span_bytes{0};
        std::array<size_t, kAverageTimeCount> history{};

        spin_mutex mutex{};

        inline void increase_bytes(size_t bytes) {
            auto current_ms = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
            auto current_span = (span_t) (current_ms / kMeasureTimespanMs);

            std::lock_guard slock{this->mutex};
            this->total_bytes += bytes;

            if(this->span_index != current_span) {
                this->history[this->span_index % kAverageTimeCount] = std::exchange(this->span_bytes, 0);
            }

            this->span_index = current_span;
            this->span_bytes += bytes;
        }

        [[nodiscard]] inline size_t take_delta() {
            std::lock_guard slock{this->mutex};
            assert(this->delta_bytes <= this->total_bytes);
            auto delta = this->total_bytes - this->delta_bytes;
            this->delta_bytes = this->total_bytes;
            return delta;
        }

        [[nodiscard]] inline double current_bandwidth() const {
            return (this->history[(this->span_index - 1) % kAverageTimeCount] * (double) 1000) / (double) kMeasureTimespanMs;
        }

        [[nodiscard]] inline double average_bandwidth() const {
            return (std::accumulate(this->history.begin(), this->history.end(), 0UL) * (double) 1000) / (double) (kMeasureTimespanMs * kAverageTimeCount);
        }
    };
}