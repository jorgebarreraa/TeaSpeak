#pragma once

#include <cstdint>
#include <chrono>
#include <atomic>

namespace ts {
    namespace server {
        namespace server {
            namespace udp {
                /**
                 * Handles ping/pong protocol for connection health monitoring
                 */
                class PingHandler {
                    public:
                        // Callback types
                        typedef void(*callback_send_ping_t)(void* /* user data */, uint16_t& /* ping id */);
                        typedef void(*callback_send_recovery_t)(void* /* user data */);
                        typedef void(*callback_timeout_t)(void* /* user data */);

                        PingHandler() = default;
                        ~PingHandler() = default;

                        /**
                         * Reset the ping handler state
                         */
                        void reset() {
                            this->last_ping_id_ = 0;
                            this->last_pong_received_ = std::chrono::system_clock::now();
                            this->last_command_ack_ = std::chrono::system_clock::now();
                            this->pending_ping_ = false;
                        }

                        /**
                         * Called when a pong packet is received
                         * @param ping_id The ping ID from the pong packet
                         */
                        void received_pong(uint16_t ping_id) {
                            if(ping_id == this->last_ping_id_) {
                                this->last_pong_received_ = std::chrono::system_clock::now();
                                this->pending_ping_ = false;
                            }
                        }

                        /**
                         * Called when a command acknowledgment is received
                         * Used to track connection activity
                         */
                        void received_command_acknowledged() {
                            this->last_command_ack_ = std::chrono::system_clock::now();
                        }

                        /**
                         * Process ping timeout checking and send pings as needed
                         * Should be called periodically
                         */
                        void process_ping_timeout(const std::chrono::system_clock::time_point& now) {
                            // Check if we need to send a ping
                            auto time_since_pong = now - this->last_pong_received_;
                            auto time_since_ack = now - this->last_command_ack_;

                            // If no activity for 10 seconds, send ping
                            if(!this->pending_ping_ &&
                               time_since_pong > std::chrono::seconds(10) &&
                               time_since_ack > std::chrono::seconds(10)) {
                                if(this->callback_send_ping) {
                                    this->callback_send_ping(this->callback_argument, this->last_ping_id_);
                                    this->pending_ping_ = true;
                                }
                            }

                            // If ping is pending for too long, try recovery
                            if(this->pending_ping_ && time_since_pong > std::chrono::seconds(30)) {
                                if(this->callback_send_recovery_command) {
                                    this->callback_send_recovery_command(this->callback_argument);
                                }

                                // If still no response after 60 seconds, timeout
                                if(time_since_pong > std::chrono::seconds(60)) {
                                    if(this->callback_time_outed) {
                                        this->callback_time_outed(this->callback_argument);
                                    }
                                }
                            }
                        }

                        // Callback pointers (must be set by user)
                        void* callback_argument{nullptr};
                        callback_send_ping_t callback_send_ping{nullptr};
                        callback_send_recovery_t callback_send_recovery_command{nullptr};
                        callback_timeout_t callback_time_outed{nullptr};

                    private:
                        uint16_t last_ping_id_{0};
                        std::chrono::system_clock::time_point last_pong_received_{std::chrono::system_clock::now()};
                        std::chrono::system_clock::time_point last_command_ack_{std::chrono::system_clock::now()};
                        bool pending_ping_{false};
                };
            }
        }
    }
}
