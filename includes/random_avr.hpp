#pragma once
#include "detail/avr_compat.hpp"
#include "detail/portable_wide_multiply.hpp"
#include <assert.h>
#include <limits.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#ifdef RND_AVR_FAST_FLOAT
	#include <string.h>
#endif

// A C++17 Random<E> frontend for AVR-libc, which does not provide the standard
// C++ library facilities used by random.hpp. Include one frontend or the other;
// both intentionally expose rnd::Random<E>.
//
// C++17 has no std::bit_cast, so it cannot express Inigo Quilez's very fast
// representation-based float generation in a portable, constexpr-friendly way.
// This header therefore uses a slightly slower arithmetic implementation by
// default, preserving constexpr evaluation throughout the floating-point API.
// If compile-time floating-point generation is not needed, define
// RND_AVR_FAST_FLOAT before including this header to select the IQ implementation
// using memcpy as a C++17-safe runtime bit cast, for a small performance gain.

#ifdef RND_AVR_FAST_FLOAT
	#define RND_DETAIL_AVR_FLOAT_CONSTEXPR
#else
	#define RND_DETAIL_AVR_FLOAT_CONSTEXPR constexpr
#endif

namespace rnd {

	template <class E>
	class Random final{
		using engine_result_type = typename E::result_type;
		static constexpr unsigned value_bits = sizeof(engine_result_type) * CHAR_BIT;

		static_assert(detail::avr_supported_uint<engine_result_type>, "Random<E> requires a uint8_t, uint16_t, uint32_t, or uint64_t result_type");
		static_assert((E::min)() == engine_result_type{0}, "Random<E> requires an engine whose minimum is zero");
		static_assert((E::max)() == static_cast<engine_result_type>(~engine_result_type{0}), "Random<E> requires an engine spanning its complete result_type");

		template <class T>
		static constexpr bool valid_weight_type =
			detail::avr_supported_uint<T> &&
			(sizeof(detail::avr_remove_cvref_t<T>) <= sizeof(engine_result_type));

	public:
		using engine_type = E;
		using result_type = typename E::result_type;
		using seed_type = typename E::seed_type;

		constexpr Random() noexcept = default;
		explicit constexpr Random(seed_type seed_value) noexcept : _engine(seed_value){}
		explicit constexpr Random(engine_type engine) noexcept : _engine(engine){}

		constexpr bool operator==(const Random& rhs) const noexcept{ return _engine == rhs._engine; }
		constexpr bool operator!=(const Random& rhs) const noexcept{ return !(*this == rhs); }

		constexpr const engine_type& engine() const noexcept{ return _engine; }
		constexpr engine_type& engine() noexcept{ return _engine; }

		constexpr void seed() noexcept{ _engine.seed(); }
		constexpr void seed(seed_type value) noexcept{ _engine.seed(value); }
		constexpr void discard(unsigned long long count) noexcept{ _engine.discard(count); }

		static constexpr result_type (min)() noexcept{ // Parentheses prevent expansion of Arduino's min macro.
			return result_type{0};
		}
		static constexpr result_type (max)() noexcept{ // Parentheses prevent expansion of Arduino's max macro.
			return (E::max)();
		}

		// --- raw values/bits ---

		constexpr result_type next() noexcept{ return _engine(); }
		constexpr result_type operator()() noexcept{ return next(); }

		template <class T = result_type>
		constexpr T bits(unsigned n) noexcept{
			static_assert(detail::avr_supported_uint<T>, "Random::bits<T>() requires uint8_t, uint16_t, uint32_t, or uint64_t");
			assert(n > 0 && n <= bit_width<T>());
			return n <= value_bits ? take_high_bits<T>(next(), n) : gather_bits<T>(n);
		}

		template <unsigned N, class T = result_type>
		constexpr T bits() noexcept{
			static_assert(N > 0, "Random::bits<N>() needs at least one bit");
			static_assert(detail::avr_supported_uint<T>, "Random::bits<N, T>() requires uint8_t, uint16_t, uint32_t, or uint64_t");
			static_assert(N <= bit_width<T>(), "T cannot hold N bits");
			return N <= value_bits ? take_high_bits<T>(next(), N) : gather_bits<T>(N);
		}

		template <class T>
		constexpr T bits_as() noexcept{
			return bits<bit_width<T>(), T>();
		}

		[[nodiscard]] constexpr Random split() noexcept{
			return Random{bits_as<seed_type>()};
		}

		// --- integers ---

		// Produce [0, bound) using multiply-high in the engine's native width.
		constexpr result_type next(result_type bound) noexcept{
			assert(bound != 0 && "Random::next(bound): bound must be positive.");
			return scale_to_bound(next(), bound);
		}
		constexpr result_type operator()(result_type bound) noexcept{ return next(bound); }

		template <result_type Bound, class T = result_type>
		constexpr T next() noexcept{
			static_assert(Bound > 0, "Random::next<Bound>(): bound must be positive");
			static_assert(detail::avr_supported_integer<T>, "Random::next<Bound, T>() requires a supported fixed-width integer type");
			static_assert(uint64_t{Bound - 1} <= integral_max<T>(), "Bound is too large for return type T");
			if constexpr(Bound == 1) return T{0};
			if constexpr((Bound & (Bound - 1)) == 0){
				using U = detail::avr_unsigned_t<T>;
				return static_cast<T>(bits<power_of_two_exponent(Bound), U>());
			}
			return static_cast<T>(next(Bound));
		}

		template <class I, detail::avr_enable_if_t<detail::avr_supported_integer<I>, int> = 0>
		constexpr I between(I lo, I hi) noexcept{
			if(!(lo < hi)){
				assert(false && "Random::between(lo, hi): inverted or empty range.");
				return lo;
			}
			using U = detail::avr_unsigned_t<I>;
			const U bound = static_cast<U>(hi) - static_cast<U>(lo);
			assert(uint64_t{bound} <= uint64_t{(max)()} && "Random::between(lo, hi): range is too large for this engine.");
			return static_cast<I>(static_cast<U>(lo) + static_cast<U>(next(static_cast<result_type>(bound))));
		}

		// --- floating point ---

		template <class F = float>
		RND_DETAIL_AVR_FLOAT_CONSTEXPR F normalized() noexcept{
			static_assert(detail::avr_is_binary32<F>, "random_avr.hpp supports only binary32 float and double");
		#ifdef RND_AVR_FAST_FLOAT
			//the IQ hack implemented without c++20 (constexpr-friendly) std::bit_cast
			// eg. this is only available at runtime, but it is fast
			const uint32_t representation = UINT32_C(0x3f800000) | bits<23, uint32_t>();
			F value;
			memcpy(&value, &representation, sizeof(value)); // C++17 bit cast; AVR GCC removes the copy.
			return value - F{1};
		#else //a constexpr-friendly alternative.
			constexpr F scale = F{1} / F{8388608}; // Exact 2^-23; the constexpr alternative to a bit cast.
			return static_cast<F>(bits<23, uint32_t>()) * scale;
		#endif
		}

		template <class F = float>
		RND_DETAIL_AVR_FLOAT_CONSTEXPR F signed_norm() noexcept{
			return F{2} * normalized<F>() - F{1};
		}

		template <class F, detail::avr_enable_if_t<detail::avr_supported_float<F>, int> = 0>
		RND_DETAIL_AVR_FLOAT_CONSTEXPR F between(F lo, F hi) noexcept{
			static_assert(detail::avr_is_binary32<F>, "random_avr.hpp supports only binary32 float and double");
			assert(lo < hi && "Random::between(lo, hi): inverted or empty range.");
			return lo + (hi - lo) * normalized<F>();
		}

		// --- probability/distributions ---

		constexpr bool coin_flip() noexcept{
			return bits<1, unsigned>() != 0;
		}

		template <class F = float>
		RND_DETAIL_AVR_FLOAT_CONSTEXPR bool coin_flip(F probability) noexcept{
			static_assert(detail::avr_is_binary32<F>, "random_avr.hpp supports only binary32 float and double");
			assert(F{0} <= probability && probability <= F{1} && "Random::coin_flip(probability): probability must be in [0, 1].");
			return normalized<F>() < probability;
		}

		template <class F = float>
		RND_DETAIL_AVR_FLOAT_CONSTEXPR F gaussian(F mean, F stddev) noexcept{
			static_assert(detail::avr_is_binary32<F>, "random_avr.hpp supports only binary32 float and double");
			assert(stddev >= F{0} && "Random::gaussian(mean, stddev): standard deviation must be non-negative.");
			F sum{};
			for(unsigned i = 0; i < 12; ++i){
				sum += normalized<F>();
			}
			return mean + (sum - F{6}) * stddev;
		}

		// --- collections ---

		[[nodiscard]] constexpr size_t index(size_t size) noexcept{
			assert(size != 0 && "Random::index(): empty collection.");
			assert(size <= static_cast<size_t>((max)()) && "Random::index(): collection is too large for this engine.");
			return static_cast<size_t>(next(static_cast<result_type>(size)));
		}

		template <class T>
		[[nodiscard]] constexpr size_t index(const T* collection, size_t size) noexcept{
			assert(collection != nullptr && "Random::index(): null collection.");
			return index(size);
		}

		template <class T, size_t N>
		[[nodiscard]] constexpr size_t index(const T (&)[N]) noexcept{ return index(N); }

		template <class T>
		[[nodiscard]] constexpr T* iterator(T* collection, size_t size) noexcept{
			assert(collection != nullptr && "Random::iterator(): null collection.");
			return collection + index(size);
		}

		template <class T, size_t N>
		[[nodiscard]] constexpr T* iterator(T (&collection)[N]) noexcept{ return iterator(collection, N); }

		template <class T>
		[[nodiscard]] constexpr T& element(T* collection, size_t size) noexcept{
			return *iterator(collection, size);
		}

		template <class T, size_t N>
		[[nodiscard]] constexpr T& element(T (&collection)[N]) noexcept{ return *iterator(collection, N); }

		// --- weighted collections ---

		template <class W>
		[[nodiscard]] constexpr size_t weighted_index(const W* weights, size_t size) noexcept{
			if(weights == nullptr){
				assert(false && "Random::weighted_index(): null weight collection.");
				abort();
			}
			auto weight_at = [weights](size_t i) constexpr -> decltype(auto){ return weights[i]; };
			return weighted_offset(size, weight_at);
		}

		template <class W, size_t N>
		[[nodiscard]] constexpr size_t weighted_index(const W (&weights)[N]) noexcept{
			return weighted_index(weights, N);
		}

		template <class T, class Projection>
		[[nodiscard]] constexpr T* weighted_iterator(T* collection, size_t size, Projection projection) noexcept{
			if(collection == nullptr){
				assert(false && "Random::weighted_iterator(): null collection.");
				abort();
			}
			auto weight_at = [collection, &projection](size_t i) constexpr -> decltype(auto){
				return detail::avr_invoke(projection, collection[i], detail::avr_priority_2{});
			};
			return collection + weighted_offset(size, weight_at);
		}

		template <class T, size_t N, class Projection>
		[[nodiscard]] constexpr T* weighted_iterator(T (&collection)[N], Projection projection) noexcept{
			return weighted_iterator(collection, N, projection);
		}

		template <class T, class Projection>
		[[nodiscard]] constexpr T& weighted_element(T* collection, size_t size, Projection projection) noexcept{
			return *weighted_iterator(collection, size, projection);
		}

		template <class T, size_t N, class Projection>
		[[nodiscard]] constexpr T& weighted_element(T (&collection)[N], Projection projection) noexcept{
			return *weighted_iterator(collection, N, projection);
		}

	private:
		engine_type _engine{};

		template <class T>
		static constexpr unsigned bit_width() noexcept{ return sizeof(T) * CHAR_BIT; }

		template <class T>
		static constexpr T low_bits_mask(unsigned n) noexcept{
			return n >= bit_width<T>()
				? static_cast<T>(~T{0})
				: static_cast<T>((T{1} << n) - T{1});
		}

		template <class T>
		static constexpr T take_high_bits(result_type value, unsigned n) noexcept{
			assert(n > 0 && n <= value_bits && n <= bit_width<T>());
			return static_cast<T>(value >> (value_bits - n)) & low_bits_mask<T>(n);
		}

		template <class T>
		constexpr T gather_bits(unsigned n) noexcept{
			T result{};
			unsigned filled{};
			while(filled < n){
				const unsigned remaining = n - filled;
				const unsigned take = remaining < value_bits ? remaining : value_bits;
				result = static_cast<T>(result | static_cast<T>(take_high_bits<T>(next(), take) << filled));
				filled += take;
			}
			return static_cast<T>(result & low_bits_mask<T>(n));
		}

		template <class I>
		static constexpr uint64_t integral_max() noexcept{
			using value_type = typename detail::avr_remove_cv<I>::type;
			using U = detail::avr_unsigned_t<value_type>;
			if constexpr(value_type(-1) < value_type(0)){
				return uint64_t{static_cast<U>(~U{0}) >> 1};
			}
			return uint64_t{static_cast<U>(~U{0})};
		}

		static constexpr unsigned power_of_two_exponent(result_type bound) noexcept{
			unsigned exponent{};
			while(bound > 1){
				bound >>= 1; ++exponent;
			}
			return exponent;
		}

		template <class WeightAt>
		constexpr result_type total_weight(size_t size, WeightAt& weight_at) noexcept{
			using weight_type = detail::avr_remove_cvref_t<decltype(weight_at(size_t{}))>;
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

		template <class WeightAt>
		constexpr size_t weighted_offset(size_t size, WeightAt weight_at) noexcept{
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
				if(target < weight) return i;
				target -= weight;
			}
			assert(false && "Random::weighted_index(): weights changed during selection.");
			abort();
		}

		static constexpr result_type scale_to_bound(result_type value, result_type bound) noexcept{
			if constexpr(sizeof(result_type) == 1){
				return static_cast<result_type>((uint16_t{value} * uint16_t{bound}) >> 8);
			} else if constexpr(sizeof(result_type) == 2){
				return static_cast<result_type>((uint32_t{value} * uint32_t{bound}) >> 16);
			} else if constexpr(sizeof(result_type) == 4){
				return static_cast<result_type>((uint64_t{value} * uint64_t{bound}) >> 32);
			} else{
				return static_cast<result_type>(detail::mul64_to_128_parts(value, bound).hi);
			}
		}
	};

} // namespace rnd

#undef RND_DETAIL_AVR_FLOAT_CONSTEXPR
