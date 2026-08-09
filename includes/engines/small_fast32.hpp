#pragma once
#include "../detail/bit_operations.hpp"
#include <stdint.h> // AVR-libc provides <stdint.h>, but not the C++ <cstdint> wrapper.
/*
  SmallFast64 PRNG - a modern C++ 32-bit two-rotate implementation of Jenkins Small Fast PRNG.

  Original algorithm and C code by Bob Jenkins (public domain)
  https://burtleburtle.net/bob/rand/smallprng.html

  C++ implementation by Ulf Benjaminsson, 2025,
  Licensed under the MIT License. See LICENSE.md for details.
  https://github.com/ulfben/cpp_prngs/
*/
class SmallFast32 final{
	using u32 = uint32_t;
	using u64 = uint64_t;

	u32 a;
	u32 b;
	u32 c;
	u32 d;

public:
	using result_type = u32;
	using seed_type = u64;

	constexpr SmallFast32() noexcept
		: SmallFast32(0xBADC0FFEu){}

	//64-bit constructor for convenience and better seeding quality.
	//but for backwards compatibility with the original implementation, a 32-bit constructor is provided too.
	explicit constexpr SmallFast32(seed_type seed) noexcept
		: a(0xf1ea5eedu)
		, b(static_cast<u32>(seed))
		, c(static_cast<u32>(seed >> 32))
		, d(static_cast<u32>(seed ^ (seed >> 32))){
		discard(20); //warmup
	}

	// Overload for 32-bit seeds, only to be able to validate against the original implementation
	// The 64-bit seed constructor is the primary one and should be preferred for better seeding quality.
	explicit constexpr SmallFast32(u32 seed) noexcept
		: a(0xf1ea5eedu)
		, b(seed)
		, c(seed)
		, d(seed){
		discard(20); //warmup
	}

	constexpr void seed() noexcept{
		*this = SmallFast32{};
	}
	constexpr void seed(seed_type seed) noexcept{
		*this = SmallFast32{seed};
	}

	static constexpr result_type (min)() noexcept{ // Parentheses prevent expansion of Arduino's min macro.
		return result_type{0};
	}
	static constexpr result_type (max)() noexcept{ // Parentheses prevent expansion of Arduino's max macro.
		// Equivalent to std::numeric_limits<result_type>::max(), but <limits> is not available on AVR-libc.
		return static_cast<result_type>(~result_type{0});
	}
	constexpr result_type operator()() noexcept{
		return next();
	}

	constexpr result_type next() noexcept{
		const u32 e = a - rnd::detail::rotl(b, 27);
		a = b ^ rnd::detail::rotl(c, 17);
		b = c + d;
		c = d + e;
		d = e + a;
		return d;
	}

	constexpr void discard(unsigned long long n) noexcept{
		while(n--){
			next();
		}
	}

	constexpr bool operator==(const SmallFast32& rhs) const noexcept{
		return a == rhs.a && b == rhs.b && c == rhs.c && d == rhs.d;
	}
	constexpr bool operator!=(const SmallFast32& rhs) const noexcept{
		return !(*this == rhs);
	}
};
