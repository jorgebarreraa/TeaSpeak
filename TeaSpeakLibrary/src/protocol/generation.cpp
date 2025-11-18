#include "./generation.h"

using namespace ts::protocol;

generation_estimator::generation_estimator() {
    this->reset();
}

void generation_estimator::reset() {
    this->last_generation = 0;
    this->last_packet_id = 0;
}

uint16_t generation_estimator::visit_packet(uint16_t packet_id) {
    if(this->last_packet_id >= generation_estimator::overflow_area_begin) {
        if(packet_id > this->last_packet_id) {
            /* normal behaviour */
            this->last_packet_id = packet_id;
            return this->last_generation;
        } else if(packet_id < generation_estimator::overflow_area_end) {
            /* we're within a new generation */
            this->last_packet_id = packet_id;
            return ++this->last_generation;
        } else {
            /* old packet */
            return this->last_generation;
        }
    } else if(this->last_packet_id <= generation_estimator::overflow_area_end) {
        if(packet_id >= generation_estimator::overflow_area_begin) /* old packet */
            return this->last_generation - 1;
        if(packet_id > this->last_packet_id)
            this->last_packet_id = packet_id;
        return this->last_generation;
    } else {
        /* only update on newer packet id */
        if(packet_id > this->last_packet_id)
            this->last_packet_id = packet_id;
        return this->last_generation;
    }
}