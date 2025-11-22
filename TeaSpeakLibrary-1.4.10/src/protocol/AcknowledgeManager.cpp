#include "AcknowledgeManager.h"
#include <cmath>
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
    this->rto_calculator_.reset();

    {
        std::unique_lock lock{this->entry_lock};
        auto pending_entries = std::move(this->entries);
        lock.unlock();

        /* save because entries are not accessible anymore */
        for(const auto& entry : pending_entries) {
            if(entry->acknowledge_listener) {
                (*entry->acknowledge_listener)(false);
            }
        }
    }
}

size_t AcknowledgeManager::awaiting_acknowledge() {
    std::lock_guard lock(this->entry_lock);
    return this->entries.size();
}

void AcknowledgeManager::process_packet(uint8_t type, uint32_t id, void *ptr, std::unique_ptr<std::function<void(bool)>> ack) {
    std::shared_ptr<Entry> entry{new Entry{}, [&](Entry* entry){
        assert(this->destroy_packet);
        this->destroy_packet(entry->packet_ptr);
        delete entry;
    }};
    entry->acknowledge_listener = std::move(ack);

    entry->packet_type = type;
    entry->packet_full_id = id;
    entry->packet_ptr = ptr;

    entry->resend_count = 0;
    entry->first_send = system_clock::now();
    entry->next_resend = entry->first_send + std::chrono::milliseconds{(int64_t) ceil(this->rto_calculator_.current_rto())};

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
    std::unique_ptr<std::function<void(bool)>> ack_listener;
    {
        std::lock_guard lock{this->entry_lock};
        for(auto it = this->entries.begin(); it != this->entries.end(); it++) {
            if((*it)->packet_type == target_type && (*it)->packet_full_id == target_id) {
                entry = *it;
                ack_listener = std::move(entry->acknowledge_listener); /* move it out so nobody else could call it as well */

                entry->send_count--;
                if(entry->send_count == 0) {
                    this->entries.erase(it);
                    if(entry->resend_count == 0) {
                        auto difference = std::chrono::system_clock::now() - entry->first_send;
                        this->rto_calculator_.update((float) std::chrono::duration_cast<std::chrono::milliseconds>(difference).count());
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
    if(ack_listener) {
        (*ack_listener)(true);
    }
    return true;
}

void AcknowledgeManager::execute_resend(const system_clock::time_point& now , std::chrono::system_clock::time_point &next_resend,std::deque<std::shared_ptr<Entry>>& buffers) {
    std::deque<std::shared_ptr<Entry>> resend_failed;

    {
        std::lock_guard lock{this->entry_lock};

        this->entries.erase(std::remove_if(this->entries.begin(), this->entries.end(), [&](std::shared_ptr<Entry>& entry) {
            if(entry->acknowledged) {
                if (entry->next_resend + std::chrono::milliseconds{(int64_t) ceil(this->rto_calculator_.current_rto() * 4)} <= now) {
                    /* Some resends are lost. So we just drop it after time */
                    return true;
                }
            } else {
                if (entry->next_resend <= now) {
                    if (entry->resend_count > 15 && entry->first_send + seconds(15) < now) {
                        /* packet resend seems to have failed */
                        resend_failed.push_back(std::move(entry));
                        return true;
                    } else {
                        entry->next_resend = now + std::chrono::milliseconds{(int64_t) std::min(ceil(this->rto_calculator_.current_rto()), 1500.f)};
                        buffers.push_back(entry);
                        //entry->resend_count++; /* this MUST be incremented by the result handler (resend may fails) */
                        entry->send_count++;
                    }
                }
                if (next_resend > entry->next_resend) {
                    next_resend = entry->next_resend;
                }
            }
            return false;
        }), this->entries.end());
    }

    for(const auto& failed : resend_failed) {
        this->callback_resend_failed(this->callback_data, failed);
    }
}