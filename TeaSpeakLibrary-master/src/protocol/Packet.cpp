//
// Created by wolverindev on 07.10.17.
//

#include <cstring>
#include <iostream>
#include <bitset>
#include "Packet.h"
#include "buffers.h"
#include "misc/endianness.h"

using namespace std;
namespace ts {
    namespace protocol {

        PacketTypeInfo::PacketTypeInfo(const std::string& name, PacketType type, bool ack, int max_length) noexcept {
            this->data = new PacketTypeProperties{name, type, max_length, ack};
            this->owns_data = true;

            if(type < 0x0F)
                types.insert({type, *this});
        }

        PacketTypeInfo::~PacketTypeInfo() {
            if(this->owns_data)
                delete this->data;
        }

        PacketTypeInfo::PacketTypeInfo(const PacketTypeInfo &red) : data(red.data) { }

        std::map<int, PacketTypeInfo> PacketTypeInfo::types;
        PacketTypeInfo PacketTypeInfo::fromid(int id) {
            for(const auto& elm : types)
                if(elm.first == id) return elm.second;
            return PacketTypeInfo::Undefined;
        }

        PacketTypeInfo PacketTypeInfo::Voice = {"Voice", PacketType::VOICE, false, 1024};
        PacketTypeInfo PacketTypeInfo::VoiceWhisper = {"VoiceWhisper", PacketType::VOICE_WHISPER, false, 1024};
        PacketTypeInfo PacketTypeInfo::Command = {"Command", PacketType::COMMAND, true, 487};
        PacketTypeInfo PacketTypeInfo::CommandLow = {"CommandLow", PacketType::COMMAND_LOW, true, 487};
        PacketTypeInfo PacketTypeInfo::Ping = {"Ping", PacketType::PING, false, 1024};
        PacketTypeInfo PacketTypeInfo::Pong = {"Pong", PacketType::PONG, false, 1024};
        PacketTypeInfo PacketTypeInfo::Ack = {"Ack", PacketType::ACK, false, 1024};
        PacketTypeInfo PacketTypeInfo::AckLow = {"AckLow", PacketType::ACK_LOW, false, 1024};
        PacketTypeInfo PacketTypeInfo::Init1 = {"Init1", PacketType::INIT1, false, 1024};
        PacketTypeInfo PacketTypeInfo::Undefined = {"Undefined", PacketType::UNDEFINED, false, 1024};

        namespace PacketFlag {
            std::string to_string(PacketFlag flag){
                switch(flag){
                    case Fragmented:
                        return "Fragmented";
                    case NewProtocol:
                        return "NewProtocol";
                    case Compressed:
                        return "Compressed";
                    case Unencrypted:
                        return "Unencrypted";
                    default:
                        return "None";
                }
            }
        }

        BasicPacket::BasicPacket(size_t header_length, size_t data_length) {
            this->_header_length = (uint8_t) header_length;
            this->_buffer = pipes::buffer(MAC_SIZE + this->_header_length + data_length);
            memset(this->_buffer.data_ptr(), 0, this->_buffer.length());
        }

        BasicPacket::~BasicPacket() {}

        void BasicPacket::append_data(const std::vector<pipes::buffer> &data) {
            size_t length = 0;
            for(const auto& buffer : data)
                length += buffer.length();

            /* we've to allocate a new buffer because out buffer is fixed in size */
            size_t index = this->_buffer.length();
            auto new_buffer = buffer::allocate_buffer(length + index);
            new_buffer.write(this->_buffer, index);

            for(const auto& buffer : data) {
                new_buffer.write(buffer, buffer.length(), index);
                index += buffer.length();
            }

            this->_buffer = new_buffer;
        }

        std::string BasicPacket::flags() const {
            std::string result;

            if(this->has_flag(PacketFlag::Unencrypted))    result += string(result.empty() ? "" : " | ") + "Unencrypted";
            if(this->has_flag(PacketFlag::Compressed))     result += string(result.empty() ? "" : " | ") + "Compressed";
            if(this->has_flag(PacketFlag::Fragmented))     result += string(result.empty() ? "" : " | ") + "Fragmented";
            if(this->has_flag(PacketFlag::NewProtocol))    result += string(result.empty() ? "" : " | ") + "NewProtocol";

            if(result.empty()) result = "none";
            return result;
        }

        void BasicPacket::applyPacketId(PacketIdManager& manager) {
            this->applyPacketId(manager.nextPacketId(this->type()), manager.generationId(this->type()));
        }
        void BasicPacket::applyPacketId(uint16_t packetId, uint16_t generationId) {
            if(this->memory_state.id_branded)
                throw std::logic_error("Packet already got a packet id!");
            this->memory_state.id_branded = true;
            this->setPacketId(packetId, generationId);
        }
        Command BasicPacket::asCommand() {
            return Command::parse(this->data());
        }

        /**
         * @param buffer -> [mac][Header [uint16 BE packetId | [uint8](4bit flags | 4bit type)]][Data]
         * @return
         */
        std::unique_ptr<ServerPacket> ServerPacket::from_buffer(const pipes::buffer_view &buffer) {
            auto result = make_unique<ServerPacket>();

            result->_buffer = buffer.own_buffer();
            result->_header_length = SERVER_HEADER_SIZE;

            return result;
        }

        ServerPacket::ServerPacket(uint8_t flagMask, const pipes::buffer_view& data) : BasicPacket(SERVER_HEADER_SIZE, data.length()) {
            this->header()[2] = flagMask;
            memcpy(this->data().data_ptr(), data.data_ptr(), data.length());
        }

        ServerPacket::ServerPacket(const PacketTypeInfo& type, const pipes::buffer_view& data) : BasicPacket(SERVER_HEADER_SIZE, data.length()) {
            this->header()[2] |= (uint8_t) type.type();
            memcpy(this->data().data_ptr(), data.data_ptr(), data.length());
        }

        ServerPacket::ServerPacket(ts::protocol::PacketTypeInfo type, size_t data_length) : BasicPacket(SERVER_HEADER_SIZE, data_length) {
            this->header()[2] |= type.type();
        }

        ServerPacket::~ServerPacket() {}

        uint16_t ServerPacket::packetId() const {
            return be2le16(&this->header()[0]);
        }

        void ServerPacket::setPacketId(uint16_t pkId, uint16_t gen) {
            le2be16(pkId, &this->header()[0]);
            this->genId = gen;
        }

        uint16_t ServerPacket::generationId() const {
            return this->genId;
        }

        PacketTypeInfo ServerPacket::type() const {
            return PacketTypeInfo::fromid(this->header()[2] & 0xF);
        }


        std::unique_ptr<ClientPacket> ClientPacket::from_buffer(const pipes::buffer_view &buffer) {
            auto result = make_unique<ClientPacket>();

            result->_buffer = buffer.own_buffer();
            result->_header_length = CLIENT_HEADER_SIZE;

            return result;
        }

        ClientPacket::ClientPacket(const PacketTypeInfo &type, const pipes::buffer_view& data) : BasicPacket(CLIENT_HEADER_SIZE, data.length()) {
            this->header()[4] = type.type() & 0xF;
            memcpy(this->data().data_ptr(), data.data_ptr(), data.length());
        }


        ClientPacket::ClientPacket(const PacketTypeInfo &type, uint8_t flag_mask, const pipes::buffer_view& data) : ClientPacket(type, data) {
            this->header()[4] |= flag_mask;
        }

        ClientPacket::~ClientPacket() {}

        uint16_t ClientPacket::packetId() const {
            return be2le16(&this->header()[0]);
        }

        uint16_t ClientPacket::generationId() const {
            return this->genId;
        }

        PacketTypeInfo ClientPacket::type() const {
            return PacketTypeInfo::fromid(this->header()[4] & 0xF);
        }

        void ClientPacket::type(const ts::protocol::PacketTypeInfo &type) {
            auto& field = this->header().data_ptr<uint8_t>()[4];
            field &= (uint8_t) ~0xF;
            field |= type.type();
        }

        void ClientPacket::setPacketId(uint16_t pkId, uint16_t gen) {
            this->header()[0] = (uint8_t) ((pkId >> 8) & 0xFF);
            this->header()[1] = (uint8_t) ((pkId >> 0) & 0xFF);
            this->genId = gen;
        }

        uint16_t ClientPacket::clientId() const {
            return be2le16(&this->header()[2]);
        }

        void ClientPacket::clientId(uint16_t clId) {
            this->header()[2] = clId >> 8;
            this->header()[3] = clId & 0xFF;
        }


        /* New packet parser API */
        bool PacketParser::is_encrypted() const {
            if(this->decrypted) return false;

            return (this->flags() & PacketFlag::Unencrypted) == 0;
        }

        bool PacketParser::is_compressed() const {
            if(this->uncompressed) return false;

            return (this->flags() & PacketFlag::Compressed) > 0;
        }

        bool PacketParser::is_fragmented() const {
            if(this->defragmented) return false;

            return (this->flags() & PacketFlag::Fragmented) > 0;
        }

        uint16_t ClientPacketParser::packet_id() const { return be2le16(this->_buffer.data_ptr<uint8_t>(), ClientPacketParser::kHeaderOffset + 0); }
        uint16_t ClientPacketParser::client_id() const { return be2le16(this->_buffer.data_ptr<uint8_t>(), ClientPacketParser::kHeaderOffset + 2); }
        uint8_t ClientPacketParser::type() const { return (uint8_t) this->_buffer[ClientPacketParser::kHeaderOffset + 4] & 0xFU; }
        uint8_t ClientPacketParser::flags() const { return (uint8_t) this->_buffer[ClientPacketParser::kHeaderOffset + 4] & 0xF0U; }

        uint16_t ServerPacketParser::packet_id() const { return be2le16(this->_buffer.data_ptr<uint8_t>(), ClientPacketParser::kHeaderOffset + 0); }
        uint8_t ServerPacketParser::type() const { return (uint8_t) this->_buffer[ClientPacketParser::kHeaderOffset + 2] & 0xFU; }
        uint8_t ServerPacketParser::flags() const { return (uint8_t) this->_buffer[ClientPacketParser::kHeaderOffset + 2] & 0xF0U; }
    }
}