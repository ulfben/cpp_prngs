#include <array>
#include <chrono>
#include <cstdint>
#include <random>
#include <string>
#include <iostream>
#include <bit>
#include <limits>
#include <algorithm>
#include <optional>
#include <cassert>
#include "/engines/romuduojr.hpp"

// ULID (Universally Unique Lexicographically Sortable Identifier) is a 128-bit
// identifier format with the following layout:
//
//   - 48 bits: millisecond timestamp since Unix epoch
//   - 80 bits: randomness
//
// Encoded in Crockford Base32 it becomes a 26 character string that is
// lexicographically sortable in the same order as its timestamp. This makes
// ULIDs useful as human-friendly, time-orderable identifiers for logs,
// database keys, filenames, etc.
// 
// I was inspired by Marius Bancila's article on the subject: 
//   https://mariusbancila.ro/blog/2025/11/27/universally-unique-lexicographically-sortable-identifiers-ulids/
//
// This header provides:
//
//   - ulid_t::generate()
//       Generates a ULID using the current time and a per-thread RomuDuoJr
//       PRNG. The result is time-ordered at millisecond precision, but
//       multiple IDs within the same millisecond are not guaranteed to be
//       strictly monotonic.
//
//   - ulid_t::generate_monotonic()
//       Generates a per-thread monotonic ULID sequence. Within each thread,
//       IDs are strictly increasing in lexicographic order, even when many
//       IDs are created in the same millisecond or when the system clock
//       moves backwards. 
//
//   - ulid_t::from_string()
//       Parses a 26 character Crockford Base32 ULID string into a ulid_t.
//       Returns std::nullopt if the string is invalid or non-canonical.
//
//   - ulid_t::to_string()
//       Encodes a ulid_t into its canonical 26 character Crockford Base32
//       representation.
//
// Monotonicity is per thread only: there is no cross-thread coordination,
// no locking, and no global ordering between threads.

struct ulid_t final {
    using byte = std::uint8_t;

    [[nodiscard]] constexpr static ulid_t generate() noexcept {        
        const std::uint64_t ts = now_ms();        
        static thread_local auto rng = RomuDuoJr{ ts ^ std::random_device{}() };
        static thread_local std::uniform_int_distribution<byte> dist(0, 0xFF);
        ulid_t ulid{};
        write_be<6>(ts, ulid.timestamp_bytes()); //fill timestamp bytes (big-endian)       
        std::ranges::generate(ulid.random_bytes(), [&] { return dist(rng); }); //fill random bytes
        return ulid;
    }

    [[nodiscard]] constexpr static ulid_t generate_monotonic() {                
        auto ts = now_ms();
        static thread_local auto rng = RomuDuoJr{ ts ^ std::random_device{}() };
        static thread_local std::uniform_int_distribution<byte> dist(0, 0xFF);
        static thread_local std::uint64_t last_ts = 0;
        static thread_local std::array<byte, 10> last_rand{};
        static thread_local bool have_last = false;

        if (!have_last || ts > last_ts) { // new millisecond: draw fresh randomness            
            last_ts = ts;
            std::ranges::generate(last_rand, [&] { return dist(rng); });
            have_last = true;
        } else { // same millisecond OR clock went backwards.                        
            if (ts < last_ts) { ts = last_ts; } // for monotonicity, pin timestamp to last_ts 
            increment_big_endian(last_rand); // and bump the random field.
        }        
        ulid_t ulid{};        
        write_be<6>(ts, ulid.timestamp_bytes()); //fill the timestamp 
        std::ranges::copy(last_rand, ulid.random_bytes().begin()); //and the random section
        return ulid;
    }

     [[nodiscard]] constexpr static std::optional<ulid_t> from_string(std::string_view s) noexcept {
        if (s.size() != 26) { return std::nullopt; }        
        std::array<std::uint64_t, 3> acc{}; // 192-bit accumulator: acc[0] = least significant 64 bits        
        for (char ch : s) {
            auto v_opt = decode_crockford(ch);
            if (!v_opt) { return std::nullopt; }
            const std::uint64_t v = *v_opt; // 0..31            
            std::uint64_t carry = v;
            for (std::size_t i = 0; i < acc.size(); ++i) {
                const std::uint64_t new_carry = acc[i] >> (64 - 5);
                acc[i] = (acc[i] << 5) | carry;
                carry = new_carry;
            }            
        }
        // Optional canonicality check: top 2 bits of 130-bit value should be zero.
        // Those are bits 128 and 129, which correspond to the lowest 2 bits of acc[2].        
        if ((acc[2] & 0x3u) != 0) {
            return std::nullopt;
        }               
        ulid_t ulid{};
        write_be<8>(acc[1], ulid.high_bytes()); // high bits are in acc[1] (bits 64..127)
        write_be<8>(acc[0], ulid.low_bytes()); // low bits are in acc[0] (bits 0..63)
        return ulid;
    }

    [[nodiscard]] constexpr std::string to_string() const {
        return encode_base32(data);
    }

    [[nodiscard]] constexpr explicit operator std::string() const {
        return to_string();
    }   

    constexpr auto operator<=>(const ulid_t&) const = default;

private:
    static constexpr char ENCODING[32] = {
        '0','1','2','3','4','5','6','7','8','9',
        'A','B','C','D','E','F','G','H','J','K',
        'M','N','P','Q','R','S','T','V','W','X',
        'Y','Z'
    };
    
    std::array<byte, 16> data{};  

    constexpr std::span<byte, 6> timestamp_bytes() noexcept {
        return {data.begin(), 6};
    }
    constexpr std::span<byte, 10> random_bytes() noexcept {        
        return {timestamp_bytes().end(), 10};
    }
    constexpr std::span<byte, 8> high_bytes() noexcept {
        return {data.begin(), 8};
    }
    constexpr std::span<byte, 8> low_bytes() noexcept {
        return {high_bytes().end(), 8};
    }

    constexpr static std::uint64_t now_ms() noexcept {
        using namespace std::chrono;
        return duration_cast<milliseconds>(
                   system_clock::now().time_since_epoch()
               ).count();
    }  

    constexpr static std::string encode_base32(std::span<const byte, 16> bytes) {
        std::string out(26, '0');
        std::uint32_t bitpos = 128 + 2;
        for (int i = 0; i < 26; ++i) {
            bitpos -= 5;
            const auto value = extract_5bits(bytes, bitpos);
            out[i] = ENCODING[value];
        }
        return out;
    }

    constexpr static std::uint32_t extract_5bits(std::span<const byte, 16> bytes,
                                       std::uint32_t bitpos) noexcept {
        const std::uint32_t byteIndex = bitpos >> 3;
        const std::uint32_t startBit  = bitpos & 7U;
        const std::uint32_t first  = bytes[byteIndex];
        const std::uint32_t second = (byteIndex + 1U < 16U)
            ? bytes[byteIndex + 1U]
            : 0U;
        std::uint32_t v = (first << 8) | second;
        v >>= (11U - startBit);
        return v & 0x1FU;
    }

    constexpr static void increment_big_endian(std::span<byte, 10> rand) noexcept {       
        for (auto it = rand.rbegin(); it != rand.rend(); ++it) { // ULID is big-endian, so increment from least significant byte (back)
            if (*it != 0xFF) {
                ++(*it);
                return;
            }
            *it = 0;
        }
        // If we get here, we overflowed 80 bits (all 0xFF -> all 0x00).
        // Monotonicity within that millisecond is technically broken,
        // but if you're greedy enough to take 2^80 IDs/ms ... you deserve it. :P
    }

    //helper for extracting bytes in big-endian order
    template<std::size_t N>
    constexpr static void write_be(std::uint64_t value, std::span<byte, N> out) noexcept {
        static_assert(N <= 8);
        for (std::size_t i = 0; i < N; ++i) {
            out[i] = static_cast<byte>((value >> ((N - 1 - i) * 8)) & 0xFF);
        }
    }

    constexpr static std::optional<std::uint8_t> decode_crockford(char c) noexcept {
        switch (c) {        
        case '0': case 'O': case 'o': return 0;
        case '1': case 'I': case 'i': case 'L': case 'l': return 1;
        case '2': return 2;
        case '3': return 3;
        case '4': return 4;
        case '5': return 5;
        case '6': return 6;
        case '7': return 7;
        case '8': return 8;
        case '9': return 9;        
        case 'A': case 'a': return 10;
        case 'B': case 'b': return 11;
        case 'C': case 'c': return 12;
        case 'D': case 'd': return 13;
        case 'E': case 'e': return 14;
        case 'F': case 'f': return 15;
        case 'G': case 'g': return 16;
        case 'H': case 'h': return 17;
        case 'J': case 'j': return 18;
        case 'K': case 'k': return 19;
        case 'M': case 'm': return 20;
        case 'N': case 'n': return 21;
        case 'P': case 'p': return 22;
        case 'Q': case 'q': return 23;
        case 'R': case 'r': return 24;
        case 'S': case 's': return 25;
        case 'T': case 't': return 26;
        case 'V': case 'v': return 27;
        case 'W': case 'w': return 28;
        case 'X': case 'x': return 29;
        case 'Y': case 'y': return 30;
        case 'Z': case 'z': return 31;
        default:
            return std::nullopt;
        }
    }
};
