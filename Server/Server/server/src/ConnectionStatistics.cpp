//
// Created by wolverindev on 11.11.17.
//

#include <misc/memtracker.h>

#include <utility>
#include "ConnectionStatistics.h"

using namespace std;
using namespace std::chrono;
using namespace ts;
using namespace ts::server;
using namespace ts::stats;
using namespace ts::protocol;

ConnectionStatistics::ConnectionStatistics(shared_ptr<ConnectionStatistics> handle) : handle(std::move(handle)) {
    memtrack::allocated<ConnectionStatistics>(this);
}

ConnectionStatistics::~ConnectionStatistics() {
    memtrack::freed<ConnectionStatistics>(this);
}

void ConnectionStatistics::logIncomingPacket(const category::value &category, size_t size) {
    assert(category >= 0 && category <= 2);
    this->statistics_second_current.connection_bytes_received[category] += size;
    this->statistics_second_current.connection_packets_received[category] += 1;

    if(this->handle)
        this->handle->logIncomingPacket(category, size);
}

void ConnectionStatistics::logOutgoingPacket(const category::value &category, size_t size) {
    assert(category >= 0 && category <= 2);
    this->statistics_second_current.connection_bytes_sent[category] += size;
    this->statistics_second_current.connection_packets_sent[category] += 1;

    if(this->handle) {
        this->handle->logOutgoingPacket(category, size);
    }
}

/* file transfer */
void ConnectionStatistics::logFileTransferIn(uint32_t bytes) {
    this->statistics_second_current.file_bytes_received += bytes;
    this->file_bytes_received += bytes;

    if(this->handle)
        this->handle->logFileTransferIn(bytes);
}

void ConnectionStatistics::logFileTransferOut(uint32_t bytes) {
    this->statistics_second_current.file_bytes_sent += bytes;
    this->file_bytes_sent += bytes;

    if(this->handle)
        this->handle->logFileTransferOut(bytes);
}

void ConnectionStatistics::tick() {
    auto now = std::chrono::system_clock::now();
    auto time_difference = this->last_second_tick.time_since_epoch().count() > 0 ? now - this->last_second_tick : std::chrono::seconds{1};
    if(time_difference >= std::chrono::seconds{1}) {
        BandwidthEntry<uint32_t> current{};
        current.atomic_exchange(this->statistics_second_current);

        auto period_ms = std::chrono::floor<std::chrono::milliseconds>(time_difference).count();
        auto current_normalized = current.mul<long double>(1000.0 / period_ms);

        this->statistics_second = this->statistics_second.mul<long double>(.2) + current_normalized.mul<long double>(.8);
        this->total_statistics += current;

        auto current_second = std::chrono::floor<std::chrono::seconds>(now.time_since_epoch()).count();
        if(statistics_minute_offset == 0) {
            statistics_minute_offset = current_second;
        }

        /* fill all "lost" with the current bandwidth as well */
        while(statistics_minute_offset <= current_second) {
            this->statistics_minute[statistics_minute_offset++ % this->statistics_minute.size()] = current_normalized;
        }
        this->last_second_tick = now;
    }
}
BandwidthEntry<uint32_t> ConnectionStatistics::minute_stats() const {
    BandwidthEntry<uint32_t> result{};
    for(const auto& second : this->statistics_minute)
        result += second;
    return result.mul<uint32_t>(1. / (double) this->statistics_minute.size());
}

FileTransferStatistics ConnectionStatistics::file_stats() {
    FileTransferStatistics result{};

    result.bytes_received = this->file_bytes_received;
    result.bytes_sent = this->file_bytes_sent;

    return result;
}

std::pair<uint64_t, uint64_t> ConnectionStatistics::mark_file_bytes() {
    std::pair<uint64_t, uint64_t> result;

    {
        if(this->mark_file_bytes_received < this->file_bytes_received)
            result.second = this->file_bytes_received - this->mark_file_bytes_received;
        this->mark_file_bytes_received = (uint64_t) this->file_bytes_received;
    }

    {
        if(this->mark_file_bytes_sent < this->file_bytes_sent)
            result.first = this->file_bytes_sent - this->mark_file_bytes_sent;
        this->mark_file_bytes_sent = (uint64_t) this->file_bytes_sent;
    }

    return result;
}