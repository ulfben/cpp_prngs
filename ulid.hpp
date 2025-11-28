#pragma once
#include "random.hpp" //grab from: https://github.com/ulfben/cpp_prngs/
#include "romuduojr.hpp" //grab from: https://github.com/ulfben/cpp_prngs/
#include <algorithm>
#include <array>
#include <chrono>
#include <compare>
#include <cstdint>
#include <optional>
#include <random>
#include <span>
#include <string>
#include <string_view>
#include <ostream>

// Source: https://github.com/ulfben/cpp_prngs/blob/main/ulid.hpp
// ULID (Universally Unique Lexicographically Sortable Identifier) is fundamentally:
// - a 128-bit unsigned integer
// - serialized to 16 numeric bytes
// - encoded using Crockford base32
//
// The 128-bit are laid out thusly:
//   - 48 bits: millisecond timestamp since Unix epoch
//   - 80 bits: randomness
//
// Encoded in Crockford Base32 it becomes a 26 character string that is
// lexicographically sortable in the same order as its timestamp. This makes
// ULIDs useful as human-friendly, time-orderable identifiers for logs,
// database keys, filenames, etc.
// 
// This header provides:
//
//   - ulid_t::generate()
//       Generates a ULID using the current time and a per-thread PRNG. 
//		 The result is time-ordered at millisecond precision, but multiple 
//		 IDs within the same millisecond are not guaranteed to be
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
//
// Many thanks to Marius Bancila for the inspiration! 
//   https://mariusbancila.ro/blog/2025/11/27/universally-unique-lexicographically-sortable-identifiers-ulids/

class ulid_t final{
public:
	using byte = std::uint8_t;
	using Random = rnd::Random<RomuDuoJr>;

	[[nodiscard]] static ulid_t generate() noexcept{
		const auto ts = now_ms();
		static thread_local auto rng = Random{ts ^ std::random_device{}()};
		ulid_t ulid{};
		write_be<6>(ts, ulid.timestamp_bytes()); //fill timestamp bytes (big-endian)       
		std::ranges::generate(ulid.random_bytes(), [&]{ return rng.bits<8, byte>(); }); //fill random bytes
		return ulid;
	}

	[[nodiscard]] static ulid_t generate_monotonic() noexcept{
		auto ts = now_ms();
		static thread_local auto rng = Random{ts ^ std::random_device{}()};
		static thread_local std::uint64_t last_ts = 0;
		static thread_local std::array<byte, 10> last_rand{};
		static thread_local bool have_last = false;

		if(!have_last || ts > last_ts){ // new millisecond: draw fresh randomness            
			last_ts = ts;
			std::ranges::generate(last_rand, [&]{ return rng.bits<8, byte>(); });
			have_last = true;
		} else{ // same millisecond OR clock went backwards.                        
			if(ts < last_ts){ ts = last_ts; } // for monotonicity, pin timestamp to last_ts 
			increment_big_endian(last_rand); // and bump the random field.
		}
		ulid_t ulid{};
		write_be<6>(ts, ulid.timestamp_bytes()); //fill the timestamp 
		std::ranges::copy(last_rand, ulid.random_bytes().begin()); //and the random section
		return ulid;
	}

	[[nodiscard]] constexpr static std::optional<ulid_t> from_string(std::string_view s) noexcept{
		if(s.size() != 26){ return std::nullopt; }
		std::array<std::uint64_t, 3> acc{}; // 192-bit accumulator: acc[0] = least significant 64 bits        
		for(char ch : s){
			auto v_opt = decode_crockford(ch);
			if(!v_opt){ return std::nullopt; }
			const std::uint64_t v = *v_opt; // 0..31            
			std::uint64_t carry = v;
			for(std::size_t i = 0; i < acc.size(); ++i){
				const std::uint64_t new_carry = acc[i] >> (64 - 5);
				acc[i] = (acc[i] << 5) | carry;
				carry = new_carry;
			}
		}
		// Optional canonicality check: top 2 bits of 130-bit value should be zero.
		// Those are bits 128 and 129, which correspond to the lowest 2 bits of acc[2].        
		if((acc[2] & 0x3u) != 0){
			return std::nullopt;
		}
		ulid_t ulid{};
		write_be<8>(acc[1], ulid.high_bytes()); // high bits are in acc[1] (bits 64..127)
		write_be<8>(acc[0], ulid.low_bytes()); // low bits are in acc[0] (bits 0..63)
		return ulid;
	}

	[[nodiscard]] constexpr std::string to_string() const{
		return encode_base32(data);
	}

	[[nodiscard]] constexpr explicit operator std::string() const{
		return to_string();
	}

	[[nodiscard]] constexpr std::array<byte, 16> to_bytes() const noexcept{
		return data;
	}

	[[nodiscard]] constexpr static ulid_t from_bytes(std::span<const byte, 16> bytes) noexcept{
		ulid_t ulid{};
		std::ranges::copy(bytes, ulid.data.begin());
		return ulid;
	}

	[[nodiscard]] constexpr std::uint64_t timestamp_ms() const noexcept{		
		std::uint64_t ts = 0;
		for(int i = 0; i < 6; ++i){ // ULID stores a 48-bit big-endian timestamp in data[0..5]
			ts = (ts << 8) | static_cast<std::uint64_t>(data[i]);
		}
		return ts;
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

	constexpr std::span<byte, 6> timestamp_bytes() noexcept{
		return std::span<byte, 6>{data.begin(), 6};
	}
	constexpr std::span<byte, 10> random_bytes() noexcept{
		return std::span<byte, 10>{data.begin() + 6, 10};
	}
	constexpr std::span<byte, 8> high_bytes() noexcept{
		return std::span<byte, 8>{data.begin(), 8};
	}
	constexpr std::span<byte, 8> low_bytes() noexcept{
		return std::span<byte, 8>{data.begin() + 8, 8};
	}

	static std::uint64_t now_ms() noexcept{
		using namespace std::chrono;
		return static_cast<std::uint64_t>(
			duration_cast<milliseconds>(
				system_clock::now().time_since_epoch()
			).count()
		);
	}

	constexpr static std::string encode_base32(std::span<const byte, 16> bytes){
		// interpret the 16 bytes as a single 128-bit big-endian integer: N = (hi << 64) | lo
		std::uint64_t hi = 0;
		for(int i = 0; i < 8; ++i){
			hi = (hi << 8) | bytes[i];
		}
		std::uint64_t lo = 0;
		for(int i = 8; i < 16; ++i){
			lo = (lo << 8) | bytes[i];
		}
		// we want 26 digits, each is 5 bits, covering bits 125..0 of the 128-bit value.
		std::string out(26, '0');
		for(int i = 0; i < 26; ++i){			
			const auto digit = extract_digit(hi, lo, i);
			out[i] = ENCODING[digit];
		}
		return out;
	}

	constexpr static std::uint32_t extract_digit(std::uint64_t hi, std::uint64_t lo, int index) noexcept{
		const int shift = 125 - (5 * index); // bit index of MSB of this 5-bit digit
		if(shift == 0){ // Lowest 5 bits of N
			return static_cast<std::uint32_t>(lo & 0x1Fu);
		} else if(shift < 64){ // digit spans (possibly) across hi/lo; use the low 64 bits of (N >> shift)
			const std::uint64_t part = (hi << (64 - shift)) | (lo >> shift);
			return static_cast<std::uint32_t>(part & 0x1Fu);
		}
		return static_cast<std::uint32_t>((hi >> (shift - 64)) & 0x1Fu); // shift in [64, 125], only hits hi		 	
	}

	constexpr static void increment_big_endian(std::span<byte, 10> rand) noexcept{
		for(auto it = rand.rbegin(); it != rand.rend(); ++it){ // ULID is big-endian, so increment from least significant byte (back)
			if(*it != 0xFF){
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
	constexpr static void write_be(std::uint64_t value, std::span<byte, N> out) noexcept{
		static_assert(N <= 8);
		for(std::size_t i = 0; i < N; ++i){
			out[i] = static_cast<byte>((value >> ((N - 1 - i) * 8)) & 0xFF);
		}
	}

	constexpr static std::optional<std::uint8_t> decode_crockford(char c) noexcept{
		using u = std::uint8_t;
		switch(c){
		case '0': case 'O': case 'o': return u{0};
		case '1': case 'I': case 'i': case 'L': case 'l': return u{1};
		case '2': return u{2};
		case '3': return u{3};
		case '4': return u{4};
		case '5': return u{5};
		case '6': return u{6};
		case '7': return u{7};
		case '8': return u{8};
		case '9': return u{9};
		case 'A': case 'a': return u{10};
		case 'B': case 'b': return u{11};
		case 'C': case 'c': return u{12};
		case 'D': case 'd': return u{13};
		case 'E': case 'e': return u{14};
		case 'F': case 'f': return u{15};
		case 'G': case 'g': return u{16};
		case 'H': case 'h': return u{17};
		case 'J': case 'j': return u{18};
		case 'K': case 'k': return u{19};
		case 'M': case 'm': return u{20};
		case 'N': case 'n': return u{21};
		case 'P': case 'p': return u{22};
		case 'Q': case 'q': return u{23};
		case 'R': case 'r': return u{24};
		case 'S': case 's': return u{25};
		case 'T': case 't': return u{26};
		case 'V': case 'v': return u{27};
		case 'W': case 'w': return u{28};
		case 'X': case 'x': return u{29};
		case 'Y': case 'y': return u{30};
		case 'Z': case 'z': return u{31};
		default:
			return std::nullopt;
		}
	}
};

inline std::ostream& operator<<(std::ostream& os, const ulid_t& id){
	return os << id.to_string();
}
