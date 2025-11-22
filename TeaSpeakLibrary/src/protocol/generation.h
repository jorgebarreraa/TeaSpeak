#pragma once

#include <cstdint>

namespace ts::protocol {
    class GenerationEstimator {
        public:
            GenerationEstimator();

            void reset();
            [[nodiscard]] uint16_t visit_packet(uint16_t /* packet id */);
            [[nodiscard]] inline uint16_t generation() const { return this->last_generation; }
            [[nodiscard]] inline uint16_t current_packet_id() const { return this->last_packet_id; }

            void set_last_state(uint16_t last_packet, uint16_t generation) {
                this->last_packet_id = last_packet;
                this->last_generation = generation;
            }
        private:
            constexpr static uint16_t overflow_window{1024 * 8};
            constexpr static uint16_t overflow_area_begin{0xFFFF - overflow_window};
            constexpr static uint16_t overflow_area_end{overflow_window};

            uint16_t last_generation{0};
            uint16_t last_packet_id{0};
    };
}