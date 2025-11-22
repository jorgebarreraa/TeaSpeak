#pragma once

#include <cstring>
#include <string>
#include <map>
#include <utility>
#include <ThreadPool/Future.h>
#include <pipes/buffer.h>
#include "../query/Command.h"

namespace ts {
    namespace protocol {
        enum PacketType : uint8_t {
            VOICE = 0x00,
            VOICE_WHISPER = 0x01,
            COMMAND = 0x02,
            COMMAND_LOW = 0x03,
            PING = 0x04,
            PONG = 0x05,
            ACK = 0x06,
            ACK_LOW = 0x07,
            INIT1 = 0x08,

            PACKET_MAX = INIT1,
            UNDEFINED = 0xFF
        };

        struct PacketTypeProperties {
            std::string name;
            PacketType type;
            int max_length;
            bool requireAcknowledge;
        };

        class PacketTypeInfo {
            public:
                static PacketTypeInfo Voice;
                static PacketTypeInfo VoiceWhisper;
                static PacketTypeInfo Command;
                static PacketTypeInfo CommandLow;
                static PacketTypeInfo Ping;
                static PacketTypeInfo Pong;
                static PacketTypeInfo Ack;
                static PacketTypeInfo AckLow;
                static PacketTypeInfo Init1;
                static PacketTypeInfo Undefined;

                static PacketTypeInfo fromid(int id);

                std::string name() const { return data->name; }
                PacketType type() const { return data->type; }

                bool requireAcknowledge(){ return data->requireAcknowledge; }

                bool operator==(const PacketTypeInfo& other) const {
                    return other.data->type == this->data->type;
                }

                bool operator!=(const PacketTypeInfo& other){
                    return other.data->type != this->data->type;
                }

                int max_length() const { return data->max_length; }
                inline bool fragmentable() { return *this == PacketTypeInfo::Command || *this == PacketTypeInfo::CommandLow; }
                inline bool compressable() { return *this == PacketTypeInfo::Command || *this == PacketTypeInfo::CommandLow; }

                PacketTypeInfo(const PacketTypeInfo&);
                PacketTypeInfo(PacketTypeInfo&& remote) : data(remote.data) {}

                ~PacketTypeInfo();
            private:
                static std::map<int, PacketTypeInfo> types;
                PacketTypeInfo(const std::string&, PacketType, bool, int) noexcept;
                PacketTypeProperties* data;
                bool owns_data = false;
        };

        struct PacketIdManagerData {
            PacketIdManagerData(){
                memset(this->packetCounter, 0, sizeof(uint32_t) * 16);
            }
            uint32_t packetCounter[16]{};
        };

        class PacketIdManager {
            public:
                PacketIdManager() : data(new PacketIdManagerData){}
                ~PacketIdManager() = default;
                PacketIdManager(const PacketIdManager& ref) = default;
                PacketIdManager(PacketIdManager&& ref) = default;

                uint16_t nextPacketId(const PacketTypeInfo &type){
                    return static_cast<uint16_t>(data->packetCounter[type.type()]++ & 0xFFFFU);
                }

                uint16_t currentPacketId(const PacketTypeInfo &type){
                    return static_cast<uint16_t>(data->packetCounter[type.type()] & 0xFFFFU);
                }

                uint16_t generationId(const PacketTypeInfo &type){
                    return static_cast<uint16_t>((data->packetCounter[type.type()] >> 16U) & 0xFFFFU);
                }

                void reset() {
                    memset(&data->packetCounter[0], 0, sizeof(uint32_t) * 16);
                }
            private:
                std::shared_ptr<PacketIdManagerData> data;
        };

        namespace PacketFlag {
            enum PacketFlag : uint8_t {
                None = 0x00,
                Fragmented = 0x10,  //If packet type voice then its toggle the CELT Mono
                NewProtocol = 0x20,
                Compressed = 0x40,  //If packet type voice than its the header
                Unencrypted = 0x80
            };
            typedef uint8_t PacketFlags;

            std::string to_string(PacketFlag flag);
        }

        #define MAC_SIZE 8
        #define SERVER_HEADER_SIZE 3
        #define CLIENT_HEADER_SIZE 5

        class BasicPacket {
            public:
                explicit BasicPacket(size_t header_length, size_t data_length);
                virtual ~BasicPacket();

                BasicPacket(const BasicPacket&) = delete;
                BasicPacket(BasicPacket&&) = delete;

                virtual uint16_t packetId() const = 0;
                virtual uint16_t generationId() const = 0;
                virtual PacketTypeInfo type() const = 0;

                /* packet flag info */
                inline bool has_flag(PacketFlag::PacketFlag flag) const { return this->_flags_type_byte() & flag; }
                inline uint8_t flag_mask() const { return this->_flags_type_byte(); };
                [[nodiscard]] std::string flags() const;

                /* manipulate flags */
                inline void set_flags(PacketFlag::PacketFlags flags) {
                    uint8_t& byte = this->_flags_type_byte();
                    byte &= 0xF; /* clear all flags */
                    byte |= (flags & 0xF0);
                }
                inline void enable_flag(PacketFlag::PacketFlag flag){ this->toggle_flag(flag, true); }
                inline void toggle_flag(PacketFlag::PacketFlag flag, bool state) {
                    if(state)
                        this->_flags_type_byte() |= flag;
                    else
                        this->_flags_type_byte() &= (uint8_t) ~flag;
                }

                virtual void applyPacketId(PacketIdManager &);
                virtual void applyPacketId(uint16_t, uint16_t);

                void setListener(std::unique_ptr<threads::Future<bool>> listener){
                    if(!this->type().requireAcknowledge())
                        throw std::logic_error("Packet type does not support a acknowledge listener!");
                    this->listener = std::move(listener);
                }
                inline std::unique_ptr<threads::Future<bool>>& getListener() { return this->listener; }

                inline size_t length() const { return this->_buffer.length(); }
                inline const pipes::buffer_view mac() const { return this->_buffer.view(0, MAC_SIZE); }
                inline pipes::buffer mac() { return this->_buffer.range(0, MAC_SIZE); }
                inline size_t mac_length() const { return MAC_SIZE; }

                inline const pipes::buffer_view header() const { return this->_buffer.view(MAC_SIZE, this->_header_length); }
                inline pipes::buffer header() { return this->_buffer.range(MAC_SIZE, this->_header_length); }
                inline size_t header_length() const { return this->_header_length; }

                inline size_t data_length() const { return this->_buffer.length() - (MAC_SIZE + this->_header_length); }
                inline const pipes::buffer_view data() const { return this->_buffer.view(MAC_SIZE + this->_header_length); }
                inline pipes::buffer data() { return this->_buffer.range(MAC_SIZE + this->_header_length); }

                void append_data(const std::vector<pipes::buffer> &data);

                inline void data(const pipes::buffer_view &data){
                    this->_buffer.resize(MAC_SIZE + this->_header_length + data.length());
                    memcpy((char*) this->_buffer.data_ptr() + MAC_SIZE + this->_header_length, data.data_ptr(), data.length());
                }

                inline void mac(const pipes::buffer_view  &_new){
                    assert(_new.length() >= MAC_SIZE);
                    memcpy(this->_buffer.data_ptr(), _new.data_ptr(), MAC_SIZE);
                }

                [[nodiscard]] inline bool isEncrypted() const { return this->memory_state.encrypted; }
                inline void setEncrypted(bool flag){ this->memory_state.encrypted = flag; }

                [[nodiscard]] inline bool isCompressed() const { return this->memory_state.compressed; }
                inline void setCompressed(bool flag){ this->memory_state.compressed = flag; }

                [[nodiscard]] inline bool isFragmentEntry() const { return this->memory_state.fragment_entry; }
                inline void setFragmentedEntry(bool flag){ this->memory_state.fragment_entry = flag; }

                Command asCommand();

                //Has the size of a byte
                union {
#ifdef WIN32
                    __pragma(pack(push, 1))
#endif
                    struct {
                        bool encrypted: 1;
                        bool compressed: 1;
                        bool fragment_entry: 1;

                        bool id_branded: 1;
                    }
#ifdef WIN32
                    __pragma(pack(pop));
#else
                    __attribute__((packed));
#endif

                    uint8_t flags = 0;
                } memory_state;

                pipes::buffer buffer() { return this->_buffer; }
                void buffer(pipes::buffer buffer) {
                    assert(buffer.length() >= this->_header_length + MAC_SIZE);
                    this->_buffer = std::move(buffer);
                }
            protected:
                BasicPacket() = default;

                virtual const uint8_t& _flags_type_byte() const = 0;
                virtual uint8_t& _flags_type_byte() = 0;

                virtual void setPacketId(uint16_t, uint16_t) = 0;
                uint8_t _header_length;
                pipes::buffer _buffer;

                uint16_t genId = 0;
                std::unique_ptr<threads::Future<bool>> listener;
        };


        /**
         * Packet from the client
         */
        class ClientPacket : public BasicPacket {
                friend std::unique_ptr<ClientPacket> std::make_unique<ClientPacket>();
            public:
                static constexpr size_t META_MAC_SIZE = 8;
                static constexpr size_t META_HEADER_SIZE = CLIENT_HEADER_SIZE;
                static constexpr size_t META_SIZE = META_MAC_SIZE + META_HEADER_SIZE;

                [[nodiscard]] static std::unique_ptr<ClientPacket> from_buffer(const pipes::buffer_view& buffer);

                ClientPacket(const PacketTypeInfo& type, const pipes::buffer_view& data);
                ClientPacket(const PacketTypeInfo& type, uint8_t flag_mask, const pipes::buffer_view& data);
                ~ClientPacket() override;
                ClientPacket(const ClientPacket&) = delete;
                ClientPacket(ClientPacket&&) = delete;

                uint16_t clientId() const;
                void clientId(uint16_t);

                uint16_t packetId() const override;

                uint16_t generationId() const override;
                void generationId(uint16_t generation) { this->genId = generation; }

                PacketTypeInfo type() const override;
                void type(const PacketTypeInfo&);

            private:
                ClientPacket() = default;

                const uint8_t &_flags_type_byte() const override {
                    return this->header().data_ptr<uint8_t>()[4];
                }

                uint8_t &_flags_type_byte() override {
                    return this->header().data_ptr<uint8_t>()[4];
                }

                void setPacketId(uint16_t, uint16_t) override;
        };

        class PacketParser {
            public:
                PacketParser(const PacketParser&) = delete;
                explicit PacketParser(pipes::buffer_view buffer) : _buffer{std::move(buffer)} {}

                [[nodiscard]] inline const void* data_ptr() const { return this->_buffer.data_ptr(); }
                [[nodiscard]] inline void* mutable_data_ptr() { return (void*) this->_buffer.data_ptr(); }

                [[nodiscard]] inline pipes::buffer_view buffer() const { return this->_buffer; }
                [[nodiscard]] inline pipes::buffer_view mac() const { return this->_buffer.view(0, 8); }
                [[nodiscard]] virtual pipes::buffer_view payload() const = 0;
                [[nodiscard]] virtual size_t payload_length() const = 0;

                [[nodiscard]] inline uint32_t full_packet_id() const { return this->packet_id() | (this->estimated_generation() << 16U); }
                [[nodiscard]] virtual uint16_t packet_id() const = 0;
                [[nodiscard]] virtual uint8_t type() const = 0;
                [[nodiscard]] virtual uint8_t flags() const = 0;

                [[nodiscard]] bool is_encrypted() const;
                [[nodiscard]] bool is_compressed() const;
                [[nodiscard]] bool is_fragmented() const;

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

                [[nodiscard]] inline pipes::buffer_view payload() const override { return this->_buffer.view(kPayloadOffset); }
                [[nodiscard]] inline size_t payload_length() const override { return this->_buffer.length() - kPayloadOffset; }

                [[nodiscard]] uint16_t client_id() const;
                [[nodiscard]] uint16_t packet_id() const override;
                [[nodiscard]] uint8_t type() const override;
                [[nodiscard]] uint8_t flags() const override;
        };

        /**
         * Packet from the server
         */
        class ServerPacket : public BasicPacket {
                friend std::unique_ptr<ServerPacket> std::make_unique<ServerPacket>();
            public:
                static constexpr size_t META_MAC_SIZE = 8;
                static constexpr size_t META_HEADER_SIZE = SERVER_HEADER_SIZE;
                static constexpr size_t META_SIZE = META_MAC_SIZE + META_HEADER_SIZE;

                [[nodiscard]] static std::unique_ptr<ServerPacket> from_buffer(const pipes::buffer_view& buffer);

                ServerPacket(uint8_t flagMask, const pipes::buffer_view& data);
                ServerPacket(const PacketTypeInfo& type, const pipes::buffer_view& data);
                ServerPacket(PacketTypeInfo type, size_t /* data length */);
                ~ServerPacket() override;

                ServerPacket(const ServerPacket&) = delete;
                ServerPacket(ServerPacket&&) = delete;

                [[nodiscard]] uint16_t packetId() const override;
                [[nodiscard]] uint16_t generationId() const override;
                void generationId(uint16_t generation) { this->genId = generation; }
                [[nodiscard]] PacketTypeInfo type() const override;

            private:
                ServerPacket() = default;

                [[nodiscard]] const uint8_t &_flags_type_byte() const override {
                    return this->header().data_ptr<uint8_t>()[2];
                }

                uint8_t &_flags_type_byte() override {
                    return this->header().data_ptr<uint8_t>()[2];
                }

                void setPacketId(uint16_t, uint16_t) override;
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

                [[nodiscard]] inline pipes::buffer_view payload() const override { return this->_buffer.view(kPayloadOffset); }
                [[nodiscard]] inline size_t payload_length() const override { return this->_buffer.length() - kPayloadOffset; }

                [[nodiscard]] uint16_t packet_id() const override;
                [[nodiscard]] uint8_t type() const override;
                [[nodiscard]] uint8_t flags() const override;
        };
    }
}