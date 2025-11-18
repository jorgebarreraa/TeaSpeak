#include "./PrecomputedPuzzles.h"
#include "..//Configuration.h"
#include <tomcrypt.h>

using namespace std;
using namespace ts::server::udp;

Puzzle::Puzzle() {
    mp_init_multi(&this->x, &this->n, &this->result, nullptr);
}

Puzzle::~Puzzle() {
    mp_clear_multi(&this->x, &this->n, &this->result, nullptr);
}

PuzzleManager::PuzzleManager() = default;
PuzzleManager::~PuzzleManager() = default;

size_t PuzzleManager::precomputed_puzzle_count() {
    std::lock_guard lock{this->cache_mutex};
    return this->cached_puzzles.size();
}

bool PuzzleManager::precompute_puzzles(size_t amount) {
    std::random_device rd{};
    std::mt19937 mt{rd()};

    amount = 5;
    while(this->precomputed_puzzle_count() < amount) {
        this->generate_puzzle(mt);
    }

    return this->precomputed_puzzle_count() > 0;
}

std::shared_ptr<Puzzle> PuzzleManager::next_puzzle() {
    {
        std::lock_guard lock{this->cache_mutex};
        auto it = this->cached_puzzles.begin() + (this->cache_index++ % this->cached_puzzles.size());
        if((*it)->fail_count > 2) {
            this->cached_puzzles.erase(it);
        } else {
            return *it;
        }
    }

    std::random_device rd{};
    std::mt19937 mt{rd()};
    this->generate_puzzle(mt);
    return this->next_puzzle();
}

inline void random_number(std::mt19937& generator, mp_int *result, int length){
    std::uniform_int_distribution<uint8_t> dist{};

    uint8_t buffer[length];
    for(auto& byte : buffer) {
        byte = dist(generator);
    }

    mp_zero(result);
    mp_read_unsigned_bin(result, buffer, length);
}

inline bool solve_puzzle(Puzzle *puzzle) {
    mp_int exp{};
    mp_init(&exp);
    mp_2expt(&exp, puzzle->level);

    if (mp_exptmod(&puzzle->x, &exp, &puzzle->n, &puzzle->result) != CRYPT_OK) { //Sometimes it fails (unknown why :D)
        mp_clear(&exp);
        return false;
    }

    mp_clear(&exp);
    return true;
}

inline bool write_bin_data(mp_int& data, uint8_t* result, size_t length) {
    ulong n{length};
    if(auto err = mp_to_unsigned_bin_n(&data, result, &n); err) {
        return false;
    }

    if(n != length) {
        auto off = length - n;
        memmove(result + off, result, n);
        memset(result, 0, off);
    }
    return true;
}

void PuzzleManager::generate_puzzle(std::mt19937& random_generator) {
    auto puzzle = std::make_shared<Puzzle>();
    puzzle->level = ts::config::voice::RsaPuzzleLevel;

    while(true) {
#if 0
        random_number(random_generator, &puzzle->x, 64);
        random_number(random_generator, &puzzle->n, 64);
#else
        mp_set(&puzzle->x, 1);
        mp_set(&puzzle->n, 1);
#endif
        if(!solve_puzzle(&*puzzle)) {
            continue;
        }

        auto valid_x = mp_unsigned_bin_size(&puzzle->x) <= 64;
        auto valid_n = mp_unsigned_bin_size(&puzzle->n) <= 64;
        auto valid_result = mp_unsigned_bin_size(&puzzle->result) <= 64;
        if(!valid_n || !valid_x || !valid_result) {
            continue;
        }

        if(!write_bin_data(puzzle->x, puzzle->data_x, 64)) {
            continue;
        }

        if(!write_bin_data(puzzle->n, puzzle->data_n, 64)) {
            continue;
        }

        if(!write_bin_data(puzzle->result, puzzle->data_result, 64)) {
            continue;
        }

        /* everything seems to be good */
        break;
    }
    this->cached_puzzles.push_back(std::move(puzzle));
}