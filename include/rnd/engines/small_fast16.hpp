#pragma once
#include "../detail/bit_operations.hpp"
#include <assert.h>
#include <stdint.h> // AVR-libc provides <stdint.h>, but not the C++ <cstdint> wrapper.
/*
  SmallFast16 PRNG - a C++ port of the 16-bit Jenkins Small Fast variant.

  Intended for 8-bit microcontrollers, relying on narrower arithmetic and 8 byte state.

  Original algorithm by Bob Jenkins (public domain):
  https://burtleburtle.net/bob/rand/smallprng.html

  16-bit constants and statistical validation by Melissa E. O'Neill:
  https://www.pcg-random.org/posts/bob-jenkins-small-prng-passes-practrand.html

  Based on jsf.hpp, Copyright (c) 2018 Melissa E. O'Neill (MIT License):
  https://gist.github.com/imneme/85cff47d4bad8de6bdeb671f9c76c814

  C++ port and modifications by Ulf Benjaminsson, 2026
  https://github.com/ulfben/cpp_prngs/

  Licensed under the MIT License. See LICENSE.md for details.
*/
namespace rnd {

class SmallFast16 final{
	using u16 = uint16_t;

public:
	using result_type = u16;
	using seed_type = u16;
	struct state_type{
		u16 a;
		u16 b;
		u16 c;
		u16 d;
	};

	private:
	struct Direct{};

	u16 a;
	u16 b;
	u16 c;
	u16 d;

	constexpr SmallFast16(state_type state, Direct) noexcept
		: a(state.a), b(state.b), c(state.c), d(state.d){
		assert((a | b | c | d) != 0 && "SmallFast16 all-zero state is invalid");
	}

	public:
	constexpr SmallFast16() noexcept
		: SmallFast16(0xBADCu){}

	explicit constexpr SmallFast16(seed_type seed) noexcept
		: a(0x5eedu), b(seed), c(seed), d(seed){
		discard(20); // Warm up the state
	}

	constexpr void seed() noexcept{
		*this = SmallFast16{};
	}
	constexpr void seed(seed_type value) noexcept{
		*this = SmallFast16{value};
	}

	constexpr state_type state() const noexcept{
		return {a, b, c, d};
	}

	static constexpr SmallFast16 from_state(state_type state) noexcept{
		return SmallFast16{state, Direct{}};
	}

	static constexpr result_type (min)() noexcept{ // Parentheses prevent expansion of Arduino's min macro.
		return result_type{0};
	}
	static constexpr result_type (max)() noexcept{ // Parentheses prevent expansion of Arduino's max macro.
		// Equivalent to std::numeric_limits<result_type>::max(), but <limits> is not available on AVR-libc.
		return static_cast<result_type>(~result_type{0});
	}

	constexpr result_type next() noexcept{
		const u16 e = a - rnd::detail::rotl(b, 13);
		a = b ^ rnd::detail::rotl(c, 8);
		b = c + d;
		c = d + e;
		d = e + a;
		return d;
	}

	constexpr result_type operator()() noexcept{
		return next();
	}

	constexpr void discard(unsigned long long count) noexcept{
		while(count--){
			next();
		}
	}

	constexpr bool operator==(const SmallFast16& rhs) const noexcept{
		return a == rhs.a && b == rhs.b && c == rhs.c && d == rhs.d;
	}
	constexpr bool operator!=(const SmallFast16& rhs) const noexcept{
		return !(*this == rhs);
	}
};

} // namespace rnd
