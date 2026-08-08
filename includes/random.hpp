#pragma once
#include "concepts.hpp" //for RandomBitEngine concept
#include "detail/wide_multiply.hpp" //for constexpr and portable 128-bit multiplication
#include <algorithm>
#include <bit> // for std::bit_cast
#include <cassert>
#include <concepts>
#include <cstdint>
#include <iterator>
#include <limits>
#include <ranges>
#include <type_traits>
#include <utility>

// This is an RNG interface that wraps around any engine that meets the RandomBitEngine concept.
// It provides useful functions for generating values, including integers, floating-point numbers, weighted picks,
// as well as methods for Gaussian distribution, coin flips (with odds), picking from collections (index or element), etc.
// Source: https://github.com/ulfben/cpp_prngs/
// Demo is available on Compiler Explorer: https://compiler-explorer.com/z/zTh6nazxj
// Benchmarks: https://github.com/ulfben/cpp_prngs/#performance-benchmarks

namespace rnd {

	template <class F>
	concept supported_float =
		(std::same_as<F, float> || std::same_as<F, double>) &&
		(sizeof(F) == sizeof(std::uint32_t) || sizeof(F) == sizeof(std::uint64_t)) && //this is ... a bit belt-and-suspenders, but it's cool that we *can* check. :) 
		std::numeric_limits<F>::is_iec559; //the IQ hack in normalized() requires IEEE 754 (IEC 559) floating point types.

	template <RandomBitEngine E>
	class Random final{
		static constexpr unsigned value_bits = std::numeric_limits<typename E::result_type>::digits;	

		template <class T>
		static constexpr bool valid_weight_type =
			std::unsigned_integral<std::remove_cv_t<T>> &&
			!std::same_as<std::remove_cv_t<T>, bool> &&
			(std::numeric_limits<std::remove_cv_t<T>>::digits <= value_bits);

		template <class R, class Projection>
		using projected_weight_t = std::remove_cvref_t<std::invoke_result_t<Projection&, std::ranges::range_reference_t<R>>>;

	public:
		using engine_type = E;
		using result_type = typename E::result_type;
		using seed_type = typename E::seed_type;
		static_assert(std::is_unsigned_v<result_type>);
		static_assert(E::min() == 0);
		static_assert(E::max() == std::numeric_limits<result_type>::max());

		constexpr Random() noexcept = default; //the engine will default initialize
		explicit constexpr Random(seed_type seed_val) noexcept : _e(seed_val){}
		explicit constexpr Random(engine_type engine) noexcept : _e(engine){}
		constexpr bool operator==(const Random& rhs) const noexcept = default;

		//access to the underlying engine for manual serialization, etc.
		constexpr const E& engine() const noexcept{
			return _e;
		}

		constexpr E& engine() noexcept{
			return _e;
		}

		constexpr void seed() noexcept{
			_e.seed(); //reseed to default state
		}

		constexpr void seed(seed_type v) noexcept{
			_e.seed(v); //reseed with custom seed
		}

		//advance the random engine n steps.
		//some engines (like PCG32) can do this faster than linear time
		constexpr void discard(unsigned long long n) noexcept{
			_e.discard(n);
		}				
	
		// Returns a child generator derived from the parent.		
		// Suitable for deriving task- or thread-local random streams when strict non-overlap is not required. 
		[[nodiscard]] constexpr Random split() noexcept{
			return Random{bits_as<seed_type>()}; //consume enough engine outputs to fill one seed_type value.		
		}

		static constexpr result_type  min() noexcept{
			return 0; 
		}

		static constexpr result_type  max() noexcept{
			return E::max();
		}

		// --- raw values/bits ---

		// Produces a random value in the range [min(), max()], inclusive.
		constexpr result_type next() noexcept{
			return _e();
		}

		// Produces a random value in the range [min(), max()], inclusive.
		constexpr result_type operator()() noexcept{
			return next();
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
				return gather_bits_runtime<T>(N); // reuse runtime gather (the loop count is deterministic anyway).
			}
		}

		// Convenience: fill T with random bits.
		template <class T>
		constexpr T bits_as() noexcept{
			static_assert(std::is_unsigned_v<T>, "bits_as<T>() requires an unsigned T");
			return bits<std::numeric_limits<T>::digits, T>();
		}

		// --- integers ---

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
			return static_cast<I>(U(lo) + static_cast<U>(next(safe_bound)));
		}


		// --- floating point ---				

		// real in [0.0,1.0) using the "IQ float hack"
		//   see Iñigo Quilez, "sfrand": https://iquilezles.org/articles/sfrand/
		// Fast, branchless and, now, portable.
		template <supported_float F = float>
		constexpr F normalized() noexcept{			
			using UInt = std::conditional_t<sizeof(F) == sizeof(std::uint32_t), std::uint32_t, std::uint64_t>;  // Pick wide enough unsigned int type for F
			constexpr int mantissa_bits = std::numeric_limits<F>::digits - 1; // Number of mantissa bits for F (e.g., 23 for float)
			constexpr UInt base = std::bit_cast<UInt>(F{1}); // Bit pattern for F{1}, i.e., exponent set, mantissa 0
			const UInt mantissa = this->template bits<mantissa_bits, UInt>();      // Get random bits to fill the mantissa field
			const UInt as_int = base | mantissa; // Combine base (1.0) with random mantissa bits
			return std::bit_cast<F>(as_int) - F{1}; // Convert bits to float/double, then subtract 1.0 to get [0,1)
		}			

		// real in [-1.0,1.0) using the IQ float hack.
		template <supported_float F = float>
		constexpr F signed_norm() noexcept{
			return F{2} * normalized<F>() - F{1}; // scale to [0.0, 2.0), then shift to [-1.0, 1.0)
		}

		// real in [lo, hi)
		template <supported_float F = float>
		constexpr F between(F lo, F hi) noexcept{
			return lo + (hi - lo) * normalized<F>();
		}

		// --- probability/distributions ---

		// boolean
		constexpr bool coin_flip() noexcept{
			return bits<1, unsigned>() != 0;
		}

		// boolean with probability
		template <supported_float F = float>
		constexpr bool coin_flip(F probability) noexcept{
			return normalized<F>() < probability;
		}

		template <supported_float F = float>
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

		// --- collections ---

		// pick an index in [0, size)
		template <std::ranges::sized_range R>
		[[nodiscard]] constexpr auto index(R&& collection) noexcept{
			using size_type = std::ranges::range_size_t<R>;
			const size_type size = std::ranges::size(collection);
			assert(size != 0 && "Random::index(): empty collection.");
			assert(std::in_range<result_type>(size) &&
				"Random::index(): collection is too large for this engine.");
			return static_cast<size_type>(next(static_cast<result_type>(size)));
		}

		// get an iterator to a random element. Accepts const and non-const ranges
		template <std::ranges::forward_range R>
			requires std::ranges::sized_range<R> &&
				std::ranges::borrowed_range<R>
		[[nodiscard]] constexpr auto iterator(R&& collection) noexcept{
			const auto offset = static_cast<std::ranges::range_difference_t<R>>(index(collection));
			return std::ranges::next(std::ranges::begin(collection), offset);
		}

		//return a reference to a random element in a collection
		//accepts both const and non-const ranges
		template <std::ranges::forward_range R>
			requires std::ranges::sized_range<R> &&
				std::ranges::borrowed_range<R>
		[[nodiscard]] constexpr decltype(auto) element(R&& collection) noexcept{
			return *iterator(std::forward<R>(collection));
		}

		// Pick an index proportionally to unsigned integral weights.
		// A zero weight excludes the corresponding index from selection.
		// At least one weight must be positive and the sum must fit in result_type.
		template <std::ranges::forward_range R>
			requires std::ranges::sized_range<R> &&
				valid_weight_type<std::ranges::range_value_t<R>>
		[[nodiscard]] constexpr auto weighted_index(R&& weights) noexcept{
			using size_type = std::ranges::range_size_t<R>;
			assert(!std::ranges::empty(weights) && "Random::weighted_index(): empty weight range.");

			result_type total{};
			for(const auto weight : weights){
				assert(weight <= max() - total && "Random::weighted_index(): total weight is too large for this engine.");
				total += weight;
			}
			assert(total != 0 && "Random::weighted_index(): at least one weight must be positive.");

			result_type target = next(total);
			size_type selected{};
			for(const auto weight : weights){
				if(target < weight){
					return selected;
				}
				target -= weight;
				++selected;
			}			
			std::unreachable(); //with stable weights, a positive total, and target < total, the loop must return.
		}

		// Get an iterator to an element selected proportionally to its projected weight.
		// A zero weight excludes the corresponding element from selection.
		// At least one projected weight must be positive, the sum must fit in
		// result_type, and the projection must return stable weights across both passes.
		template <std::ranges::forward_range R, class Projection>
			requires std::ranges::sized_range<R>&&
				std::ranges::borrowed_range<R>&&
				std::invocable<Projection&, std::ranges::range_reference_t<R>>&&
				valid_weight_type<projected_weight_t<R, Projection>>
		[[nodiscard]] constexpr auto weighted_iterator(R&& collection, Projection projection) noexcept{
			auto weights = collection | std::views::transform(std::move(projection));
			const auto offset = static_cast<std::ranges::range_difference_t<R>>(weighted_index(weights));
			return std::ranges::next(std::ranges::begin(collection), offset);
		}

		// Return a reference to an element selected proportionally to its projected weight.
		// Note: if your projection throws an exception, we die.
		template <std::ranges::forward_range R, class Projection>
			requires std::ranges::sized_range<R>&&
				std::ranges::borrowed_range<R>&&
				std::invocable<Projection&, std::ranges::range_reference_t<R>>&&
				valid_weight_type<projected_weight_t<R, Projection>>
		[[nodiscard]] constexpr decltype(auto) weighted_element(R&& collection, Projection projection) noexcept{
			return *weighted_iterator(std::forward<R>(collection), std::move(projection));
		}			

	private:
		E _e{}; //the underlying engine providing random bits. This class will turn those into useful values.

		template <class T>
		static constexpr T low_bits_mask(unsigned n) noexcept{
			assert(n <= std::numeric_limits<T>::digits); // n in [0, digits(T)]
			constexpr unsigned W = std::numeric_limits<T>::digits;
			if(n == 0) return T{0};
			if(n >= W) return std::numeric_limits<T>::max(); // avoid UB on (1<<W)
			return static_cast<T>((T{1} << n) - T{1});
		}

		template <class T>
		constexpr T take_high_bits(E::result_type x, unsigned n) noexcept{
			assert(1 <= n && n <= std::numeric_limits<T>::digits); // Preconditions: 1 <= n <= value_bits, and n <= digits(T)
			const unsigned shift = value_bits - n;    // shift in [0, value_bits-1]
			return static_cast<T>(x >> shift) & low_bits_mask<T>(n);
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
			// If n == digits(T), low_bits_mask returns all-ones, so this is cheap and safe.
			return acc & low_bits_mask<T>(n);
		}
	};
} //namespace rnd
