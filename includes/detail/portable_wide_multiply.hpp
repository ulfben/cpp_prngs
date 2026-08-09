#pragma once
#include <stdint.h> // AVR-libc provides <stdint.h>, but not the C++ <cstdint> wrapper.

namespace rnd{
	namespace detail{
		struct u128_parts final{
			uint64_t lo;
			uint64_t hi;
		};

		// Portable 64x64 -> 128-bit multiplication using four 32-bit limbs.
		[[nodiscard]] constexpr u128_parts mul64_to_128_parts(uint64_t a, uint64_t b) noexcept{
			const uint64_t a0 = static_cast<uint32_t>(a);
			const uint64_t a1 = a >> 32;
			const uint64_t b0 = static_cast<uint32_t>(b);
			const uint64_t b1 = b >> 32;

			const uint64_t p00 = a0 * b0;
			const uint64_t p01 = a0 * b1;
			const uint64_t p10 = a1 * b0;
			const uint64_t p11 = a1 * b1;

			const uint64_t mid = p01 + p10;
			const uint64_t mid_carry = mid < p01 ? (uint64_t{1} << 32) : uint64_t{0};
			const uint64_t mid_low = (mid & UINT32_MAX) << 32;
			const uint64_t low = p00 + mid_low;
			const uint64_t high = p11 + (mid >> 32) + mid_carry + (low < p00 ? 1u : 0u);
			return {low, high};
		}
	} // detail namespace
} // rnd namespace
