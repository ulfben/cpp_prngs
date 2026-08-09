#pragma once
#include "../detail/bit_operations.hpp"
#include <assert.h>
#include <stdint.h> // AVR-libc provides <stdint.h>, but not the C++ <cstdint> wrapper.

// pcg32.hpp - Minimal PCG32 implementation for C++
//
// Based on "Really minimal PCG32 code" by M.E. O'Neill (2014)
// https://github.com/imneme/pcg-c-basic/
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
// Ported to modern C++ and adapted by Ulf Benjaminsson, 2025
// Copyright (c) 2014 M.E. O'Neill, pcg-random.org
// Copyright (c) 2025 Ulf Benjaminsson, github.com/ulfben/cpp_prngs

class PCG32 final{
	struct Direct{}; //tag for from_state()
	using u64 = uint64_t;
	using u32 = uint32_t; //Important! We must guarantee exactly 32 bits. uint_fast32_t is often 64-bit on modern 64-bit UNIX systems which would break this implementation
	static constexpr u64 DEFAULT_SEED = 0x853c49e6748fea9bULL;
	static constexpr u64 DEFAULT_STREAM = 0xda3e39cb94b95bdbULL;
	static constexpr u64 MULT = 6364136223846793005ULL;
	u64 state{0}; // RNG state.  All values are possible.
	u64 inc{1}; // controls which RNG sequence (stream) is selected. Must *always* be odd.

	//private constructor to allow factory function from_state() to bypass the seeding routines.
	constexpr PCG32(u64 state_val, u64 inc_val, Direct) noexcept
		: state(state_val), inc(inc_val){
		assert((inc & 1u) != 0 && "PCG32 state increment must be odd");
	}

public:
	using result_type = u32;
	using seed_type = u64;
	using state_type = u64;

	constexpr PCG32() noexcept
		: PCG32(DEFAULT_SEED, DEFAULT_STREAM){}

	explicit constexpr PCG32(seed_type seed_val) noexcept
		: PCG32(seed_val, DEFAULT_STREAM){}

	constexpr PCG32(seed_type seed_val, seed_type sequence) noexcept{
		seed(seed_val, sequence);
	}
	//factory function to create a PCG32 from a state, bypassing the seeding routines.
	static constexpr PCG32 from_state(state_type state_val, state_type inc_val) noexcept{
		return PCG32{state_val, inc_val, Direct{}};
	}
	constexpr result_type next() noexcept{
		const auto oldstate = state;
		state = oldstate * MULT + inc;
		const u32 xorshifted = static_cast<u32>(((oldstate >> 18u) ^ oldstate) >> 27u);
		const u32 rot = static_cast<u32>(oldstate >> 59u);
		return rnd::detail::rotr(xorshifted, rot);
	}
	constexpr result_type operator()() noexcept{
		return next();
	}

	constexpr void seed() noexcept{
		seed(DEFAULT_SEED, DEFAULT_STREAM);
	}

	//seed and a sequence selection constant (a.k.a. stream id).
	constexpr void seed(seed_type seed_val, seed_type sequence = DEFAULT_STREAM) noexcept{
		state = 0U;
		inc = (sequence << 1u) | 1u; //ensure inc is odd
		(void) next(); //“Warm up” the internal LCG so the first returned bits are mixed.
		state += seed_val;
		(void) next();
	}

	constexpr void discard(unsigned long long delta) noexcept{
		//Based on Brown, "Random Number Generation with Arbitrary Stride,"
		// Transactions of the American Nuclear Society (Nov. 1994)       
		u64 cur_mult = MULT;
		u64 cur_plus = inc;
		u64 acc_mult = 1u;
		u64 acc_plus = 0u;
		while(delta > 0){
			if(delta & 1){
				acc_mult *= cur_mult;
				acc_plus = acc_plus * cur_mult + cur_plus;
			}
			cur_plus = (cur_mult + 1) * cur_plus;
			cur_mult *= cur_mult;
			delta /= 2;
		}
		state = acc_mult * state + acc_plus;
	}

	constexpr PCG32 split() noexcept{
		return PCG32{next(), (next() << 1u) | 1u};
	}

	static constexpr result_type (min)() noexcept{ // Parentheses prevent expansion of Arduino's min macro.
		return result_type{0};
	}

	static constexpr result_type (max)() noexcept{ // Parentheses prevent expansion of Arduino's max macro.
		// Equivalent to std::numeric_limits<result_type>::max(), but <limits> is not available on AVR-libc.
		return static_cast<result_type>(~result_type{0});
	}

	constexpr bool operator==(const PCG32& rhs) const noexcept{
		return state == rhs.state && inc == rhs.inc;
	}
	constexpr bool operator!=(const PCG32& rhs) const noexcept{
		return !(*this == rhs);
	}
};
