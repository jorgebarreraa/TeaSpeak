//
// Created by WolverinDEV on 11/03/2020.
//

#include "./PingHandler.h"

using namespace ts::server::server::udp;

void PingHandler::reset() {
    this->last_ping_id = 0;
    this->current_ping_ = std::chrono::milliseconds{0};

    this->last_recovery_command_send = std::chrono::system_clock::time_point{};
    this->last_command_acknowledge_ = std::chrono::system_clock::time_point{};

    this->last_response_ = std::chrono::system_clock::time_point{};
    this->last_request_ = std::chrono::system_clock::time_point{};
}

void PingHandler::received_pong(uint16_t ping_id) {
    if(this->last_ping_id != ping_id) return;

    auto now = std::chrono::system_clock::now();
    this->current_ping_ = std::chrono::floor<std::chrono::milliseconds>(now - this->last_request_);

    this->last_response_ = now;
    this->last_command_acknowledge_ = now; /* That's here for purpose!*/
}

void PingHandler::received_command_acknowledged() {
    this->last_command_acknowledge_ = std::chrono::system_clock::now();
}

void PingHandler::tick(const std::chrono::system_clock::time_point& now) {
    if(this->last_request_ + PingHandler::kPingRequestInterval < now) {
        this->send_ping_request(); /* may update last_response_ */
    }

    if(this->last_response_ + PingHandler::kPingTimeout < now) {
        if(this->last_recovery_command_send + PingHandler::kRecoveryRequestInterval < now) {
            this->send_recovery_request();
        }

        if(this->last_command_acknowledge_ + PingHandler::kRecoveryTimeout < now) {
            if(auto callback{this->callback_time_outed}; callback) {
                callback(this->callback_argument);
            }
        }
    }
}

void PingHandler::send_ping_request() {
    auto now = std::chrono::system_clock::now();
    if(this->last_response_.time_since_epoch().count() == 0) {
        this->last_response_ = now;
    }

    this->last_request_ = now;

    if(auto callback{this->callback_send_ping}; callback) {
        callback(this->callback_argument, this->last_ping_id);
    }
}

void PingHandler::send_recovery_request() {
    auto now = std::chrono::system_clock::now();
    if(this->last_command_acknowledge_.time_since_epoch().count() == 0) {
        this->last_command_acknowledge_ = now;
    }

    this->last_recovery_command_send = now;

    if(auto callback{this->callback_send_recovery_command}; callback) {
        callback(this->callback_argument);
    }
}
