#pragma once

#include <tommath.h>
#include <memory>
#include <mutex>
#include <vector>
#include <misc/spin_mutex.h>
#include <random>

namespace ts::server::udp {
    struct Puzzle {
        mp_int x{};
        mp_int n{};
        int level{0};

        mp_int result{};

        uint8_t data_x[64]{0};
        uint8_t data_n[64]{0};
        uint8_t data_result[64]{0};

        size_t fail_count{0};

        Puzzle();
        ~Puzzle();
    };

    class PuzzleManager {
        public:
            PuzzleManager();
            ~PuzzleManager();

            [[nodiscard]] bool precompute_puzzles(size_t amount);

            [[nodiscard]] size_t precomputed_puzzle_count();

            [[nodiscard]] std::shared_ptr<Puzzle> next_puzzle();
        private:
            void generate_puzzle(std::mt19937&);

            size_t cache_index{0};
            std::mutex cache_mutex{};
            std::vector<std::shared_ptr<Puzzle>> cached_puzzles{};
    };
}