#pragma once

#include <cstring>
#include <string>
#include <map>
#include <utility>
#include <array>
#include <pipes/buffer.h>
#include "../query/Command.h"

namespace ts::protocol {
    enum PacketType : uint8_t {
        VOICE            = 0x00,
        VOICE_WHISPER    = 0x01,
        COMMAND          = 0x02,
        COMMAND_LOW      = 0x03,
        PING             = 0x04,
        PONG             = 0x05,
        ACK              = 0x06,
        ACK_LOW          = 0x07,
        INIT1            = 0x08,
    };

    class PacketIdManager {
        public:
            PacketIdManager() = default;
            ~PacketIdManager() = default;
            PacketIdManager(const PacketIdManager& ref) = delete;
            PacketIdManager(PacketIdManager&& ref) = delete;

            [[nodiscard]] uint16_t nextPacketId(const PacketType &type) {
                return (uint16_t) (this->packet_counter[(uint8_t) type & 0xFU]++);
            }

            [[nodiscard]] uint16_t currentPacketId(const PacketType &type) {
                return (uint16_t) (this->packet_counter[(uint8_t) type & 0xFU]);
            }

            [[nodiscard]] uint16_t generationId(const PacketType &type) {
                return (uint16_t) (this->packet_counter[(uint8_t) type & 0xFU] >> 16U);
            }

            [[nodiscard]] uint32_t generate_full_id(const PacketType& type) {
                return this->packet_counter[type]++;
            }

            void reset() {
                memset(&this->packet_counter[0], 0, sizeof(uint32_t) * 16);
            }

        private:
            std::array<uint32_t, 16> packet_counter{};
    };

    enum struct PacketFlag {
        None = 0x00,
        Fragmented = 0x10,  //If packet type voice then its toggle the CELT Mono
        NewProtocol = 0x20,
        Compressed = 0x40,  //If packet type voice than its the header
        Unencrypted = 0x80
    };
    typedef uint8_t PacketFlags;

    constexpr const char* packet_flag_to_string(const PacketFlag& flag) {
        switch(flag){
            case PacketFlag::Fragmented:
                return "Fragmented";

            case PacketFlag::NewProtocol:
                return "NewProtocol";

            case PacketFlag::Compressed:
                return "Compressed";

            case PacketFlag::Unencrypted:
                return "Unencrypted";

            case PacketFlag::None:
            default:
                return "None";
        }
    }

    #define MAC_SIZE 8
    #define SERVER_HEADER_SIZE 3
    #define CLIENT_HEADER_SIZE 5

    class PacketParser {
        public:
            PacketParser(const PacketParser&) = delete;
            explicit PacketParser(pipes::buffer_view buffer) : _buffer{std::move(buffer)} {}

            [[nodiscard]] inline const void* data_ptr() const { return this->_buffer.data_ptr(); }
            [[nodiscard]] inline void* mutable_data_ptr() { return (void*) this->_buffer.data_ptr(); }

            [[nodiscard]] inline pipes::buffer_view buffer() const { return this->_buffer; }
            [[nodiscard]] inline pipes::buffer_view mac() const { return this->_buffer.view(0, 8); }
            [[nodiscard]] virtual pipes::buffer_view header() const = 0;
            [[nodiscard]] virtual pipes::buffer_view payload() const = 0;
            [[nodiscard]] virtual void* payload_ptr_mut() = 0;
            [[nodiscard]] virtual size_t payload_length() const = 0;

            [[nodiscard]] inline uint32_t full_packet_id() const { return this->packet_id() | (uint32_t) ((uint32_t) this->estimated_generation() << 16U); }
            [[nodiscard]] virtual uint16_t packet_id() const = 0;
            [[nodiscard]] virtual uint8_t type() const = 0;
            [[nodiscard]] virtual uint8_t flags() const = 0;

            [[nodiscard]] inline bool has_flag(const PacketFlag& flag) const { return this->flags() & (uint8_t) flag; }
            [[nodiscard]] inline bool is_encrypted() const {
                return !this->decrypted && !this->has_flag(PacketFlag::Unencrypted);
            }

            [[nodiscard]] inline bool is_compressed() const {
                return !this->uncompressed && this->has_flag(PacketFlag::Compressed);
            }

            [[nodiscard]] inline bool is_fragmented() const {
                return !this->defragmented && this->has_flag(PacketFlag::Fragmented);
            }

            [[nodiscard]] uint16_t estimated_generation() const { return this->generation; }
            void set_estimated_generation(uint16_t gen) { this->generation = gen; }

            inline void set_decrypted() { this->decrypted = true; }
            inline void set_uncompressed() { this->uncompressed = true; }
            inline void set_defragmented() { this->defragmented = true; }

        protected:
            uint16_t generation{};
            bool decrypted{false}, uncompressed{false}, defragmented{false};
            pipes::buffer_view _buffer{};
    };

    class ClientPacketParser : public PacketParser {
        public:
            constexpr static auto kHeaderOffset = 8;
            constexpr static auto kHeaderLength = CLIENT_HEADER_SIZE;

            constexpr static auto kPayloadOffset = kHeaderOffset + CLIENT_HEADER_SIZE;
            explicit ClientPacketParser(pipes::buffer_view buffer) : PacketParser{std::move(buffer)} {}
            ClientPacketParser(const ClientPacketParser&) = delete;

            [[nodiscard]] inline bool valid() const {
                if(this->_buffer.length() < kPayloadOffset) return false;
                return this->type() <= 8;
            }
            [[nodiscard]] inline pipes::buffer_view header() const override { return this->_buffer.view(kHeaderOffset, kHeaderLength); }
            [[nodiscard]] inline pipes::buffer_view payload() const override { return this->_buffer.view(kPayloadOffset); }
            [[nodiscard]] inline void* payload_ptr_mut() override { return (char*) this->mutable_data_ptr() + kPayloadOffset; };
            [[nodiscard]] inline size_t payload_length() const override { return this->_buffer.length() - kPayloadOffset; }

            [[nodiscard]] uint16_t client_id() const;
            [[nodiscard]] uint16_t packet_id() const override;
            [[nodiscard]] uint8_t type() const override;
            [[nodiscard]] uint8_t flags() const override;
    };

    class ServerPacketParser : public PacketParser {
        public:
            constexpr static auto kHeaderOffset = 8;
            constexpr static auto kHeaderLength = SERVER_HEADER_SIZE;

            constexpr static auto kPayloadOffset = kHeaderOffset + SERVER_HEADER_SIZE;

            explicit ServerPacketParser(pipes::buffer_view buffer) : PacketParser{std::move(buffer)} {}
            ServerPacketParser(const ServerPacketParser&) = delete;

            [[nodiscard]] inline bool valid() const {
                if(this->_buffer.length() < kPayloadOffset) return false;
                return this->type() <= 8;
            }

            [[nodiscard]] inline pipes::buffer_view header() const override { return this->_buffer.view(kHeaderOffset, kHeaderLength); }
            [[nodiscard]] inline pipes::buffer_view payload() const override { return this->_buffer.view(kPayloadOffset); }
            [[nodiscard]] inline void* payload_ptr_mut() override { return (char*) this->mutable_data_ptr() + kPayloadOffset; };
            [[nodiscard]] inline size_t payload_length() const override { return this->_buffer.length() - kPayloadOffset; }

            [[nodiscard]] uint16_t packet_id() const override;
            [[nodiscard]] uint8_t type() const override;
            [[nodiscard]] uint8_t flags() const override;
    };

    struct OutgoingPacket {
        public:
            OutgoingPacket() = default;
            virtual ~OutgoingPacket() = default;

            /* general info */
            std::atomic<uint16_t> ref_count{0};
            size_t payload_size;
            uint16_t generation;

            inline auto ref() {
                auto count = ++ref_count;
                assert(count > 1);
                return count;
            }

            inline void unref() {
                if(--this->ref_count == 0) {
                    this->free_object();
                }
            }

            /* some helper methods */
            inline void set_packet_id(uint16_t id) {
                auto packet_id_bytes = this->packet_id_bytes();
                packet_id_bytes[0] = id >> 8U;
                packet_id_bytes[1] = id & 0xFFU;
            }

            [[nodiscard]] inline uint16_t packet_id() const {
                auto packet_id_bytes = this->packet_id_bytes();
                return (uint16_t) (packet_id_bytes[0] << 8U) | packet_id_bytes[1];
            }

            [[nodiscard]] inline auto packet_type() const {
                auto type_and_flags = this->type_and_flags();
                return (PacketType) (type_and_flags & 0xFU);
            }

            /**
             * @returns a pointer to the beginning of the packet including the packet header
             */
            [[nodiscard]] virtual const void* packet_data() const = 0;

            /**
             * @returns the full packet length including the packet header
             */
            [[nodiscard]] virtual size_t packet_length() const = 0;
            [[nodiscard]] virtual uint8_t type_and_flags() const = 0;

            [[nodiscard]] virtual OutgoingPacket* next_in_queue() const = 0;
            virtual void set_next_in_queue(OutgoingPacket*) = 0;
        protected:
            [[nodiscard]] virtual const uint8_t* packet_id_bytes() const = 0;
            [[nodiscard]] virtual uint8_t* packet_id_bytes() = 0;
            virtual void free_object() = 0;
    };

    struct OutgoingClientPacket : public OutgoingPacket {
        public:
            OutgoingClientPacket() = default;
            ~OutgoingClientPacket() override = default;

            OutgoingClientPacket* next; /* used within the write/process queue */

            /* actual packet data */
            uint8_t mac[8];
            uint8_t packet_id_bytes_[2];
            uint8_t client_id_bytes[2];
            uint8_t type_and_flags_;
            uint8_t payload[1]; /* variable size */

            [[nodiscard]] inline const void* packet_data() const override {
                return this->mac;
            }

            [[nodiscard]] inline size_t packet_length() const override {
                return this->payload_size + (8 + 2 + 2 + 1);
            }

            [[nodiscard]] inline uint8_t type_and_flags() const override {
                return this->type_and_flags_;
            }

            [[nodiscard]] inline OutgoingPacket* next_in_queue() const override {
                return this->next;
            }

            inline void set_next_in_queue(OutgoingPacket* packet) override {
                this->next = dynamic_cast<OutgoingClientPacket*>(packet);
                assert(!packet || this->next);
            }
        protected:
            [[nodiscard]] inline const uint8_t* packet_id_bytes() const override {
                return this->packet_id_bytes_;
            }

            [[nodiscard]] inline uint8_t* packet_id_bytes() override {
                return this->packet_id_bytes_;
            }

            void free_object() override;
    };

    struct OutgoingServerPacket : public OutgoingPacket {
        public:
            virtual ~OutgoingServerPacket() = default;

            OutgoingServerPacket* next; /* used within the write/process queue */

            /* actual packet data */
            uint8_t mac[8];
            uint8_t packet_id_bytes_[2];
            uint8_t type_and_flags_;
            uint8_t payload[1]; /* variable size */

            [[nodiscard]] inline uint8_t type_and_flags() const override {
                return this->type_and_flags_;
            }

            [[nodiscard]] inline const void* packet_data() const override {
                return this->mac;
            }

            [[nodiscard]] inline size_t packet_length() const override {
                return this->payload_size + (8 + 2 + 1);
            }

            [[nodiscard]] inline OutgoingPacket* next_in_queue() const override {
                return this->next;
            }

            inline void set_next_in_queue(OutgoingPacket* packet) override {
                this->next = dynamic_cast<OutgoingServerPacket*>(packet);
                assert(!packet || this->next);
            }
        protected:
            [[nodiscard]] inline const uint8_t* packet_id_bytes() const override {
                return this->packet_id_bytes_;
            }

            [[nodiscard]] inline uint8_t* packet_id_bytes() override {
                return this->packet_id_bytes_;
            }

            void free_object() override;
    };

    /* This will allocate a new outgoing packet. To delete just unref the packet! */
    OutgoingServerPacket* allocate_outgoing_server_packet(size_t /* payload size */);
    OutgoingClientPacket* allocate_outgoing_client_packet(size_t /* payload size */);

    inline PacketFlags& operator|=(PacketFlags& flags, const PacketFlag& flag) {
        flags |= (uint8_t) flag;
        return flags;
    }

    inline PacketFlags operator|(PacketFlags flags, const PacketFlag& flag) {
        return flags |= flag;
    }

    inline PacketFlags& operator&=(PacketFlags& flags, const PacketFlag& flag) {
        flags &= (uint8_t) flag;
        return flags;
    }

    inline PacketFlags operator&(PacketFlags flags, const PacketFlag& flag) {
        return flags &= flag;
    }

    inline PacketFlags operator|(const PacketFlag& flag_a, const PacketFlag& flag_b) {
        return (uint8_t) flag_a | (uint8_t) flag_b;
    }
}