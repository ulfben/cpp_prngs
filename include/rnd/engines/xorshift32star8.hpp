#pragma once
#include <assert.h>
#include <stdint.h> // AVR-libc provides <stdint.h>, but not the C++ <cstdint> wrapper.
/*
	XorShift32Star8 - a truncated XorShift* generator with 8-bit output and 32-bit state.

	Intended for byte-oriented applications and small 8-bit microcontrollers, such as
	classic AVR-based Arduino boards, where an 8-bit result is sufficient but a larger
	state cycle is still desirable. Its 32-bit XorShift state has a period of 2^32 - 1
	for any nonzero state: roughly 4.29 billion updates before the internal state repeats.

	The XorShift state transition was introduced by George Marsaglia (2003):
	https://doi.org/10.18637/jss.v008.i14

	The multiplicative XorShift* scrambler was introduced by Sebastiano Vigna (2014):
	https://arxiv.org/abs/1402.6246

	This specialization uses M.E. O'Neill's 32-bit parameter set A: shifts (6, 17, 9)
	and multiplier 0xB2E1CB1D, returning the high 8 bits of the multiplied state.

	Based on xorshift.hpp, Copyright (c) 2017-19 Melissa E. O'Neill (MIT License):
	https://gist.github.com/imneme/9b769cefccac1f2bd728596da3a856dd

	C++ port and modifications by Ulf Benjaminsson, 2026
	https://github.com/ulfben/cpp_prngs/

	Licensed under the MIT License. See LICENSE.md for details.
*/
namespace rnd {

class XorShift32Star8 final{
	using u8 = uint8_t;
	using u32 = uint32_t;

	static constexpr u32 DEFAULT_SEED = 0x7C62C6E0u; // Low 32 bits of O'Neill's default state.
	static constexpr u32 MULTIPLIER = 0xB2E1CB1Du;

	u32 state_;

	struct Direct{};
	constexpr XorShift32Star8(u32 state, Direct) noexcept : state_(state){
		assert(state_ != 0 && "XorShift32Star8 all-zero state is invalid");
	}

public:
	using result_type = u8;
	using seed_type = u32;
	struct state_type{
		u32 state;
	};

	constexpr XorShift32Star8() noexcept
		: XorShift32Star8(DEFAULT_SEED){}

	// Map seed zero to the default because an all-zero XorShift state cannot advance.
	explicit constexpr XorShift32Star8(seed_type seed) noexcept
		: state_(seed != 0 ? seed : DEFAULT_SEED){}

	constexpr state_type state() const noexcept{
		return {state_};
	}

	static constexpr XorShift32Star8 from_state(state_type state) noexcept{
		return XorShift32Star8{state.state, Direct{}};
	}

	constexpr void seed() noexcept{
		*this = XorShift32Star8{};
	}
	constexpr void seed(seed_type value) noexcept{
		*this = XorShift32Star8{value};
	}

	static constexpr result_type (min)() noexcept{ // Parentheses prevent expansion of Arduino's min macro.
		return result_type{0};
	}
	static constexpr result_type (max)() noexcept{ // Parentheses prevent expansion of Arduino's max macro.
		// Equivalent to std::numeric_limits<result_type>::max(), but <limits> is not available on AVR-libc.
		return static_cast<result_type>(~result_type{0});
	}

	constexpr result_type next() noexcept{
		const u32 product = state_ * MULTIPLIER;
		state_ ^= state_ >> 6;
		state_ ^= state_ << 17;
		state_ ^= state_ >> 9;
		return static_cast<result_type>(product >> 24);
	}

	constexpr result_type operator()() noexcept{
		return next();
	}

	constexpr void discard(unsigned long long count) noexcept{
		while(count--){
			next();
		}
	}

	constexpr bool operator==(const XorShift32Star8& rhs) const noexcept{
		return state_ == rhs.state_;
	}
	constexpr bool operator!=(const XorShift32Star8& rhs) const noexcept{
		return !(*this == rhs);
	}
};

} // namespace rnd
