#pragma once

#include <cstdint>
#include <string_view>
#include <pipes/buffer.h>

namespace ts::command {
    struct CommandFragment {
        uint16_t packet_id{0};
        uint16_t packet_generation{0};

        uint8_t packet_flags{0};
        uint32_t payload_length : 24;
        pipes::buffer payload{};

        CommandFragment() : payload_length{0} { }
        CommandFragment(uint16_t packetId, uint16_t packetGeneration, uint8_t packetFlags, uint32_t payloadLength, pipes::buffer payload)
                : packet_id{packetId}, packet_generation{packetGeneration}, packet_flags{packetFlags}, payload_length{payloadLength}, payload{std::move(payload)} {}

        CommandFragment& operator=(const CommandFragment&) = default;
        CommandFragment(const CommandFragment& other) = default;
        CommandFragment(CommandFragment&&) = default;
    };

    /* Windows aligns stuff somewhat different */
#ifndef WIN32
    static_assert(sizeof(CommandFragment) == 8 + sizeof(pipes::buffer));
#endif

    struct ReassembledCommand {
        public:
            static ReassembledCommand* allocate(size_t /* command length */);
            static void free(ReassembledCommand* /* command */);

            [[nodiscard]] inline size_t length() const { return this->length_; }
            inline void set_length(size_t length) { assert(this->capacity_ >= length); this->length_ = length; }

            [[nodiscard]] inline size_t capacity() const { return this->capacity_; }

            [[nodiscard]] inline const char* command() const { return (const char*) this + sizeof(ReassembledCommand); }
            [[nodiscard]] inline char* command() { return (char*) this + sizeof(ReassembledCommand); }

            [[nodiscard]] inline std::string_view command_view() const { return std::string_view{this->command(), this->length()}; }

            mutable ReassembledCommand* next_command; /* nullptr by default */
        private:
            explicit ReassembledCommand() = default;

            size_t capacity_;
            size_t length_;
    };
}