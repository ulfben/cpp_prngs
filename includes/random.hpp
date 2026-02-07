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
#include "detail.hpp"   //for constexpr and portable 128-bit multiplication

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
	template <RandomBitEngine E>
	class Random final{
		static constexpr unsigned value_bits = std::numeric_limits<E::result_type>::digits;
		E _e{}; //the underlying engine providing random bits. This class will turn those into useful values.
		
		template <class T>
		static constexpr T mask_low(unsigned n) noexcept{			
			assert(n <= std::numeric_limits<T>::digits); // n in [0, digits(T)]
			constexpr unsigned W = std::numeric_limits<T>::digits;
			if(n == 0) return T{0};
			if(n >= W) return std::numeric_limits<T>::max(); // avoid UB on (1<<W)
			return (T{1} << n) - T{1};
		}
			
		template <class T>
		constexpr T take_high_bits(E::result_type x, unsigned n) noexcept{
			assert(1 <= n && n <= std::numeric_limits<T>::digits); // Preconditions: 1 <= n <= value_bits, and n <= digits(T)
			const unsigned shift = value_bits - n;    // shift in [0, value_bits-1]
			return static_cast<T>(x >> shift) & mask_low<T>(n);
		}
		
		template <class T>
		constexpr T gather_bits_runtime(unsigned n) noexcept{
			assert(1 <= n && n <= std::numeric_limits<T>::digits); // Preconditions: 1 <= n <= digits(T)			
			T acc = 0;
			unsigned filled = 0;
			while(filled < n){
				const unsigned take = std::min<unsigned>(value_bits, n - filled);
				const T chunk = take_high_bits<T>(next(), take);
				acc |= (chunk << filled);             // filled < digits(T) always holds here
				filled += take;
			}
			// If n == digits(T), mask_low returns all-ones, so this is cheap and safe.
			return acc & mask_low<T>(n);
		}
	public:
		using engine_type = E;
		using result_type = typename E::result_type;
		using seed_type = typename E::seed_type;
		static_assert(std::is_unsigned_v<result_type>);
		static_assert(E::min() == 0);
		static_assert(E::max() == std::numeric_limits<result_type>::max());

		constexpr Random() noexcept = default; //the engine will default initialize

		explicit constexpr Random(engine_type engine) noexcept : _e(engine){}

		explicit constexpr Random(seed_type seed_val) noexcept : _e(seed_val){};

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
		constexpr void discard(result_type n) noexcept{
			_e.discard(n);
		}

		constexpr void seed() noexcept{
			_e = E{};
		}

		constexpr void seed(seed_type v) noexcept{
			_e.seed(v);
		}

		// returns a decorrelated, forked engine; advances this engine's state 2 steps.
		// use for parallel or independent streams use (think: task/thread-local randomness)
		// consumes two outputs from the current engine to ensure the new engine's state is well-separated from the current one			
		constexpr Random<E> split() noexcept{
			constexpr std::uint64_t tag = 0x53504C49542D3031ULL;  //the tag ensures split() uses a distinct seed domain			
			std::uint64_t a = bits_as<std::uint64_t>(); // Always get full-width 64-bit material, even from 16/32-bit engines.
			std::uint64_t b = bits_as<std::uint64_t>();			
			std::uint64_t seed = (a ^ std::rotl(b, 32)) ^ tag; // Mix two consecutive pulls + domain-separating tag			
			
			// xnasam avalanche, inlined. See: seeding.hpp for details.
			seed ^= 0x9E3779B97F4A7C15ULL;
			seed ^= std::rotr(seed, 25) ^ std::rotr(seed, 47);
			seed *= 0x9E6C63D0676A9A99ULL;
			seed ^= (seed >> 23) ^ (seed >> 51);
			seed *= 0x9E6D62D06F6A9A9BULL;
			seed ^= (seed >> 23) ^ (seed >> 51);

			return Random<E>{ seed };
		}


		static constexpr auto min() noexcept{
			return 0; 
		}

		static constexpr auto max() noexcept{
			return E::max();
		}

		// Produces a random value in the range [min(), max()], inclusive.
		constexpr result_type next() noexcept{
			return _e();
		}

		// Produces a random value in the range [min(), max()], inclusive.
		constexpr result_type operator()() noexcept{
			return next();
		}
						
		// Produces a random value in [0, bound) (exclusive) via multiply-high range reduction (Lemire-style).
		// This much faster than naive modulo and has very small bias for non power-of-two bounds, and no bias for powers of two bounds.
		// See: https://lemire.me/blog/2016/06/27/a-fast-alternative-to-the-modulo-reduction/
		constexpr result_type next(result_type bound) noexcept{
			assert(bound > 0 && "bound must be non-zero and positive");
			result_type raw_value = next(); // raw_value is [0, 2^value_bits - 1] (i.e. min()..max(), inclusive)
			if constexpr(value_bits <= 32){ // for small engines, multiply into a 64-bit product
				auto product = std::uint64_t(raw_value) * std::uint64_t(bound);	// product < bound * 2^value_bits   (since raw_value < 2^value_bits)
				auto result = result_type(product >> value_bits); // equivalent to floor(product / 2^value_bits)
				return result;                    // result is now in range [0, bound)
			} else if constexpr(value_bits <= 64){
				// same logic, but use helper for 128-bit math, since __uint128_t isn't universally available
				return detail::mul_shift_u64<value_bits>(raw_value, bound);
			} else{ // fallback for hypothetical >64-bit engines. Naive modulo (slower, more bias)
				return bound > 0 ? raw_value % bound : bound; // avoid division by zero in release builds
			}
		}

		// Produces a random value in [0, bound) (exclusive) 
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
			if constexpr(Bound == 1){
				return T{0};
			}else if constexpr((Bound & (Bound - 1)) == 0){ // if Bound is a power of two, we can use a mask / bit-extract.
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

		// Runtime: returns n random bits in the low n bits of T.
		// Works for n > value_bits by concatenating successive outputs (high bits from each draw).
		template <class T = result_type>
		constexpr T bits(unsigned n) noexcept{
			static_assert(std::is_unsigned_v<T>, "bits<T>(n) requires an unsigned T");
			assert(n > 0);
			assert(n <= std::numeric_limits<T>::digits);
			if(n <= value_bits){
				return take_high_bits<T>(next(), n);
			}
			return gather_bits_runtime<T>(n);
		}

		// Compile-time: returns N random bits in the low N bits of T.
		template <unsigned N, class T = result_type>
		constexpr T bits() noexcept{
			static_assert(N > 0, "Need at least 1 bit");
			static_assert(std::is_unsigned_v<T>, "bits<N,T> requires an unsigned T");
			static_assert(N <= std::numeric_limits<T>::digits, "T cannot hold N bits");

			if constexpr(N <= value_bits){
				return take_high_bits<T>(next(), N);
			} else{
				// Still centralized: reuse runtime gather (the loop count is deterministic anyway).
				return gather_bits_runtime<T>(N);
			}
		}

		// Convenience: fill T with random bits.
		template <class T>
		constexpr T bits_as() noexcept{
			static_assert(std::is_unsigned_v<T>, "bits_as<T>() requires an unsigned T");
			return bits<std::numeric_limits<T>::digits, T>();
		}		
	};
} //namespace rnd
