#pragma once
#include "../detail/bit_operations.hpp"
#include <stdint.h> // AVR-libc provides <stdint.h>, but not the C++ <cstdint> wrapper.
/*
  SmallFast8 PRNG - a C++ port of the 8-bit Jenkins Small Fast variant.

  Intended for highly resource-constrained 8-bit targets, using byte-wide arithmetic and 4 bytes of state.
  It favors minimal state over long streams; O'Neill reports a PractRand failure at 2^28 bytes.
  Ergo: statistical weaknesses become detectable after roughly 268 million raw outputs. 
  Likely well more than enough for an Arduboy game. :) 

  Original algorithm by Bob Jenkins (public domain):
  https://burtleburtle.net/bob/rand/smallprng.html

  8-bit constants and statistical validation by Melissa E. O'Neill:
  https://www.pcg-random.org/posts/bob-jenkins-small-prng-passes-practrand.html

  Based on jsf.hpp, Copyright (c) 2018 Melissa E. O'Neill (MIT License):
  https://gist.github.com/imneme/85cff47d4bad8de6bdeb671f9c76c814

  C++ port and modifications by Ulf Benjaminsson, 2026
  https://github.com/ulfben/cpp_prngs/

  Licensed under the MIT License. See LICENSE.md for details.
*/
namespace rnd {

class SmallFast8 final{
	using u8 = uint8_t;

	u8 a;
	u8 b;
	u8 c;
	u8 d;

public:
	using result_type = u8;
	using seed_type = u8;

	constexpr SmallFast8() noexcept
		: SmallFast8(0xDCu){}

	explicit constexpr SmallFast8(seed_type seed) noexcept
		: a(0xEDu), b(seed), c(seed), d(seed){
		discard(20); // Warm up the state
	}

	constexpr void seed() noexcept{
		*this = SmallFast8{};
	}
	constexpr void seed(seed_type value) noexcept{
		*this = SmallFast8{value};
	}

	static constexpr result_type (min)() noexcept{ // Parentheses prevent expansion of Arduino's min macro.
		return result_type{0};
	}
	static constexpr result_type (max)() noexcept{ // Parentheses prevent expansion of Arduino's max macro.
		// Equivalent to std::numeric_limits<result_type>::max(), but <limits> is not available on AVR-libc.
		return static_cast<result_type>(~result_type{0});
	}

	constexpr result_type next() noexcept{
		// C++ promotes u8 values to int during arithmetic.
		// Casting back to u8 keeps only the low 8 bits, giving the 8-bit wraparound required by the algorithm.
		const u8 e = static_cast<u8>(a - rnd::detail::rotl(b, 1));
		a = static_cast<u8>(b ^ rnd::detail::rotl(c, 4));
		b = static_cast<u8>(c + d);
		c = static_cast<u8>(d + e);
		d = static_cast<u8>(e + a);
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

	constexpr bool operator==(const SmallFast8& rhs) const noexcept{
		return a == rhs.a && b == rhs.b && c == rhs.c && d == rhs.d;
	}
	constexpr bool operator!=(const SmallFast8& rhs) const noexcept{
		return !(*this == rhs);
	}
};

} // namespace rnd
