//
// Created by WolverinDEV on 06/04/2020.
//

#include <cassert>
#include <utility>
#include <cstring>
#include "PacketLossCalculator.h"

using namespace ts::protocol;

void UnorderedPacketLossCalculator::packet_received(uint32_t packet_id) {
    if(packet_id > this->packet_history_offset) {
        const auto age = packet_id - this->packet_history_offset; /* best case should be 1 */

        if(age < this->packet_history.max_bits()) {
            const auto received = this->packet_history.shift_in_bounds(age).count();
            this->received_packets_ += (uint32_t) received;
            this->received_packets_total_ += (uint32_t) received;

            this->lost_packets_ += (uint32_t) (age - received);
            this->lost_packets_total_ += (uint32_t) (age - received);
        } else {
            const auto received = this->packet_history.clear().count();
            this->received_packets_ += (uint32_t) received;
            this->received_packets_total_ += (uint32_t) received;
            this->lost_packets_ += (uint32_t) (this->packet_history.max_bits() - received);
            this->lost_packets_total_ += (uint32_t) (this->packet_history.max_bits() - received);

            if(age >= this->packet_history.max_bits() * 2) {
                this->packet_history.set_unchecked(0);
                this->lost_packets_ += (uint32_t) (age - this->packet_history.max_bits() * 2);
                this->lost_packets_total_ += (uint32_t) (age - this->packet_history.max_bits() * 2);
            } else {
                this->packet_history.set_unchecked(age - this->packet_history.max_bits());
            }
        }
        this->packet_history.set_unchecked(0);
        this->packet_history_offset = packet_id;
        if(packet_id < this->packet_history.max_bits()) {
            this->received_packets_ = 0;
            this->received_packets_total_ = 0;
            this->lost_packets_ = 0;
            this->lost_packets_total_ = 0;
        }
    } else {
        /* unordered packet */
        const auto age = this->packet_history_offset - packet_id;
        if(age >= this->packet_history.max_bits())
            return; /* well that packet is way too old */

        this->packet_history.set_unchecked(age);
    }
}

void UnorderedPacketLossCalculator::short_stats() {
    constexpr auto target_interval = 32;

    const auto packets_passed = this->packet_history_offset - this->last_history_offset;
    if(packets_passed < target_interval) return;

    const auto factor = .5;
    this->received_packets_ = (uint32_t) (this->received_packets_ * factor);
    this->lost_packets_ = (uint32_t) (this->lost_packets_ * factor);
    this->last_history_offset = this->packet_history_offset;
}

void UnorderedPacketLossCalculator::reset() {
    this->received_packets_ = 0;
    this->received_packets_total_ = 0;
    this->lost_packets_ = 0;
    this->lost_packets_total_ = 0;
    this->reset_offsets();
}
void UnorderedPacketLossCalculator::reset_offsets() {
    this->packet_history_offset = 0;
    this->last_history_offset = 0;
    this->packet_history.clear();
}

void CommandPacketLossCalculator::packet_send(uint32_t packet_id) {
    if(packet_id > this->packet_history_offset) {
        assert(packet_id - 1 == this->packet_history_offset || this->packet_history_offset == 0); /* the method will only be called with an incrementing packet id */
        /* newly send packet */
        auto lost = std::exchange(this->packet_ack_counts[packet_id % CommandPacketLossCalculator::packet_ack_counts_length], 1);
        this->lost_packets_ += lost;
        this->lost_packets_total_ += lost;
        this->packet_history_offset = packet_id;
    } else {
        /* We're not really interested if the packet id matches the resend packet. If not we may accidentally increase the loss count. */
        this->packet_ack_counts[packet_id % CommandPacketLossCalculator::packet_ack_counts_length]++;
    }
    this->packets_send_unshorten++;
}

void CommandPacketLossCalculator::ack_received(uint32_t packet_id) {
    auto& count = this->packet_ack_counts[packet_id % CommandPacketLossCalculator::packet_ack_counts_length];
    if(count > 0) /* could happen if receive an acknowledge for an packet which is older than out buffer size or the client send the ack twice... */
        count--;

    this->received_packets_++;
    this->received_packets_total_++;
}

void CommandPacketLossCalculator::short_stats() {
    constexpr auto target_interval = 64;

    const auto packets_passed = this->packets_send_unshorten;
    if(packets_passed < target_interval) return;
    this->packets_send_unshorten = 0;

    const auto factor = target_interval / packets_passed;
    this->received_packets_ *= factor;
    this->lost_packets_ *= factor;
}

void CommandPacketLossCalculator::reset() {
    this->received_packets_ = 0;
    this->received_packets_total_ = 0;
    this->lost_packets_ = 0;
    this->lost_packets_total_ = 0;
    this->reset_offsets();
}

void CommandPacketLossCalculator::reset_offsets() {
    this->packet_history_offset = 0;
    this->packets_send_unshorten = 0;
    memset(packet_ack_counts, 0, packet_ack_counts_length);
}