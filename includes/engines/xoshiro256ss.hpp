#pragma once
#include "../concepts.hpp" //for RandomBitEngine
#include <limits>
#include <cstdint>
#include <span>
#include <bit> //std::rotl

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
	using u64 = std::uint64_t;
	u64 s[4]{};

	static constexpr u64 splitmix64(u64 x) noexcept{
		u64 z = (x + 0x9e3779b97f4a7c15uLL);
		z = (z ^ (z >> 30)) * 0xbf58476d1ce4e5b9uLL;
		z = (z ^ (z >> 27)) * 0x94d049bb133111ebuLL;
		return z ^ (z >> 31);
	}
	   //private constructor to allow factory function from_state() to bypass the seeding routines.
	constexpr Xoshiro256SS(std::span<const u64> state) noexcept{
		std::ranges::copy(state, s);
	}
public:
	using result_type = u64;
	using seed_type = u64;
	using state_type = u64;

	constexpr Xoshiro256SS() noexcept
		: Xoshiro256SS(0xFEEDFACECAFEBEEFuLL){}

	explicit constexpr Xoshiro256SS(seed_type seed) noexcept{
	// Seed initialization: instead of copying the same splitmix64(seed)
	// into all 4 state words, I  chain splitmix64 calls with added constants. 
	// Each constant is chosen to be large, odd, and distinct so that a poor 
	// initial seed can never collapse the entire 256-bit state into something trivial.    
		s[0] = splitmix64(seed);
		s[1] = splitmix64(s[0] + 0x9E3779B97F4A7C15uLL); // golden ratio
		s[2] = splitmix64(s[1] + 0x7F4A7C15F39CCCD1ULL); // arbitrary odd
		s[3] = splitmix64(s[2] + 0x3549B5A7B97C9A31ULL); // another odd
	}
	//factory function to create a Xoshiro256SS from a state, bypassing the seeding routines.
	static constexpr Xoshiro256SS from_state(std::span<const state_type> state) noexcept{
		return Xoshiro256SS{state};
	}
	constexpr void seed() noexcept{
		*this = Xoshiro256SS{};
	}
	constexpr void seed(seed_type seed) noexcept{
		*this = Xoshiro256SS{seed};
	}
	static constexpr result_type min() noexcept{
		return result_type{0};
	}
	static constexpr result_type max() noexcept{
		return std::numeric_limits<result_type>::max();
	}
	constexpr result_type next() noexcept{
		const auto result = std::rotl(s[1] * 5, 7) * 9;
		const auto t = s[1] << 17;
		s[2] ^= s[0];
		s[3] ^= s[1];
		s[1] ^= s[2];
		s[0] ^= s[3];
		s[2] ^= t;
		s[3] = std::rotl(s[3], 45);
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

	constexpr bool operator==(const Xoshiro256SS& rhs) const noexcept = default; //will do the right thing since C++20! 
};
static_assert(RandomBitEngine<Xoshiro256SS>);

#if VALIDATE_PRNGS

//original implementation of xoshiro256** 1.0 by David Blackman and Sebastiano Vigna
// https://prng.di.unimi.it/xoshiro256starstar.c

constexpr uint64_t rotl(const uint64_t x, int k) noexcept{
	return (x << k) | (x >> (64 - k));
}

static constexpr auto Xoshiro256SS_REFERENCE = [](){	
	uint64_t s_local[4] = {0xFEEDFACECAFEBEEFuLL, 0, 0, 0};	
	auto next_fn = [](uint64_t state[4]) -> uint64_t{		
		const uint64_t result = rotl(state[1] * 5, 7) * 9;
		const uint64_t t = state[1] << 17;
		state[2] ^= state[0];
		state[3] ^= state[1];
		state[1] ^= state[2];
		state[0] ^= state[3];
		state[2] ^= t;
		state[3] = rotl(state[3], 45);
		return result;
	};

	std::array<uint64_t, 6> out{};
	for(auto& v : out){
		v = next_fn(s_local);
	}
	return out;
	}();

constexpr uint64_t s2[4]{0xFEEDFACECAFEBEEFuLL, 0, 0, 0};

static_assert(
	prng_outputs(Xoshiro256SS::from_state(s2)) == Xoshiro256SS_REFERENCE,
	"Xoshiro256SS output does not match Xoshiro256** reference"
	);

#endif // VALIDATE_PRNGS