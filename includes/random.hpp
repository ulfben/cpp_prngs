#pragma once

#include "detail/compat.hpp"
#include "detail/portable_wide_multiply.hpp" //for constexpr and portable 128-bit multiplication
#include <assert.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>

// This is an RNG interface that wraps around any engine that meets the RandomBitEngine requirements.
// It provides useful functions for generating values, including integers, floating-point numbers, weighted picks,
// as well as methods for Gaussian distribution, coin flips (with odds), picking from collections (index or element), etc.
//
// detail::compat isolates the small portability differences needed to keep this
// implementation shared between full standard-library environments and more
// constrained C++17 environments such as Arduino AVR. Where the standard library
// provides the required facilities, the compatibility layer simply forwards to
// them; otherwise it supplies the minimal equivalents needed by Random.
//
// Source: https://github.com/ulfben/cpp_prngs/
// Demo is available on Compiler Explorer: https://compiler-explorer.com/z/PrjqfrP5z
// Benchmarks: https://github.com/ulfben/cpp_prngs/#performance-benchmarks

// Keep the C++20 RandomBitEngine concept available on toolchains that support it,
// without making the C++17-compatible Random implementation depend on concepts.
#if defined(__cpp_concepts) && __cpp_concepts >= 201907L
#include "concepts.hpp"
#endif

namespace rnd {

	template <class E>
	class Random final{
		using engine_result_type = typename E::result_type;
		static constexpr unsigned value_bits = detail::bit_width<engine_result_type>(); //same as: std::numeric_limits<typename E::result_type>::digits;

		// The fast range reduction below assumes that every bit pattern is a possible
		// engine result. These assertions say that contract directly and, unlike a
		// C++20 concept, still give a useful message when this header is used as C++17.
		static_assert(detail::supported_uint<engine_result_type>, "Random<E> requires an 8-, 16-, 32-, or 64-bit unsigned result_type");
		static_assert((E::min)() == engine_result_type{0},
			"Random<E> requires an engine whose minimum is zero");
		static_assert((E::max)() == static_cast<engine_result_type>(~engine_result_type{0}), // Set every bit to 1, equivalent to std::numeric_limits<engine_result_type>::max().
			"Random<E> requires an engine spanning its complete result_type");

		template <class T>
		static constexpr bool valid_weight_type =
			detail::supported_uint<T> &&
			(sizeof(detail::remove_cvref_t<T>) <= sizeof(engine_result_type));

	public:
		using engine_type = E;
		using result_type = typename E::result_type;
		using seed_type = typename E::seed_type;

		constexpr Random() noexcept = default; //the engine supplies its own sensible default seed.
		explicit constexpr Random(seed_type seed_value) noexcept : _engine(seed_value){}
		explicit constexpr Random(engine_type engine) noexcept : _engine(engine){}

		constexpr bool operator==(const Random& rhs) const noexcept{ return _engine == rhs._engine; }
		constexpr bool operator!=(const Random& rhs) const noexcept{ return !(*this == rhs); }

		// Direct engine access is useful for manual serialization, debugging, or any
		// engine-specific operation that the friendly Random wrapper does not expose.
		constexpr const engine_type& engine() const noexcept{ return _engine; }
		constexpr engine_type& engine() noexcept{ return _engine; }

		constexpr void seed() noexcept{ _engine.seed(); } //restore the default-constructed state.
		constexpr void seed(seed_type value) noexcept{ _engine.seed(value); }

		// Advance the random engine n steps.
		// Some engines (like PCG32) can do this faster than linear time
		constexpr void discard(unsigned long long count) noexcept{ _engine.discard(count); }

		// Parentheses prevent expansion of Arduino's unfortunately common min/max macros... :/
		static constexpr result_type (min)() noexcept{
			return result_type{0};
		}

		static constexpr result_type (max)() noexcept{
			return (E::max)();
		}

		// --- raw values/bits ---

		// Produce one raw engine value in [min(), max()], inclusive.
		constexpr result_type next() noexcept{ return _engine(); }
		constexpr result_type operator()() noexcept{ return next(); }

		// Runtime bit extraction. Returns n random bits in the low end of T.
		// We take high bits from the engine because those are the bits most
		// small PRNGs are designed to make strongest. If T is wider than one engine
		// result, gather_high_bits() stitches together as many draws as are needed.
		template <class T = result_type>
		constexpr T bits(unsigned n) noexcept{
			static_assert(detail::supported_uint<T>, "Random::bits<T>() requires an 8-, 16-, 32-, or 64-bit unsigned type");
			assert(n > 0 && n <= detail::bit_width<T>());
			if(n <= value_bits){
				return take_high_bits<T>(next(), n);
			}
			return gather_high_bits<T>(n);
		}

		// Bit extraction with a compile-time bit count.
		// Returns N random bits in the low end of T, with N known at compile time.
		// This can be more efficient than bits(unsigned), because the compiler can
		// specialize the code for the exact bit count and eliminate unused branches.
		template <unsigned N, class T = result_type>
		constexpr T bits() noexcept{
			static_assert(N > 0, "Random::bits<N>() needs at least one bit");
			static_assert(detail::supported_uint<T>, "Random::bits<N, T>() requires an 8-, 16-, 32-, or 64-bit unsigned type");
			static_assert(N <= detail::bit_width<T>(), "T cannot hold N bits");
			if constexpr(N <= value_bits){
				return take_high_bits<T>(next(), N);
			}else{
				return gather_high_bits<T>(N);
			}
		}

		// Convenience spelling for "fill a T with random bits".
		template <class T>
		constexpr T bits_as() noexcept{
			return bits<detail::bit_width<T>(), T>();
		}

		// Fill a buffer with random bit patterns, using the engine output efficiently
		// across the whole batch. This is useful when many raw random values are
		// needed and can avoid discarding unused bits from individual engine draws.
		// see gaussian(mean, stdev) for an example use case.
		// 
		// The implementation looks branchy, but all width comparisons are compile-time
		// constants. For any given engine/T combination, if constexpr discards all but
		// one path, leaving simple fixed-width loops that compilers can optimize well.
		template <class T>
		constexpr void fill_bits(T* buffer, size_t count) noexcept{
			static_assert(detail::supported_uint<T>, "Random::fill_bits<T>() requires an 8-, 16-, 32-, or 64-bit unsigned type");
			assert(buffer != nullptr || count == 0);
			if(buffer == nullptr || count == 0){
				return;
			}
			constexpr unsigned target_bits = detail::bit_width<T>();

			if constexpr(value_bits == target_bits){
				for(size_t i = 0; i < count; ++i){
					buffer[i] = static_cast<T>(next());
				}
			}else if constexpr(value_bits > target_bits){
				static_assert(value_bits % target_bits == 0, "Random::fill_bits<T>() requires evenly divisible bit widths");
				constexpr unsigned values_per_draw = value_bits / target_bits;
				size_t i{};
				while(i < count){
					const result_type value = next();
					for(unsigned part = 0; part < values_per_draw && i < count; ++part){
						const unsigned shift = value_bits - target_bits * (part + 1);
						buffer[i++] = static_cast<T>(value >> shift);
					}
				}
			}else{
				static_assert(target_bits % value_bits == 0, "Random::fill_bits<T>() requires evenly divisible bit widths");
				constexpr unsigned draws_per_value = target_bits / value_bits;
				for(size_t i = 0; i < count; ++i){
					T value{};
					for(unsigned part = 0; part < draws_per_value; ++part){
						value = static_cast<T>((value << value_bits) | static_cast<T>(next()));
					}
					buffer[i] = value;
				}
			}
		}

		// Derive a child generator by consuming enough parent output to fill one seed.
		// This is handy when you need multiple generators for different purposes, or
		// running in different threads.
		[[nodiscard]] constexpr Random split() noexcept{
			return Random{bits_as<seed_type>()};
		}

		// --- integers ---

		// Produce [0, bound) using Lemire's multiply-high range reduction with its
		// rejection step. The rejection is necessary for non-power-of-two bounds:
		// multiply-high alone maps some source values to one result more often than
		// others, which is especially visible for narrow engines.
		// See: https://lemire.me/blog/2016/06/27/a-fast-alternative-to-the-modulo-reduction/
		// The nearly-divisionless fast/slow-path arrangement follows Tony Finch's
		// explanation at: https://dotat.at/@/2025-03-05-lemire-inline.html
		constexpr result_type next(result_type bound) noexcept{
			assert(bound > 0 && "Random::next(bound): bound must be positive.");

			result_type value = next();
			auto product = multiply_parts(value, bound);
			if(product.lo >= bound){
				return static_cast<result_type>(product.hi);
			}

			// Only the small low-product region needs the threshold. Keeping
			// this modulo out of the common path is important for engines where
			// integer division costs more than generating the next value.
			// (-bound) modulo 2^value_bits is the size of the incomplete low
			// product region. Values below it are rejected so every result in
			// [0, bound) has the same number of accepted source values.
			const result_type threshold = static_cast<result_type>(-bound) % bound;
			while(product.lo < threshold){
				value = next();
				product = multiply_parts(value, bound);
			}
			return static_cast<result_type>(product.hi);
		}

		constexpr result_type operator()(result_type bound) noexcept{ return next(bound); }

		// Bounded generation with a bound known at compile time and an optional result type.
		// This lets the compiler specialize for Bound: 1 needs no random draw, powers of two
		// can use exact bit extraction, and other constant bounds use a rejection threshold
		// that is computed at compile time.
		template <result_type Bound, class T = result_type>
		constexpr T next() noexcept{
			static_assert(Bound > 0, "Random::next<Bound>(): bound must be positive");
			static_assert(detail::supported_integer<T>, "Random::next<Bound, T>() requires a supported fixed-width integer type");
			static_assert(uint64_t{Bound - 1} <= detail::integral_max<T>(), "Bound is too large for return type T");
			if constexpr(Bound == 1){
				return T{0}; // The only possible result is 0, so no random draw is needed.
			}else if constexpr((Bound & (Bound - 1)) == 0){ // if Bound is a power of two, we can use a mask / bit-extract.
				using U = detail::unsigned_t<T>;
				return static_cast<T>(bits<detail::power_of_two_exponent(Bound), U>());
			}else{
				// Finch's constantly-divisionless form: Bound is known here, so the
				// rejection threshold is folded at compile time and the loop contains
				// only the product, low-half comparison, and occasional redraw.
				constexpr result_type threshold = static_cast<result_type>(-Bound) % Bound;
				auto product = multiply_parts(next(), Bound);
				while(product.lo < threshold){
					product = multiply_parts(next(), Bound);
				}
				return static_cast<T>(product.hi);
			}
		}

		// Integer in [lo, hi).
		template <class I, detail::enable_if_t<detail::supported_integer<I>, int> = 0>
		constexpr I between(I lo, I hi) noexcept{
			if(!(lo < hi)){
				assert(false && "Random::between(lo, hi): inverted or empty range.");
				return lo;
			}
			using U = detail::unsigned_t<I>; // (portable but) equivalent to std::make_unsigned_t<I>;
			const U bound = static_cast<U>(hi) - static_cast<U>(lo);
			assert(uint64_t{bound} <= uint64_t{(max)()} && "Random::between(lo, hi): range is too large for this engine.");
			return static_cast<I>(static_cast<U>(lo) +
				static_cast<U>(next(static_cast<result_type>(bound))));
		}

		// --- floating point ---

		// Real in [0.0,1.0). We generate exactly the number of random bits that fit in
		// F's mantissa, so desktop binary64 double keeps its full precision while AVR's
		// 32-bit double naturally follows the binary32 path.
		//
		// Turning those bits into a real number is the platform-sensitive part. The
		// compatibility layer documents and selects among constexpr std::bit_cast,
		// runtime memcpy, and exact power-of-two scaling. Random only has to supply the
		// right bits—which keeps the actual RNG algorithm pleasantly easy to follow.
		template <class F = float>
		RND_DETAIL_FLOAT_CONSTEXPR F normalized() noexcept{
			static_assert(detail::supported_float<F>,
				"Random floating-point functions require IEEE-754 binary32 float or binary32/binary64 double");
			using real_type = detail::remove_cvref_t<F>; // What type of float are we really dealing with?
			using UInt = detail::unsigned_t<real_type>; // Pick wide enough unsigned int type for F
			constexpr unsigned mantissa_bits = detail::float_traits<real_type>::mantissa_bits; // Number of mantissa bits for F (e.g., 23 for float)
			const UInt mantissa = bits<mantissa_bits, UInt>();  // get random bits to fill the mantissa field
			return detail::unit_float_from_mantissa<real_type>(mantissa);
		}

		// Real in [-1.0, 1.0).
		template <class F = float>
		RND_DETAIL_FLOAT_CONSTEXPR F signed_norm() noexcept{
			static_assert(detail::supported_float<F>,
				"Random::signed_norm() requires a supported floating-point type");
			return F{2} * normalized<F>() - F{1}; // scale to [0.0, 2.0), then shift to [-1.0, 1.0)
		}

		// Real in [lo, hi).
		template <class F, detail::enable_if_t<detail::supported_float<F>, int> = 0>
		RND_DETAIL_FLOAT_CONSTEXPR F between(F lo, F hi) noexcept{
			assert(lo < hi && "Random::between(lo, hi): inverted or empty range.");
			return lo + (hi - lo) * normalized<F>();
		}

		// --- probability/distributions ---

		// A fair coin from the high bit of one engine result.
		constexpr bool coin_flip() noexcept{
			return bits<1, unsigned>() != 0;
		}

		// A weighted coin: true with probability in [0, 1].
		template <class F = float>
		RND_DETAIL_FLOAT_CONSTEXPR bool coin_flip(F probability) noexcept{
			static_assert(detail::supported_float<F>, "Random::coin_flip(probability) requires a supported floating-point type");
			assert(F{0} <= probability && probability <= F{1} && "Random::coin_flip(probability): probability must be in [0, 1].");
			return normalized<F>() < probability;
		}

		// This is the pleasantly simple Irwin-Hall approximation to a normal
		// distribution. The sum of twelve U(0,1) samples has mean 6 and variance
		// 1, so subtracting 6 and applying mean/stddev gives an approximate normal.
		// See: https://en.wikipedia.org/wiki/Irwin-Hall_distribution
		//
		// Narrow engines use a 16-bit integer form to avoid repeatedly
		// constructing floating-point values from multiple engine draws. 
		// fill_bits() produces twelve 16-bit lanes, which are summed as integers
		// and converted to floating point only once. Benchmarks showed this to be
		// substantially faster for 8- and 16-bit engines, but slower for engines
		// 32 bits and wider.
		// 
		// SmallFast8 => 2.9x faster 
		// SmallFast16 => 3.9x faster 
		// SmallFast32 / QuarkBurst64 => ~1.5x slower
		template <class F = float>
		RND_DETAIL_FLOAT_CONSTEXPR F gaussian(F mean, F stddev) noexcept{
			static_assert(detail::supported_float<F>, "Random::gaussian() requires a supported floating-point type");
			assert(stddev >= F{0} && "Random::gaussian(mean, stddev): standard deviation must be non-negative.");
			if constexpr(value_bits >= 32){
				// On desktop-width engines this direct form was faster in my benchmarks.
				F sum{};
				for(unsigned i = 0; i < 12; ++i){
					sum += normalized<F>();
				}
				return mean + (sum - F{6}) * stddev;
			}else{				
				uint16_t lanes[12]{};
				fill_bits<uint16_t>(lanes, 12);
				uint32_t sum{};
				for(const uint16_t lane : lanes){
					sum += static_cast<uint32_t>(lane);
				}
				// 6 is 12 midpoint corrections of 0.5; 65536 is 2^16, the number of equally likely values in a uint16_t lane.
				const F normalized_sum = (static_cast<F>(sum) + F{6}) / F{65536};
				return mean + (normalized_sum - F{6}) * stddev;
			}
		}

		// --- collections ---
		//
		// data() and size() define the supported contiguous storage model. begin()
		// is used only when iterator() must return the collection's native iterator.
		//
		// std::span looks like the natural parameter type here, but a function such
		// as element(std::span<T>) cannot deduce T from a vector or array: template
		// deduction does not consider the conversion to span. Callers would have to
		// write rng.element(std::span{items}) themselves. These overloads preserve
		// the friendly element(items) spelling, while pointer-and-size remains the
		// baseline API.

		// Pick an index in [0, size).
		[[nodiscard]] constexpr size_t index(size_t size) noexcept{
			assert(size != 0 && "Random::index(): empty collection.");
			assert(size <= static_cast<size_t>((max)()) && "Random::index(): collection is too large for this engine.");
			return static_cast<size_t>(next(static_cast<result_type>(size)));
		}

		template <class C,
			detail::enable_if_t<detail::contiguous_collection<const C>::value, int> = 0> // equivalent to 'requires contiguous_collection<const C>' in C++20
		[[nodiscard]] constexpr size_t index(const C& collection) noexcept{
			return index(static_cast<size_t>(detail::collection_size(collection)));
		}

		// Get an iterator to a random element. Const collections naturally return a
		// const iterator; pointer-defined ranges naturally return a pointer.
		template <class T>
		[[nodiscard]] constexpr T* iterator(T* collection, size_t size) noexcept{
			assert(collection != nullptr && "Random::iterator(): null collection.");
			return collection + index(size);
		}

		template <class C,
			detail::enable_if_t<detail::contiguous_collection<C>::value, int> = 0> // equivalent to 'requires contiguous_collection<C>' in C++20
		[[nodiscard]] constexpr auto iterator(C& collection) noexcept{
			return detail::collection_begin(collection) +
				index(static_cast<size_t>(detail::collection_size(collection)));
		}

		// Return the selected element by reference, preserving constness.
		template <class T>
		[[nodiscard]] constexpr T& element(T* collection, size_t size) noexcept{
			return *iterator(collection, size);
		}

		template <class C,
			detail::enable_if_t<detail::contiguous_collection<C>::value, int> = 0> // equivalent to 'requires contiguous_collection<C>' in C++20
		[[nodiscard]] constexpr decltype(auto) element(C& collection) noexcept{
			return *iterator(collection);
		}

		// --- weighted collections ---
		//
		// Zero weights exclude an item. At least one weight must be positive, and the
		// sum must fit in the engine's result_type. The latter keeps the bounded draw
		// exact and is why an 8-bit engine intentionally accepts smaller totals.

		// Pick an index proportionally to a simple array of unsigned weights.
		template <class W, detail::enable_if_t<valid_weight_type<W>, int> = 0>
		[[nodiscard]] constexpr size_t weighted_index(const W* weights, size_t size) noexcept{
			if(weights == nullptr){
				assert(false && "Random::weighted_index(): null weight collection.");
				abort();
			}
			auto weight_at = [weights](size_t i) constexpr -> decltype(auto){ return weights[i]; };
			return weighted_offset(size, weight_at);
		}

		template <class C,
			detail::enable_if_t<detail::valid_weight_collection<C, result_type>::value, int> = 0>
		[[nodiscard]] constexpr size_t weighted_index(const C& weights) noexcept{
			return weighted_index(
				detail::collection_data(weights),
				static_cast<size_t>(detail::collection_size(weights)));
		}

		// Select from objects by projecting each object to its weight. A projection
		// may be a lambda, a pointer to a member function, or a pointer to a data
		// member. It must be noexcept and return a supported unsigned weight type.
		// See README.md and demo.cpp for examples
		template <class T, class Projection,
			detail::enable_if_t<detail::valid_projection<T, Projection, result_type>::value, int> = 0>
		[[nodiscard]] constexpr T* weighted_iterator(
			T* collection, size_t size, Projection projection) noexcept{
			return collection + projected_weighted_offset(collection, size, projection);
		}

		template <class C, class Projection,
			detail::enable_if_t<detail::valid_projected_collection<C, Projection, result_type>::value, int> = 0>
		[[nodiscard]] constexpr auto weighted_iterator(C& collection, Projection projection) noexcept{
			const size_t offset = projected_weighted_offset(
				detail::collection_data(collection),
				static_cast<size_t>(detail::collection_size(collection)),
				projection);
			return detail::collection_begin(collection) + offset;
		}

		template <class T, class Projection,
			detail::enable_if_t<detail::valid_projection<T, Projection, result_type>::value, int> = 0>
		[[nodiscard]] constexpr T& weighted_element(
			T* collection, size_t size, Projection projection) noexcept{
			return *weighted_iterator(collection, size, projection);
		}

		template <class C, class Projection,
			detail::enable_if_t<detail::valid_projected_collection<C, Projection, result_type>::value, int> = 0>
		[[nodiscard]] constexpr decltype(auto) weighted_element(C& collection, Projection projection) noexcept{
			return *weighted_iterator(collection, projection);
		}

	private:
		engine_type _engine{}; //the small engine that supplies all of our random bits.

		// Mask the low n bits without ever shifting by the full width of T (which
		// would be undefined behavior). n == width therefore gets the all-ones path.
		template <class T>
		static constexpr T low_bits_mask(unsigned n) noexcept{
			return n >= detail::bit_width<T>()
				? static_cast<T>(~T{0})
				: static_cast<T>((T{1} << n) - T{1});
		}

		// Move the strongest n bits down to the low end of the requested result type.
		template <class T>
		static constexpr T take_high_bits(result_type value, unsigned n) noexcept{
			assert(n > 0 && n <= value_bits && n <= detail::bit_width<T>());
			return static_cast<T>(value >> (value_bits - n)) & low_bits_mask<T>(n);
		}

		// Fill T one engine-width chunk at a time.
		template <class T>
		constexpr T gather_high_bits(unsigned n) noexcept{
			assert(value_bits < n && n <= detail::bit_width<T>());

			T result{};
			unsigned filled{};
			while(filled < n){
				const unsigned remaining = n - filled;
				const unsigned take = remaining < value_bits ? remaining : value_bits;
				const T chunk = take_high_bits<T>(next(), take);

				// filled is always less than detail::bit_width<T>(), so this shift is safe.
				result = static_cast<T>(result | static_cast<T>(chunk << filled));
				filled += take;
			}

			// When n == detail::bit_width<T>(), low_bits_mask() returns all ones.
			return static_cast<T>(result & low_bits_mask<T>(n));
		}

		// First pass of weighted selection: validate and add the weights without ever
		// allowing the sum to wrap. Assertions explain mistakes in debug builds;
		// abort() keeps a violated precondition from turning into undefined behavior
		// when assertions are compiled out.
		template <class WeightAt>
		constexpr result_type total_weight(size_t size, WeightAt& weight_at) noexcept{
			using weight_type = detail::remove_cvref_t<decltype(weight_at(size_t{}))>;
			static_assert(valid_weight_type<weight_type>, "Weights must be non-boolean unsigned integers no wider than result_type");
			result_type total{};
			for(size_t i = 0; i < size; ++i){
				const result_type weight = static_cast<result_type>(weight_at(i));
				if(weight > (max)() - total){
					assert(false && "Random::weighted_index(): total weight is too large for this engine.");
					abort();
				}
				total += weight;
			}
			return total;
		}

		// Turn a projection into the same index-based callable used by plain weights.
		template <class T, class Projection>
		constexpr size_t projected_weighted_offset(T* collection, size_t size, Projection& projection) noexcept{
			if(collection == nullptr){
				assert(false && "Random::weighted_iterator(): null collection.");
				abort();
			}
			static_assert(detail::valid_projection<T, Projection, result_type>::value, "Projection must be noexcept and return a valid unsigned weight type");
			auto weight_at = [collection, &projection](size_t i) constexpr noexcept -> decltype(auto){
				return detail::invoke(projection, collection[i]);
			};
			return weighted_offset(size, weight_at);
		}

		// Weighted selection is deliberately two-pass: one pass computes total, one
		// random draw chooses a target in [0,total), and the second pass locates it.
		// Projections must therefore return stable weights during both passes.
		template <class WeightAt>
		constexpr size_t weighted_offset(size_t size, WeightAt& weight_at) noexcept{
			if(size == 0){
				assert(false && "Random::weighted_index(): empty weight collection.");
				abort();
			}
			result_type total = total_weight(size, weight_at);
			if(total == 0){
				assert(false && "Random::weighted_index(): at least one weight must be positive.");
				abort();
			}
			result_type target = next(total);
			for(size_t i = 0; i < size; ++i){
				const result_type weight = static_cast<result_type>(weight_at(i));
				if(target < weight){
					return i;
				}
				target -= weight;
			}
			assert(false && "Random::weighted_index(): weights changed during selection.");
			abort();
		}

		// Multiply in the next wider integer type and retain both halves of the
		// full-width product. A 64-bit engine needs the portable 64x64->128 helper
		// because __uint128_t is not available on every desktop compiler (notably
		// MSVC) or on AVR.
		static constexpr detail::u128_parts multiply_parts(result_type value, result_type bound) noexcept{
			if constexpr(sizeof(result_type) == 1){
				const uint16_t product = uint16_t{value} * uint16_t{bound};
				return {static_cast<uint64_t>(static_cast<result_type>(product)),
					static_cast<uint64_t>(product >> 8)};
			}else if constexpr(sizeof(result_type) == 2){
				const uint32_t product = uint32_t{value} * uint32_t{bound};
				return {static_cast<uint64_t>(static_cast<result_type>(product)),
					static_cast<uint64_t>(product >> 16)};
			}else if constexpr(sizeof(result_type) == 4){
				const uint64_t product = uint64_t{value} * uint64_t{bound};
				return {product & UINT32_MAX, product >> 32};
			}else{
				return detail::mul64_to_128_parts(value, bound);
			}
		}
	};

} // namespace rnd

#undef RND_DETAIL_FLOAT_CONSTEXPR
