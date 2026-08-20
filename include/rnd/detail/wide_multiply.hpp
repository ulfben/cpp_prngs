#pragma once
#include "portable_wide_multiply.hpp"
#include <cstdint>
#ifdef _MSC_VER
#include <intrin.h>    // for _umul128, 64x64 multiplication
#endif

// Portable support for 64x64 -> 128-bit multiplication and shifting.
//
// Used by Random<E> to implement Daniel Lemire's fast range reduction while
// remaining constexpr and portable. GCC and Clang use native __uint128_t;
// MSVC uses _umul128 at runtime and a portable constexpr fallback.
//
// A standard facility could eventually make this layer unnecessary:
// - P3140R0 proposed std::uint_least128_t, but was not pursued.
// - P3161R4 proposes std::mul_wide(), which provides the required full-width
//   multiplication directly.
// https://www.open-std.org/jtc1/sc22/wg21/docs/papers/2024/p3140r0.html
// https://www.open-std.org/jtc1/sc22/wg21/docs/papers/2025/p3161r4.html
namespace rnd{
	namespace detail {
		// Computes (hi:lo) >> digits for digits in [1, 64], returning the low 64 bits of the shifted result.
		template <unsigned digits>
		[[nodiscard]] constexpr std::uint64_t shr128_to_u64(std::uint64_t hi, std::uint64_t lo) noexcept{
			static_assert(digits > 0 && digits <= 64);
			if constexpr(digits == 64){
				return hi;
			} else{
				return (lo >> digits) | (hi << (64u - digits));
			}
		}
		
		// Computes (x * bound) >> digits using the full 128-bit product,
		// returning the low 64-bits of the shifted result. 		
		template <unsigned digits>
		[[nodiscard]] constexpr std::uint64_t mul_shift_u64(std::uint64_t x, std::uint64_t bound) noexcept{
			static_assert(digits >= 1 && digits <= 64, "digits must be in [1, 64]");

#if defined(__SIZEOF_INT128__)
			return static_cast<std::uint64_t>(
				(static_cast<__uint128_t>(x) * static_cast<__uint128_t>(bound)) >> digits
			);

#elif defined(_MSC_VER)
			std::uint64_t hi = 0;
			std::uint64_t lo = 0;
			if consteval{
				const auto p = mul64_to_128_parts(x, bound); // constexpr fallback
				lo = p.lo;
				hi = p.hi;
			} else{ // runtime path				
				lo = _umul128(x, bound, &hi);
			}
			return shr128_to_u64<digits>(hi, lo);

#else
			static_assert(false, "mul_shift_u64 requires either __uint128_t or MSVC _umul128");
#endif
		}
	} //detail namespace
} // namespace rnd
