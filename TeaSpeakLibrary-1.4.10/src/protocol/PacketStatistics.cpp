//
// Created by WolverinDEV on 06/04/2020.
//

#include <mutex>
#include "./PacketStatistics.h"

using namespace ts::protocol;

void PacketStatistics::received_packet(ts::protocol::PacketType type, uint32_t pid) {
    std::lock_guard lock{this->data_mutex};
    switch (type) {
        case protocol::PacketType::VOICE:
            this->calculator_voice.packet_received(pid);
            return;
        case protocol::PacketType::VOICE_WHISPER:
            this->calculator_voice_whisper.packet_received(pid);
            return;

        case protocol::PacketType::COMMAND:
        case protocol::PacketType::COMMAND_LOW:
            return;

        case protocol::PacketType::ACK:
            this->calculator_ack.packet_received(pid);
            return;
        case protocol::PacketType::ACK_LOW:
            this->calculator_ack_low.packet_received(pid);
            return;
        case protocol::PacketType::PING:
            this->calculator_ping.packet_received(pid);
            return;

        default:
            /* some invalid packet lul */
            return;
    }
}

void PacketStatistics::send_command(ts::protocol::PacketType type, uint32_t pid) {
    std::lock_guard lock{this->data_mutex};
    if(type == protocol::PacketType::COMMAND)
        this->calculator_command.packet_send(pid);
    else if(type == protocol::PacketType::COMMAND_LOW)
        this->calculator_command_low.packet_send(pid);
}

void PacketStatistics::received_acknowledge(ts::protocol::PacketType type, uint32_t pid) {
    std::lock_guard lock{this->data_mutex};
    if(type == protocol::PacketType::ACK)
        this->calculator_command.ack_received(pid);
    else if(type == protocol::PacketType::ACK_LOW)
        this->calculator_command_low.ack_received(pid);
}

PacketStatistics::PacketLossReport PacketStatistics::loss_report() const {
    PacketStatistics::PacketLossReport result{};

    result.received_voice = this->calculator_voice.received_packets() +  this->calculator_voice_whisper.received_packets();
    result.lost_voice = this->calculator_voice.lost_packets() +  this->calculator_voice_whisper.lost_packets();

    result.received_keep_alive = this->calculator_ping.received_packets();
    result.lost_keep_alive = this->calculator_ping.lost_packets();

    result.received_control = this->calculator_command.received_packets() + this->calculator_command_low.received_packets();
    result.lost_control = this->calculator_command.lost_packets() + this->calculator_command_low.lost_packets();
    //result.lost_control -= this->calculator_ack.lost_packets() + this->calculator_ack_low.lost_packets(); /* subtract the lost acks (command received but ack got lost) */

    result.received_control += this->calculator_ack.received_packets() + this->calculator_ack_low.received_packets();
    //result.lost_control += this->calculator_ack.lost_packets() + this->calculator_ack_low.lost_packets(); /* this cancels out the line above */
    return result;
}

void PacketStatistics::tick() {
    auto now = std::chrono::system_clock::now();
    if(now + std::chrono::seconds{15} > this->last_short) {
        this->last_short = now;

        std::lock_guard lock{this->data_mutex};
        this->calculator_command.short_stats();
        this->calculator_command_low.short_stats();

        this->calculator_ack.short_stats();
        this->calculator_ack_low.short_stats();

        this->calculator_voice.short_stats();
        this->calculator_voice_whisper.short_stats();

        this->calculator_ping.short_stats();
    }
}

void PacketStatistics::reset() {
    std::lock_guard lock{this->data_mutex};
    this->calculator_command.reset();
    this->calculator_command_low.reset();

    this->calculator_ack.reset();
    this->calculator_ack_low.reset();

    this->calculator_voice.reset();
    this->calculator_voice_whisper.reset();

    this->calculator_ping.reset();
}

void PacketStatistics::reset_offsets() {
    std::lock_guard lock{this->data_mutex};
    this->calculator_command.reset_offsets();
    this->calculator_command_low.reset_offsets();

    this->calculator_ack.reset_offsets();
    this->calculator_ack_low.reset_offsets();

    this->calculator_voice.reset_offsets();
    this->calculator_voice_whisper.reset_offsets();

    this->calculator_ping.reset_offsets();
}

float PacketStatistics::current_packet_loss() const {
    auto report = this->loss_report();
    return report.total_loss();
}