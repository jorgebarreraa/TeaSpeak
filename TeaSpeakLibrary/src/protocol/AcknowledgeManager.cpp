#include "AcknowledgeManager.h"
#include <cmath>
#include <misc/endianness.h>
#include <algorithm>

using namespace ts;
using namespace ts::connection;
using namespace ts::protocol;
using namespace std;
using namespace std::chrono;

AcknowledgeManager::AcknowledgeManager() = default;

AcknowledgeManager::~AcknowledgeManager() {
    this->reset();
}

void AcknowledgeManager::reset() {
    {
        std::unique_lock lock{this->entry_lock};
        auto pending_entries = std::move(this->entries);
        lock.unlock();

        /* save because entries are not accessable anymore */
        for(const auto& entry : pending_entries)
            if(entry->acknowledge_listener)
                entry->acknowledge_listener->executionFailed("reset");
    }
}

size_t AcknowledgeManager::awaiting_acknowledge() {
    std::lock_guard lock(this->entry_lock);
    return this->entries.size();
}

void AcknowledgeManager::process_packet(ts::protocol::BasicPacket &packet) {
    if(!packet.type().requireAcknowledge()) return;

    auto entry = make_shared<Entry>();
    entry->acknowledge_listener = std::move(packet.getListener());

    entry->buffer = packet.buffer();

    entry->resend_count = 0;
    entry->first_send = system_clock::now();
    entry->next_resend = entry->first_send + std::chrono::milliseconds{(int64_t) ceil(this->rto)};

    entry->packet_type = packet.type().type();
    entry->packet_id = packet.packetId();
    entry->generation_id = packet.generationId();

    entry->acknowledged = false;
    entry->send_count = 1;
    {
        std::lock_guard lock(this->entry_lock);
        this->entries.push_front(std::move(entry));
    }
}

bool AcknowledgeManager::process_acknowledge(uint8_t packet_type, uint16_t target_id, std::string& error) {
    PacketType target_type{packet_type == protocol::ACK_LOW ? PacketType::COMMAND_LOW : PacketType::COMMAND};

    std::shared_ptr<Entry> entry;
    std::unique_ptr<threads::Future<bool>> ack_listener;
    {
        std::lock_guard lock{this->entry_lock};
        for(auto it = this->entries.begin(); it != this->entries.end(); it++) {
            if((*it)->packet_type == target_type && (*it)->packet_id == target_id) {
                entry = *it;
                ack_listener = std::move(entry->acknowledge_listener); /* move it out so nobody else could call it as well */

                entry->send_count--;
                if(entry->send_count == 0) {
                    this->entries.erase(it);
                    if(entry->resend_count == 0) {
                        auto difference = std::chrono::system_clock::now() - entry->first_send;
                        this->update_rto(std::chrono::duration_cast<std::chrono::milliseconds>(difference).count());
                    }
                }
                break;
            }
        }
    }
    if(!entry) {
        error = "Missing packet id (" + to_string(target_id) + ")";
        return false;
    }

    entry->acknowledged = true;
    if(ack_listener) ack_listener->executionSucceed(true);
    return true;
}

ssize_t AcknowledgeManager::execute_resend(const system_clock::time_point& now , std::chrono::system_clock::time_point &next_resend,std::deque<std::shared_ptr<Entry>>& buffers, string& error) {
    size_t resend_count{0};

    vector<shared_ptr<Entry>> need_resend;
    {
        bool cleanup{false};
        std::lock_guard lock{this->entry_lock};
        need_resend.reserve(this->entries.size());

        for (auto &entry : this->entries) {
            if(entry->acknowledged) {
                if(entry->next_resend + std::chrono::milliseconds{(int64_t) ceil(this->rto * 4)} <= now) { // Some resends are lost. So we just drop it after time
                    entry.reset();
                    cleanup = true;
                }
            } else {
               if(entry->next_resend <= now) {
                   entry->next_resend = now + std::chrono::milliseconds{(int64_t) std::min(ceil(this->rto), 1500.f)};
                   need_resend.push_back(entry);
                   entry->resend_count++;
                   entry->send_count++;
               }
                if(next_resend > entry->next_resend)
                    next_resend = entry->next_resend;
            }
        }

        if(cleanup) {
            this->entries.erase(std::remove_if(this->entries.begin(), this->entries.end(),
                    [](const auto& entry) { return !entry; }), this->entries.end());
        }
    }

    for(const auto& packet : need_resend) {
        if(packet->resend_count > 15 && packet->first_send + seconds(15) < now) { //FIXME configurable
            error = "Failed to receive acknowledge for packet " + to_string(packet->packet_id) + " of type " + PacketTypeInfo::fromid(packet->packet_type).name();
            return -1;
        }

        resend_count++;
        buffers.push_back(packet);
    }

    return resend_count;
}

/* we're not taking the clock granularity into account because its nearly 1ms and it would only add more branches  */
void AcknowledgeManager::update_rto(size_t r) {
    if(srtt == -1) {
        this->srtt = (float) r;
        this->rttvar = r / 2.f;
        this->rto = srtt + 4 * this->rttvar;
    } else {
        this->rttvar = (1.f - alpha) * this->rttvar + beta * abs(this->srtt - r);
        this->srtt = (1.f - alpha) * srtt + alpha * r;
        this->rto = std::max(200.f, this->srtt + 4 * this->rttvar);
    }
}