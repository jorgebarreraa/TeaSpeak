//
// Created by wolverindev on 07.10.17.
//

#include <cstring>
#include <bitset>
#include <mutex>
#include "./Packet.h"
#include "../misc/endianness.h"
#include "../misc/spin_mutex.h"

using namespace std;
namespace ts {
    namespace protocol {
        uint16_t ClientPacketParser::packet_id() const { return be2le16(this->_buffer.data_ptr<uint8_t>(), ClientPacketParser::kHeaderOffset + 0); }
        uint16_t ClientPacketParser::client_id() const { return be2le16(this->_buffer.data_ptr<uint8_t>(), ClientPacketParser::kHeaderOffset + 2); }
        uint8_t ClientPacketParser::type() const { return (uint8_t) this->_buffer[ClientPacketParser::kHeaderOffset + 4] & 0xFU; }
        uint8_t ClientPacketParser::flags() const { return (uint8_t) this->_buffer[ClientPacketParser::kHeaderOffset + 4] & 0xF0U; }

        uint16_t ServerPacketParser::packet_id() const { return be2le16(this->_buffer.data_ptr<uint8_t>(), ClientPacketParser::kHeaderOffset + 0); }
        uint8_t ServerPacketParser::type() const { return (uint8_t) this->_buffer[ClientPacketParser::kHeaderOffset + 2] & 0xFU; }
        uint8_t ServerPacketParser::flags() const { return (uint8_t) this->_buffer[ClientPacketParser::kHeaderOffset + 2] & 0xF0U; }
    }

    void construct_ocp(protocol::OutgoingClientPacket* packet) {
        new (packet) protocol::OutgoingClientPacket{};
    }

    void deconstruct_ocp(protocol::OutgoingClientPacket* packet) {
        packet->~OutgoingClientPacket();
    }

    void reset_ocp(protocol::OutgoingClientPacket* packet, size_t payload_size) {
        packet->next = nullptr;
        packet->payload_size = payload_size;
        packet->type_and_flags_ = 0;
        packet->generation = 0;
    }

    void construct_osp(protocol::OutgoingServerPacket* packet) {
        new (packet) protocol::OutgoingServerPacket{};
    }

    void deconstruct_osp(protocol::OutgoingServerPacket* packet) {
        packet->~OutgoingServerPacket();
    }

    void reset_osp(protocol::OutgoingServerPacket* packet, size_t payload_size) {
        packet->next = nullptr;
        packet->payload_size = payload_size;
        packet->type_and_flags_ = 0;
        packet->generation = 0;
    }

#if 1
    #define BUKKIT_ENTRY_SIZE (1650)
    #define BUKKIT_MAX_ENTRIES (3000)

    struct OSPBukkitEntry {
        bool extra_allocated;
        OSPBukkitEntry* next;
    };

    spin_mutex osp_mutex{};
    size_t sdp_count{0};
    OSPBukkitEntry* osp_head{nullptr};
    OSPBukkitEntry** osp_tail{&osp_head};

    protocol::OutgoingServerPacket* osp_from_bosp(OSPBukkitEntry* bops) {
        return reinterpret_cast<protocol::OutgoingServerPacket*>((char*) bops + sizeof(OSPBukkitEntry));
    }

    OSPBukkitEntry* bosp_from_osp(protocol::OutgoingServerPacket* ops) {
        return reinterpret_cast<OSPBukkitEntry*>((char*) ops - sizeof(OSPBukkitEntry));
    }

    void destroy_bosp(OSPBukkitEntry* entry) {
        deconstruct_osp(osp_from_bosp(entry));
        ::free(entry);
    }

    OSPBukkitEntry* construct_bosp(size_t payload_size) {
        auto base_size = sizeof(OSPBukkitEntry) + sizeof(protocol::OutgoingServerPacket) - 1;
        auto full_size = base_size + payload_size;
        auto bentry = (OSPBukkitEntry*) malloc(full_size);

        bentry->next = nullptr;
        bentry->extra_allocated = false;

        construct_osp(osp_from_bosp(bentry));
        return bentry;
    }

    void protocol::OutgoingServerPacket::free_object() {
        auto bentry = (OSPBukkitEntry*) bosp_from_osp(this);
        if(bentry->extra_allocated) {
            destroy_bosp(bentry);
            return;
        }

        std::unique_lock block{osp_mutex};
        if(sdp_count >= BUKKIT_MAX_ENTRIES) {
            block.unlock();
            destroy_bosp(bentry);
            return;
        }

        assert(!bentry->next);
        *osp_tail = bentry;
        osp_tail = &bentry->next;
        sdp_count++;
    }

    protocol::OutgoingServerPacket* protocol::allocate_outgoing_server_packet(size_t payload_size) {
        if(BUKKIT_ENTRY_SIZE >= payload_size) {
            std::lock_guard block{osp_mutex};
            if(osp_head) {
                assert(sdp_count > 0);
                sdp_count--;
                auto entry = osp_head;
                if(osp_head->next) {
                    assert(osp_tail != &osp_head->next);
                    osp_head = osp_head->next;
                } else {
                    assert(osp_tail == &osp_head->next);
                    osp_head = nullptr;
                    osp_tail = &osp_head;
                }

                entry->next = nullptr;

                auto result = osp_from_bosp(entry);
                reset_osp(result, payload_size);
                result->ref_count++;
                return result;
            } else if(sdp_count < BUKKIT_MAX_ENTRIES) {
                auto entry = construct_bosp(BUKKIT_ENTRY_SIZE);
                entry->extra_allocated = false;

                auto result = osp_from_bosp(entry);
                reset_osp(result, payload_size);
                result->ref_count++;
                return result;
            }
        }

        auto entry = construct_bosp(payload_size);
        entry->extra_allocated = true;

        auto result = osp_from_bosp(entry);
        reset_osp(result, payload_size);
        result->ref_count++;
        return result;
    }
#else
    void protocol::OutgoingServerPacket::free_object() {
        deconstruct_osp(this);
        ::free(this);
    }

    protocol::OutgoingServerPacket* protocol::allocate_outgoing_server_packet(size_t payload_size) {
        auto base_size = sizeof(protocol::OutgoingServerPacket) - 1;
        /* Allocate at least one payload byte since we're our payload array of length 1 */
        auto full_size = base_size + std::max(payload_size, (size_t) 1);
        auto result = (protocol::OutgoingServerPacket*) malloc(full_size);

        construct_osp(result);
        reset_osp(result, payload_size);
        result->ref_count++;

        return result;
    }
#endif

    void protocol::OutgoingClientPacket::free_object() {
        deconstruct_ocp(this);
        ::free(this);
    }

    protocol::OutgoingClientPacket* protocol::allocate_outgoing_client_packet(size_t payload_size) {
        auto base_size = sizeof(protocol::OutgoingClientPacket) - 1;
        /* Allocate at least one payload byte since we're our payload array of length 1 */
        auto full_size = base_size + std::max(payload_size, (size_t) 1);
        auto result = (protocol::OutgoingClientPacket*) malloc(full_size);

        construct_ocp(result);
        reset_ocp(result, payload_size);
        result->ref_count++;

        return result;
    }
}