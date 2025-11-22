#pragma once

#include <cstdio>
#include <cstring>
#include <array>
#include <string_view>

namespace str_obf {
    constexpr static auto max_key_power = 6; /* 64 bytes max */

    namespace internal {
        template <typename char_t, std::uint64_t buffer_size, std::uint64_t size, typename key_t>
        struct message {
            /* helper to access the types */
            static constexpr auto _size = size;
            using _char_t = char_t;
            using _key_t = key_t;

            /* memory */
            std::array<char_t, buffer_size> buffer{};
            key_t key{};

            /* some memory access helpers */
            [[nodiscard]] std::string_view string_view() const noexcept { return {this->buffer.begin(), this->length}; }
            [[nodiscard]] std::string string() const { return {this->buffer.begin(), this->length}; }
            [[nodiscard]] const char* c_str() const noexcept { return &this->buffer[0]; }
        };

        constexpr auto time_seed() noexcept {
            std::uint64_t shifted = 0;

            for(const auto c : __TIME__)
            {
                shifted <<= 8U;
                shifted |= (unsigned) c;
            }

            return shifted;
        }

        constexpr uint64_t string_hash(const char* str, int h = 0) noexcept {
            return !str[h] ? 5381 : (unsigned) (string_hash(str, h + 1) * 33) ^ (unsigned) str[h];
        }

#ifdef WIN32
#pragma warning(disable: 4146) // unary minus operator applied to unsigned type, result still unsigned
#endif
        constexpr std::uint32_t rng32_next(std::uint64_t& state, const std::uint32_t& inc) noexcept {
            std::uint64_t oldstate = state;
            // Advance internal state
            state = oldstate * 6364136223846793005ULL + (inc | 1UL);
            // Calculate output function (XSH RR), uses old state for max ILP
            std::uint32_t xorshifted = (uint32_t) (((oldstate >> 18u) ^ oldstate) >> 27u);
            std::uint32_t rot = oldstate >> 59u;
            return (xorshifted >> rot) | (xorshifted << ((-rot) & 31));
        }
#ifdef WIN32
#pragma warning(default: 4146) // unary minus operator applied to unsigned type, result still unsigned
#endif

        /* we use a buffer dividable by 8 so the compiler could do crazy shit, when loading (moving) the characters */
        constexpr std::uint64_t recommand_message_buffer(std::uint64_t message_size) noexcept {
            if(message_size <= 4) return 4; /* we could use the eax register here */
            return (message_size & 0xFFFFFFF8) + ((message_size & 0x7) > 0 ? 8 : 0);
        }
    }

    inline void _invalid_key_size() {}

    template <typename char_t, typename key_t>
    constexpr inline void crypt(char_t* begin, std::uint64_t length, const key_t& key) noexcept {
        static_assert(sizeof(char_t) == 1, "Currently only 8 bit supported");
        if(length == 0) return;
        if(key.size() == 0) _invalid_key_size();
        if(key.size() & (key.size() - 1UL)) _invalid_key_size(); /* key must be an power of 2 */

        auto left = length;
        size_t key_index{0};
        while(left-- > 0) {
            key_index &= key.size();
            *begin ^= key[key_index + key_index];
        }
    }

    template <typename char_t, std::uint64_t message_size, typename key_t>
    constexpr inline auto encode(const char_t(&message)[message_size], const key_t& key) noexcept {
        constexpr auto message_buffer_size = internal::recommand_message_buffer(message_size);
        internal::message<char_t, message_buffer_size, message_size, key_t> result{};
        result.key = key;

        {
            auto bit = result.buffer.begin();
            auto mit = message;

            std::uint64_t index = message_size;
            while(index-- > 0)
                *bit++ = *mit++;

            std::uint64_t padding = message_buffer_size - message_size;
            if(padding) { /* to make the string end less obvious we add some noise here (it does not harm user performance) */
                std::uint64_t rng_seed = internal::time_seed() ^ internal::string_hash(message, 0);
                std::uint64_t rng_base = rng_seed;
                while(padding-- > 0)
                    *bit++ = internal::rng32_next(rng_base, (uint32_t) rng_seed) & 0xFFUL;
            }
        }

        crypt<char_t, key_t>(result.buffer.data(), message_size, key);
        return result;
    }

    template <typename char_t, std::uint64_t length>
    constexpr inline auto str_length(const char_t(&message)[length]) noexcept { return length; }

    template <typename char_t, std::uint64_t buffer_size, std::uint64_t message_size, typename key_t>
    inline std::string decode(const internal::message<char_t, buffer_size, message_size, key_t>& message) {
        std::string result{};
        result.resize(message_size);

        memcpy(result.data(), message.buffer.begin(), message_size);
        crypt(result.data(), message_size, message.key);

        return result;
    }

    /* length is a power of 2! */
    constexpr inline std::uint64_t generate_key_length(std::uint64_t seed, std::uint64_t message_size) noexcept {
        if(message_size <= 1) return 1;

        size_t power2{0};
        while(message_size >>= 1U)
            power2++;

        if(power2 > max_key_power)
            power2 = max_key_power;

        std::uint64_t rng_base = seed;
        std::uint64_t length = 0;
        do {
            length = (std::uint64_t) ((internal::rng32_next(rng_base, (uint32_t) seed) >> 12UL) & 0xFFUL);
        } while(length == 0 || length > power2);

        return uint64_t{1} << length;
    }

    template <uint64_t line_number, std::uint64_t message_size>
    constexpr inline auto generate_key(const char* _str_seed) noexcept {
        std::uint64_t rng_seed = internal::time_seed() ^ internal::string_hash(_str_seed, 0) ^ line_number;
        std::uint64_t rng_base = rng_seed;

        constexpr std::uint64_t key_length = generate_key_length(internal::time_seed() ^ (line_number << 37UL), message_size);
        std::array<uint8_t, key_length> result{};
        for(auto& it : result)
            it = (internal::rng32_next(rng_base, (uint32_t) rng_seed) >> 16UL) & 0xFFUL;
        return result;
    }

    template <typename message>
    struct decode_helper {
        const message& encoded;
        std::array<typename message::_char_t, message::_size> buffer{0};
        bool decoded = false; /* a trivial check which (if this only gets used once) the compiler could evaluate */

#ifndef _MSC_VER /* else if you call string_view() or string() it wound inline this method */
        __attribute__((always_inline)) inline
#else
        __forceinline
#endif
        const char* c_str() noexcept  {
            if(!this->decoded) {
                memcpy(this->buffer.data(), this->encoded.buffer.data(), message::_size);
                crypt<
                        typename message::_char_t,
                        typename message::_key_t
                >((typename message::_char_t*) &this->buffer[0], message::_size, this->encoded.key);
                this->decoded = true;
            }

            return &this->buffer[0];
        }

        inline std::string_view string_view() noexcept  {
            return {this->c_str(), message::_size - 1};
        }

        inline std::string string() { return {this->c_str(), message::_size - 1}; }

        //operator const char*() noexcept { return this->c_str(); }
    };
}

#define strobf_define(variable_name, string) \
constexpr auto variable_name = ::str_obf::encode(string, str_obf::generate_key<__LINE__, str_obf::str_length(string)>(__FILE__ __TIME__))

#define strobf_val(variable_name) (str_obf::decode_helper<decltype(variable_name)>{variable_name})

#define strobf(message) \
(([]{ \
    static strobf_define(_, message); \
    return strobf_val(_); \
})())
