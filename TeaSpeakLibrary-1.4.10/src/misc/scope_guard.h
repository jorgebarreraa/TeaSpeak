#pragma once

template <typename callback_t>
struct scope_exit_callback {
    public:
        scope_exit_callback(callback_t&& callback) : callback(std::forward<callback_t>(callback)) {}
        ~scope_exit_callback() {
            this->callback();
        }

    private:
        callback_t callback;
};