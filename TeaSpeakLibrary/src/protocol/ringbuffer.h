#pragma once

#include <cstddef>
#include <cstdint>
#include <cassert>
#include <utility>
#include <memory>
#include <mutex>

// Windows defines the macro max(a, b)
#ifdef max
    #undef max
#endif

namespace ts {
    namespace protocol {
        template <typename T>
        struct RingEntry {
            bool flag_set = false;
            T entry{};
        };

        template <typename E, size_t capacity_t, typename size_type>
        class RingBuffer {
            public:
                static constexpr size_type _max_index = ~((size_type) 0);

                RingBuffer() : _capacity(capacity_t) {
                    this->_ring_index_full = 0;
                    this->_ring_base_index = 0;
                }

                inline size_type current_index() { return this->_ring_index; }

                /**
                 * @param index
                 * @return -1 underflow | 0 success | 1 overflow
                 */
                inline int accept_index(size_type index) {
                    size_t relative_index;
                    if(this->calculate_index(index, relative_index))
                        return 0;
                    if(index < this->current_index())
                        return -1;
                    return 1;
                }

                inline bool index_set(size_type index) {
                    size_t relative_index = 0;
                    if(!this->calculate_index(index, relative_index))
                        return false;
                    return this->index(relative_index).flag_set;
                }

                inline E& index_value(size_type index) {
                    size_t relative_index = 0;
                    if(!this->calculate_index(index, relative_index))
                        return {};

                    return this->index(relative_index).entry;
                }

                inline size_t current_slot() { return this->_ring_base_index; }

                inline bool slot_set(size_t slot) {
                    return this->index(slot).flag_set;
                }

                inline E& slot_value(size_t slot) {
                    return this->index(slot).entry;
                }


                inline bool front_set() { return this->_slots[this->_ring_base_index].flag_set; }

                inline E pop_front() {
                    auto& slot = this->_slots[this->_ring_base_index];
                    slot.flag_set = false;
                    auto entry = std::move(slot.entry);
                    this->_ring_base_index += 1;
                    this->_ring_index_full += 1;
                    if(this->_ring_base_index >= this->_capacity)
                        this->_ring_base_index -= this->_capacity;
                    return entry;
                }

                inline void push_front(E&& entry) {
                    /* go to the back of the ring and set this as front */
                    if(this->_ring_base_index == 0)
                        this->_ring_base_index = (size_type) this->_capacity;

                    this->_ring_base_index -= 1;
                    this->_ring_index_full -= 1;

                    auto& slot = this->_slots[this->_ring_base_index];
                    slot.entry = std::forward<E>(entry);
                    slot.flag_set = 1;
                }

                inline bool push_back(E&& entry) {
                    size_t count = 0;
                    size_t index = this->_ring_base_index;
                    while(count < this->_capacity) {
                        if(index >= this->_capacity)
                            index -= this->_capacity;

                        auto& slot = this->_slots[index];
                        if(slot.flag_set) {
                            count++;
                            index++;
                            continue;
                        }

                        slot.entry = std::forward<E>(entry);
                        slot.flag_set = 1;
                        break;
                    }

                    return count != this->_capacity;
                }

                inline void set_full_index_to(size_type index) {
                    if(index > this->_ring_index)
                        this->_ring_index = index;
                    else if(index < 100 && this->_ring_index > std::numeric_limits<size_type>::max() - 100) {
                        this->_ring_index_full += 200; /* let the index overflow into the generation counter */
                        this->_ring_index = index; /* set the lower (16) bytes */
                    }
                }

                inline bool insert_index(size_type index, E&& entry) {
                    size_t relative_index = 0;
                    if(!this->calculate_index(index, relative_index))
                        return false;

                    auto& slot = this->index(relative_index);
                    if(slot.flag_set)
                        return false;

                    slot.entry = std::forward<E>(entry);
                    slot.flag_set = true;
                    return true;
                }

                inline size_t capacity() { return this->_capacity; }


                inline void clear() {
                    this->_ring_base_index = 0;
                    for(RingEntry<E>& element : this->_slots) {
                        element.flag_set = false;
                        (void) std::move(element.entry);
                    }
                }

                inline void reset() {
                    this->clear();
                    this->_ring_index_full = 0;
                }
            protected:
                size_t _capacity;
                size_t _ring_base_index; /* index of slot 0 */

                union {
                    uint64_t _ring_index_full;
                    struct {
                        static_assert(8 - sizeof(size_type) > 0, "Invalid size type!");

                        /* little endian */
                        size_type _ring_index; /* index of the insert index | overflow is welcome here */
                        uint8_t padding[8 - sizeof(size_type)];
                    };
                };

                std::array<RingEntry<E>, capacity_t> _slots;

                inline RingEntry<E>& index(size_t relative_index) {
                    assert(relative_index < this->_capacity);
                    auto index = this->_ring_base_index + relative_index;
                    if(index >= this->_capacity)
                        index -= this->_capacity;
                    return this->_slots[index];
                }

                inline bool calculate_index(size_type index, size_t& relative_index) {
                    if(this->_ring_index > index) { /* test if index is an overflow of the counter */
                        if(index >= (size_type) (this->_ring_index + this->_capacity)) /* not anymore in bounds */
                            return false;
                        else
                            relative_index = index + (_max_index - this->_ring_index + 1);
                        if(relative_index >= this->_capacity)
                            return false;
                    } else if(this->_ring_index < index) {
                        /* check upper bounds */
                        relative_index = index - this->_ring_index;
                        if(relative_index >= this->_capacity)
                            return false;
                    } else {
                        /* index is equal, do nothing */
                        relative_index = 0;
                    }
                    return true;
                }
        };

        template <typename E, uint16_t SIZE = 32, typename PTR_TYPE = std::shared_ptr<E>>
        class PacketRingBuffer : public RingBuffer<PTR_TYPE, SIZE, uint16_t> {
            public:
                std::recursive_timed_mutex buffer_lock;
                std::recursive_timed_mutex execute_lock;
        };
    }
}