#pragma once
#include "../detail/bit_operations.hpp"
#include <stdint.h> // AVR-libc provides <stdint.h>, but not the C++ <cstdint> wrapper.
#include <assert.h>

/*
  Xoshiro256SS - a modern C++ port of xoshiro256** 1.0.

  The original "xoshiro256** 1.0" generator by David Blackman and Sebastiano Vigna (public domain)
  https://prng.di.unimi.it/xoshiro256starstar.c

  "splitmix64" by Sebastiano Vigna (public domain)
  https://prng.di.unimi.it/splitmix64.c

  C++ implementation by Ulf Benjaminsson, 2025,
  Licensed under the MIT License. See LICENSE.md for details.
  https://github.com/ulfben/cpp_prngs/
*/

class Xoshiro256SS{
	using u64 = uint64_t;
	u64 s[4]{};
	struct Direct{};

	static constexpr u64 splitmix64(u64& x) noexcept{
		x += 0x9E3779B97F4A7C15ULL; // golden ratio increment
		u64 z = x;
		z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ULL;
		z = (z ^ (z >> 27)) * 0x94D049BB133111EBULL;
		return z ^ (z >> 31);
	}
	
	//private constructor to allow factory function from_state() to bypass the seeding routines.
	constexpr Xoshiro256SS(u64 s0, u64 s1, u64 s2, u64 s3, Direct) noexcept
		: s{s0, s1, s2, s3}{
		assert((s[0] | s[1] | s[2] | s[3]) != 0 && "xoshiro256** all-zero state is invalid");
	}

public:
	using result_type = u64;
	using seed_type = u64;
	using state_type = u64;

	constexpr Xoshiro256SS() noexcept
		: Xoshiro256SS(0xFEEDFACECAFEBEEFuLL){}

	explicit constexpr Xoshiro256SS(seed_type seed) noexcept{		
		s[0] = splitmix64(seed);
		s[1] = splitmix64(seed);
		s[2] = splitmix64(seed);
		s[3] = splitmix64(seed);

		if((s[0] | s[1] | s[2] | s[3]) == 0){
			s[0] = 0xFEEDFACECAFEBEEFuLL; //all-zero state is invalid for xoshiro
		}
	}

	//factory function to create a Xoshiro256SS from a state, bypassing the seeding routines.
	template <class State>
	static constexpr Xoshiro256SS from_state(const State& state) noexcept{
		return Xoshiro256SS{state[0], state[1], state[2], state[3], Direct{}};
	}
	constexpr void seed() noexcept{
		*this = Xoshiro256SS{};
	}
	constexpr void seed(seed_type seed) noexcept{
		*this = Xoshiro256SS{seed};
	}
	static constexpr result_type (min)() noexcept{ // Parentheses prevent expansion of Arduino's min macro.
		return result_type{0};
	}
	static constexpr result_type (max)() noexcept{ // Parentheses prevent expansion of Arduino's max macro.
		// Equivalent to std::numeric_limits<result_type>::max(), but <limits> is not available on AVR-libc.
		return static_cast<result_type>(~result_type{0});
	}
	constexpr result_type next() noexcept{
		const auto result = rnd::detail::rotl(s[1] * 5, 7) * 9;
		const auto t = s[1] << 17;
		s[2] ^= s[0];
		s[3] ^= s[1];
		s[1] ^= s[2];
		s[0] ^= s[3];
		s[2] ^= t;
		s[3] = rnd::detail::rotl(s[3], 45);
		return result;
	}
	constexpr result_type operator()() noexcept{
		return next();
	}

	constexpr void discard(unsigned long long n) noexcept{
		while(n--){
			next();
		}
	}

	 /* the jump() function is equivalent to 2^128 calls to next();
	 it can be used to generate 2^128 non-overlapping subsequences
	 for parallel computations.
	 constexpr void jump() noexcept{
		 constexpr std::array<std::bitset<64>, SEED_COUNT> JUMP{
			 0x180ec6d33cfd0abaULL, 0xd5a61266f0c9392cULL, 0xa9582618e03fc9aaULL, 0x39abdc4529b1661cULL
		 };
		 State temp{0};
		 for(const auto& bits : JUMP){
			 for(std::size_t b = 0; b < 64; ++b){
				 if(bits.test(b)){
					 temp[0] ^= s[0];
					 temp[1] ^= s[1];
					 temp[2] ^= s[2];
					 temp[3] ^= s[3];
				 }
				 next();
			 }
		 }
		 s = temp;
	 } */

	constexpr bool operator==(const Xoshiro256SS& rhs) const noexcept{
		return s[0] == rhs.s[0] && s[1] == rhs.s[1] && s[2] == rhs.s[2] && s[3] == rhs.s[3];
	}
	constexpr bool operator!=(const Xoshiro256SS& rhs) const noexcept{
		return !(*this == rhs);
	}
};
