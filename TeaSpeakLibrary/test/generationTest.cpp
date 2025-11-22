#include <src/protocol/generation.h>
#include <iostream>
#include <array>
#include <vector>

using namespace ts::protocol;


typedef std::vector<std::pair<size_t, size_t>> test_vector_t;

test_vector_t generate_test_vector(size_t size, int loss, int step = 1) {
    test_vector_t result{};
    result.reserve(size);


    for(size_t i = 0; i < size; i++) {
        if ((rand() % 100) < loss) continue;
        result.emplace_back((i * step) & 0xFFFFU, (i * step) >> 16U);
    }

    return result;
}

test_vector_t swap_elements(test_vector_t vector, int per, int max_distance) {
    for(size_t index = 0; index < vector.size() - max_distance; index++) {
        if ((rand() % 100) < per) {
            //lets switch
            auto offset = rand() % max_distance;
            std::swap(vector[index], vector[index + offset]);
        }
    }

    return vector;
}

bool test_vector(const std::string_view& name, const test_vector_t& vector) {
    GenerationEstimator gen{};

    size_t last_value{0}, last_gen{0}, index{0};
    for(auto [id, exp] : vector) {
        if(auto val = gen.visit_packet(id); val != exp) {
            std::cout << "[" << name << "] failed for " << id << " -> " << exp << " | " << val << ". Last value: " << last_value << " gen: " << last_gen << "\n";
            return false;
        } else
            last_gen = val;
        last_value = id;
        index++;
    }
    return true;
}

template <size_t N>
bool test_vector(GenerationEstimator& generator, const std::array<uint16_t, N>& packet_ids, const std::array<uint16_t, N>& expected) {
    for(size_t index = 0; index < N; index++) {
        auto result = generator.visit_packet(packet_ids[index]);
        if(result != expected[index]) {
            std::cout << "failed to packet id " << packet_ids[index] << " (" << index << "). Result: " << result << " Expected: " << expected[index] << "\n";
            std::cout << "----- fail\n";
            return false;
        }

        std::cout << "PacketID: " << packet_ids[index] << " -> " << result << "\n";
    }
    std::cout << "----- pass\n";

    return true;
}

int main() {
    GenerationEstimator gen{};

    {
        test_vector("00 loss", generate_test_vector(0x30000, 0));
        test_vector("10 loss", generate_test_vector(0x30000, 10));
        test_vector("25 loss", generate_test_vector(0x30000, 25));
        test_vector("50 loss", generate_test_vector(0x30000, 50));
        test_vector("80 loss", generate_test_vector(0x30000, 80));
        test_vector("99 loss", generate_test_vector(0x30000, 99));
    }

    {
        auto base = generate_test_vector(0x30000, 0);
        test_vector("swap 30:4", swap_elements(base, 30, 4));
        test_vector("swap 30:20", swap_elements(base, 30, 20));
        test_vector("swap 30:1000", swap_elements(base, 30, 200));
        test_vector("swap 80:1000", swap_elements(base, 80, 200));
    }

    {
        test_vector("1k step", swap_elements(generate_test_vector(0x30000, 0, 6), 100, 8));

    }

    if(true) {
#if 0
        gen.set_last_state(0, 0);
        test_vector<6>(gen, {0, 1, 2, 4, 3, 5}, {0, 0, 0, 0, 0, 0});

        gen.set_last_state(0xFF00, 0);
        test_vector<6>(gen, {0, 1, 2, 4, 3, 5}, {1, 1, 1, 1, 1, 1});

        gen.set_last_state(0xFF00, 0);
        test_vector<6>(gen, {0, 1, 2, 0xFF00, 3, 5}, {1, 1, 1, 0, 1, 1});

        gen.set_last_state(0xFF00, 0);
        test_vector<6>(gen, {0xFFFE, 0xFFFF, 0, 1, 0xFFFC, 2}, {0, 0, 1, 1, 0, 1});
#endif

        gen.set_last_state(0xFF00, 1);
        test_vector<6>(gen, {0xFFFE, 0xFFFF, 0, 1, 0xFFFC, 2}, {1, 1, 2, 2, 1, 2});
    }
}