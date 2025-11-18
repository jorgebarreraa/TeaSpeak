#pragma once

namespace ts::server::file {
    enum struct ExecuteStatus {
        UNKNOWN,
        WAITING,
        SUCCESS,
        ERROR
    };

    template<typename VariantType, typename T, std::size_t index = 0>
    constexpr std::size_t variant_index() {
        if constexpr (index == std::variant_size_v<VariantType>) {
            return index;
        } else if constexpr (std::is_same_v<std::variant_alternative_t<index, VariantType>, T>) {
            return index;
        } else {
            return variant_index<VariantType, T, index + 1>();
        }
    }

    struct EmptyExecuteResponse { };
    template <class error_t, class response_t = EmptyExecuteResponse>
    class ExecuteResponse {
            typedef std::variant<EmptyExecuteResponse, error_t, response_t> variant_t;
        public:
            ExecuteStatus status{ExecuteStatus::WAITING};

            [[nodiscard]] inline auto response() const -> const response_t& { return std::get<response_t>(this->response_); }

            template <typename = std::enable_if_t<!std::is_void<error_t>::value>>
            [[nodiscard]] inline const error_t& error() const { return std::get<error_t>(this->response_); }

            inline void wait() const {
                std::unique_lock nlock{this->notify_mutex};
                this->notify_cv.wait(nlock, [&]{ return this->status != ExecuteStatus::WAITING; });
            }

            template<typename _Rep, typename _Period>
            [[nodiscard]] inline bool wait_for(const std::chrono::duration<_Rep, _Period>& time) const {
                std::unique_lock nlock{this->notify_mutex};
                return this->notify_cv.wait_for(nlock, time, [&]{ return this->status != ExecuteStatus::WAITING; });
            }

            template <typename... Args>
            inline void emplace_success(Args&&... args) {
                constexpr auto success_index = variant_index<variant_t, response_t>();

                std::lock_guard rlock{this->notify_mutex};
                this->response_.template emplace<success_index, Args...>(std::forward<Args>(args)...);
                this->status = ExecuteStatus::SUCCESS;
                this->notify_cv.notify_all();
            }

            template <typename... Args>
            inline void emplace_fail(Args&&... args) {
                constexpr auto error_index = variant_index<variant_t, error_t>();

                std::lock_guard rlock{this->notify_mutex};
                this->response_.template emplace<error_index, Args...>(std::forward<Args>(args)...);
                this->status = ExecuteStatus::ERROR;
                this->notify_cv.notify_all();
            }

            [[nodiscard]] inline bool succeeded() const {
                return this->status == ExecuteStatus::SUCCESS;
            }

            ExecuteResponse(std::mutex& notify_mutex, std::condition_variable& notify_cv)
                    : notify_mutex{notify_mutex}, notify_cv{notify_cv} {}
        private:
            variant_t response_{}; /* void* as default value so we don't initialize error_t or response_t */

            std::mutex& notify_mutex;
            std::condition_variable& notify_cv;
    };
}