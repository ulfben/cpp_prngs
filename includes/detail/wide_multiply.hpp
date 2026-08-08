#pragma once
#include <cstdint>
#ifdef _MSC_VER
#include <intrin.h>    // for _umul128, 64x64 multiplication
#endif

// Private helpers to keep Random<E> constexpr and portable.
// Provides a constexpr 128-bit multiply and shift for platforms without native __uint128_t support, such as MSVC.
// including a fully constexpr fallback on MSVC, where _umul128 is not constexpr
// This is used to implement Daniel Lemire's FastRange mapping (see Random<E> for usage).
namespace rnd{
	namespace detail {
		// Helper for constexpr 128-bit multiply on MSVC, where _umul128 is not constexpr.
		struct u128_parts final{
			std::uint64_t lo;
			std::uint64_t hi;
		};

		[[nodiscard]] constexpr u128_parts mul64_to_128_parts(std::uint64_t a, std::uint64_t b) noexcept{
			// split 32-bit limbs
			const std::uint64_t a0 = static_cast<std::uint32_t>(a);
			const std::uint64_t a1 = a >> 32;
			const std::uint64_t b0 = static_cast<std::uint32_t>(b);
			const std::uint64_t b1 = b >> 32;

			// partial products
			const std::uint64_t p00 = a0 * b0;
			const std::uint64_t p01 = a0 * b1;
			const std::uint64_t p10 = a1 * b0;
			const std::uint64_t p11 = a1 * b1;

			// combine:			
			constexpr std::uint64_t lo32_mask = 0xFFFF'FFFFull;
			const std::uint64_t mid = p01 + p10;
			const std::uint64_t mid_carry = (mid < p01) ? (1ull << 32) : 0ull;
			const std::uint64_t mid_lo = (mid & lo32_mask) << 32;
			const std::uint64_t mid_hi = mid >> 32;
			const std::uint64_t lo = p00 + mid_lo;
			const std::uint64_t lo_carry = (lo < p00) ? 1ull : 0ull;

			const std::uint64_t hi = p11 + mid_hi + mid_carry + lo_carry;
			return {lo, hi};
		}

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

		// mul_shift_u64 - the helper we actually want.
		// Computes (x * bound) >> digits, truncated to u64.
		// Used to implement Daniel Lemire's fastrange trick portably and constexpr.		
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
			static_assert(false, "mul_shift_high64 requires either __uint128_t or MSVC _umul128");
#endif
		}
	} //detail namespace
} // namespace rnd
