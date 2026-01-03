#pragma once
#include <algorithm>
#include <bit> // for std::bit_cast
#include <cassert>
#include <cmath>
#include <concepts>
#include <cstdint>
#include <iterator>
#include <ranges>
#include <type_traits>
#ifdef _MSC_VER
#include <intrin.h>    // for _umul128, 64x64 multiplication
#endif
#include "concepts.hpp" //for RandomBitEngine concept

// This is an RNG interface that wraps around any engine that meets the RandomBitEngine concept.
// It provides useful functions for generating values, including integers, floating-point numbers, and colors
// as well as methods for Gaussian distribution, coin flips (with odds), picking from collections (index or element), etc.
// Source: https://github.com/ulfben/cpp_prngs/
// Demo is available on Compiler Explorer: https://compiler-explorer.com/z/nzK9joeYE
// Benchmarks:
   // Quick Bench for generating raw random values: https://quick-bench.com/q/vWdKKNz7kEyf6kQSNnUEFOX_4DI
   // Quick Bench for generating normalized floats: https://quick-bench.com/q/GARc3WSfZu4sdVeCAMSWWPMQwSE
   // Quick Bench for generating bounded values: https://quick-bench.com/q/WHEcW9iSV7I8qB_4eb1KWOvNZU0
namespace rnd {
	namespace detail {
		//the detail namespace holds a few private helpers for keeping the Random<E> class constexpr and portable. 
		// specifically 128-bit multiply, which we need for Lemires' "fastrange" trick.
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

			// Partial products
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

		// private helper for 128-bit multiplications
		// returns (x * bound) >> digits, truncated to u64
		// Used to implement Daniel Lemire’s "fastrange" trick portably: https://lemire.me/blog/2016/06/27/a-fast-alternative-to-the-modulo-reduction/
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

	namespace detail::selftest {
		//quick-and-dirty test suite to make sure our 128-bit helper is constexpr and correct
		// feel free to delete this namespace or gate it behind a macro so headers don’t spam every TU. :)
		
		// 1. Verify Shift Logic
		constexpr std::uint64_t HI = 0x0123'4567'89AB'CDEFull;
		constexpr std::uint64_t LO = 0xFEDC'BA98'7654'3210ull;
		static_assert(shr128_to_u64<64>(HI, LO) == HI); // digits = 64 -> returns hi		
		static_assert(shr128_to_u64<1>(HI, LO) == ((LO >> 1) | (HI << 63))); // digits = 1 -> cross-word shift		
		static_assert(shr128_to_u64<63>(HI, LO) == ((LO >> 63) | (HI << 1)));

		// 2. Verify 128-bit Multiply Logic
		constexpr bool check_mul(std::uint64_t a, std::uint64_t b, std::uint64_t expect_lo, std::uint64_t expect_hi){
			const auto p = mul64_to_128_parts(a, b);
			return p.lo == expect_lo && p.hi == expect_hi;
		}

		// Identity & Zero
		static_assert(check_mul(0, 0, 0, 0));
		static_assert(check_mul(UINT64_MAX, 1, UINT64_MAX, 0));

		// Boundary: 2^32 * 2^32 = 2^64 (Result: Lo=0, Hi=1)
		static_assert(check_mul(1ULL << 32, 1ULL << 32, 0, 1));

		// Stress Test: Max * Max = (2^64 - 1)^2 = 2^128 - 2^65 + 1
		// Result: Lo = 1, Hi = F...FE
		static_assert(check_mul(UINT64_MAX, UINT64_MAX, 1, 0xFFFFFFFFFFFFFFFEull));

		// Middle Carry: (2^64 - 1) * 2^32 = 2^96 - 2^32
		// Result: Hi = 2^32 - 1, Lo = -2^32 (wrapped)
		static_assert(check_mul(UINT64_MAX, 1ULL << 32, 0xFFFFFFFF00000000ull, 0x00000000FFFFFFFFull));

		// Low carry stress
		static_assert(check_mul(0x0000'0001'FFFF'FFFFull, 0x0000'0001'FFFF'FFFFull, 0xFFFF'FFFC'0000'0001ull, 0x0000'0000'0000'0003ull));

		// 3. Verify constexpr instantiation
		template <std::uint64_t> struct require_constexpr{};
		using test_inst_1 = require_constexpr<mul_shift_u64<1>(HI, LO)>;
		using test_inst_64 = require_constexpr<mul_shift_u64<64>(HI, LO)>;
	} // namespace detail::selftest

	template <RandomBitEngine E>
	class Random final{
		static constexpr unsigned value_bits = std::numeric_limits<E::result_type>::digits;
		E _e{}; //the underlying engine providing random bits. This class will turn those into useful values.

	public:
		using engine_type = E;
		using result_type = typename E::result_type;
		static_assert(std::is_unsigned_v<result_type>);

		constexpr Random() noexcept = default; //the engine will default initialize

		explicit constexpr Random(engine_type engine) noexcept : _e(engine){}

		explicit constexpr Random(result_type seed_val) noexcept : _e(seed_val){};

		constexpr bool operator==(const Random& rhs) const noexcept = default;

		//access to the underlying engine for manual serialization, etc.
		constexpr const E& engine() const noexcept{
			return _e;
		}

		constexpr E& engine() noexcept{
			return _e;
		}

		//advance the random engine n steps.
		//some engines (like PCG32) can do this faster than linear time
		constexpr void discard(unsigned long long n) noexcept{
			_e.discard(n);
		}

		constexpr void seed() noexcept{
			_e = E{};
		}

		constexpr void seed(result_type v) noexcept{
			_e.seed(v);
		}

		// returns a decorrelated, forked engine; advances this engine's state
		// use for parallel or independent streams use (think: task/thread-local randomness)
		constexpr Random<E> split() noexcept{
			return Random<E>(_e.split());
		}

		static constexpr auto min() noexcept{
			return E::min();
		}

		static constexpr auto max() noexcept{
			return E::max();
		}

		// Produces a random value in the range [min(), max())
		constexpr result_type next() noexcept{
			return _e();
		}

		constexpr result_type operator()() noexcept{
			return next();
		}

		// produces a random value in [0, bound) using Lemire's fastrange.
		// achieves very small bias without using rejection, and is much faster than naive modulo.
		constexpr result_type next(result_type bound) noexcept{
			assert(bound > 0 && "bound must be non-zero and positive");

			result_type raw_value = next(); // raw_value is in the range [0, 2^value_bits)

			if constexpr(value_bits <= 32){ // for small engines, multiply into a 64-bit product
				auto product = std::uint64_t(raw_value) * std::uint64_t(bound); // product is now in [0, bound * 2^value_bits)
				auto result = result_type(product >> value_bits); // equivalent to floor(product / 2^value_bits)
				return result;                    // result is now in range [0, bound)
			} else if constexpr(value_bits <= 64){
				// same logic, but use helper for 128-bit math, since __uint128_t isn't universally available
				return detail::mul_shift_u64<value_bits>(raw_value, bound);
			} else{ // fallback for hypothetical >64-bit engines. Naive modulo (slower, more bias)
				return bound > 0 ? raw_value % bound : bound; // avoid division by zero in release builds
			}
		}

		constexpr result_type operator()(result_type bound) noexcept{
			return next(bound);
		}

		// compile time overload: next<Bound, Type>()
		// gets random value in [0, Bound)
		// returns value in type T (default: result_type)
		template <result_type Bound, std::integral T = result_type>
		constexpr T next() noexcept{
			static_assert(Bound > 0, "Bound must be positive");
			static_assert(Bound - 1 <= static_cast<result_type>(std::numeric_limits<T>::max()),
				"Bound is too large for return type T");
			// if Bound is a power of two, we can use a mask / bit-extract.
			if constexpr((Bound & (Bound - 1)) == 0){
				constexpr unsigned bits_needed = std::countr_zero(Bound);
				static_assert(bits_needed <= value_bits, "Bound is too large for this engine's result_type");
				return bits<bits_needed, T>();
			} else{
				// Otherwise just call the runtime version.
				// Bound is a compile-time constant here, so the compiler can constant-fold the multiply/shift.
				return static_cast<T>(next(Bound));
			}
		}

		// integer in [lo, hi)
		template <std::integral I>
		constexpr I between(I lo, I hi) noexcept{
			if(!(lo < hi)){
				assert(false && "between(lo, hi): inverted or empty range");
				return lo;
			}
			using U = std::make_unsigned_t<I>;
			U bound = U(hi) - U(lo);
			assert(bound <= E::max() &&
				"between(lo, hi): range too large for this engine. Consider a 64-bit engine "
				"(xoshiro256ss, SmallFast64) or ensure hi–lo <= max()");
			auto safe_bound = static_cast<result_type>(bound);
			return lo + static_cast<I>(next(safe_bound));
		}

		// real in [lo, hi)
		template <std::floating_point F> constexpr F between(F lo, F hi) noexcept{
			return lo + (hi - lo) * normalized<F>();
		}

		// real in [0.0,1.0) using the "IQ float hack"
		//   see Iñigo Quilez, "sfrand": https://iquilezles.org/articles/sfrand/
		// Fast, branchless and, now, portable.
		template <std::floating_point F>
		constexpr F normalized() noexcept{
			static_assert(std::numeric_limits<F>::is_iec559, "normalized() requires IEEE 754 (IEC 559) floating point types.");
			using UInt = std::conditional_t<sizeof(F) == 4, uint32_t, uint64_t>; // Pick wide enough unsigned int type for F
			constexpr int mantissa_bits = std::numeric_limits<F>::digits - 1; // Number of mantissa bits for F (e.g., 23 for float)
			static_assert(mantissa_bits <= value_bits,
				"This engine cannot generate enough mantissa bits for this floating-point type. "
				"Use a 64-bit engine or request a 32-bit float.");
			constexpr UInt base = std::bit_cast<UInt>(F(1.0)); // Bit pattern for F(1.0), i.e., exponent set, mantissa 0
			UInt mantissa = this->template bits<mantissa_bits, UInt>();      // Get random bits to fill the mantissa field
			UInt as_int = base | mantissa; // Combine base (1.0) with random mantissa bits
			return std::bit_cast<F>(as_int) - F(1.0); // Convert bits to float/double, then subtract 1.0 to get [0,1)
		}

		// real in [-1.0,1.0) using the IQ float hack.
		template <std::floating_point F>
		constexpr F signed_norm() noexcept{
			return F(2) * normalized<F>() - F(1); // scale to [0.0, 2.0), then shift to [-1.0, 1.0)
		}

		// boolean
		constexpr bool coin_flip() noexcept{
			return bool(next() & 1);
		}

		// boolean with probability
		template <std::floating_point F>
		constexpr bool coin_flip(F probability) noexcept{
			return normalized<F>() < probability;
		}

		// pick an index in [0, size)
		template <std::ranges::sized_range R>
		constexpr auto index(const R& collection) noexcept{
			assert(!std::ranges::empty(collection) && "Random::index(): empty collection.");
			using idx_t = std::ranges::range_size_t<R>;
			return static_cast<idx_t>(
				between<idx_t>(0, static_cast<idx_t>(std::ranges::size(collection))));
		}

		// get an iterator to a random element. Accepts const and non-const ranges
		template <std::ranges::forward_range R>
			requires std::ranges::sized_range<R>
		constexpr auto iterator(R&& collection) noexcept{
			assert(!std::ranges::empty(collection) && "Random::iterator(): empty collection");
			auto idx = index(collection);             // index accepts const&
			auto it = std::ranges::begin(collection); // picks begin or cbegin for us
			std::advance(it, idx);
			return it;
		}

		//return a reference to a random element in a collection
		//accepts both const and non-const ranges
		template <std::ranges::forward_range R>
			requires std::ranges::sized_range<R>
		constexpr auto element(R&& collection) noexcept{
			return *iterator(std::forward<R>(collection));
		}

		template <std::floating_point F>
		constexpr F gaussian(F mean, F stddev) noexcept{
			// Based on the Central Limit Theorem; https://en.wikipedia.org/wiki/Central_limit_theorem
			// the Irwin–Hall distribution (sum of 12 U(0,1) has mean = 6, variance = 1).
			// Subtract 6 and multiply by stddev to get an approximate N(mean, stddev) sample.
			F sum{};
			for(auto i = 0; i < 12; ++i){
				sum += normalized<F>();
			}
			return mean + (sum - F(6)) * stddev;
		}

		// Returns n random bits as T, taking the high n bits of the engine output.
		// Preconditions: 0 < n <= value_bits and n <= digits(T).		
		template <class T = result_type>
		constexpr T bits(unsigned n) noexcept{
			static_assert(std::is_unsigned_v<T>, "bits<T>(n) requires an unsigned T");
			assert(n > 0 && n <= value_bits);
			assert(n <= std::numeric_limits<T>::digits);
			const result_type x = next();
			if(n >= value_bits){  // take all bits; safe fallback if asserts are disabled in release builds
				return static_cast<T>(x);
			}
			const auto mask = (result_type(1) << n) - 1;
			const auto shift = value_bits - n;
			const auto v = (x >> shift) & mask;
			return static_cast<T>(v);
		}

		// Compile-time overload, when number of bits are known at compile time
		// Example: rng.bits<8>() or rng.bits<16, std::uint32_t>()
		template <unsigned N, class T = result_type>
		constexpr T bits() noexcept{
			static_assert(N > 0 && N <= value_bits, "Can only extract [1 – std::numeric_limits<result_type>::digits] bits");
			static_assert(std::is_unsigned_v<T>, "bits<N, T> requires an unsigned T");
			static_assert(N <= std::numeric_limits<T>::digits, "Not enough bits in return type T to hold N bits");
			return bits<T>(N); //N is a constant; the optimizer will inline and constant-fold this call
		}

		//convenience overload: fill a T with random bits
		template <class T>
		constexpr T bits_as() noexcept{
			static_assert(std::is_unsigned_v<T>, "bits<T>() requires an unsigned T");
			constexpr unsigned N = std::numeric_limits<T>::digits;
			return bits<N, T>();
		}
	};
} //namespace rnd
