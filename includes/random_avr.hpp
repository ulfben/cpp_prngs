#pragma once
#include "detail/portable_wide_multiply.hpp"
#include <assert.h>
#include <limits.h>
#include <stddef.h>
#include <stdint.h>

// A small C++17 Random<E> frontend for AVR-libc, which does not provide the
// standard C++ library facilities used by random.hpp. Include one frontend or
// the other; both intentionally expose rnd::Random<E>.

namespace rnd {

	template <class E>
	class Random final{
		using engine_result_type = typename E::result_type;
		static constexpr unsigned value_bits = sizeof(engine_result_type) * CHAR_BIT;

		static_assert(engine_result_type(-1) > engine_result_type(0), "Random<E> requires an unsigned engine result_type");
		static_assert(engine_result_type(2) != engine_result_type(1), "Random<E> does not support a boolean engine result_type");
		static_assert(sizeof(engine_result_type) == 1 || sizeof(engine_result_type) == 2 || sizeof(engine_result_type) == 4 || sizeof(engine_result_type) == 8, "Random<E> supports 8-, 16-, 32-, and 64-bit engine results");
		static_assert((E::min)() == engine_result_type{0}, "Random<E> requires an engine whose minimum is zero");
		static_assert((E::max)() == static_cast<engine_result_type>(~engine_result_type{0}), "Random<E> requires an engine spanning its complete result_type");

	public:
		using engine_type = E;
		using result_type = typename E::result_type;
		using seed_type = typename E::seed_type;

		constexpr Random() noexcept = default;
		explicit constexpr Random(seed_type seed_value) noexcept : _engine(seed_value){}
		explicit constexpr Random(engine_type engine) noexcept : _engine(engine){}

		constexpr bool operator==(const Random& rhs) const noexcept{
			return _engine == rhs._engine;
		}
		constexpr bool operator!=(const Random& rhs) const noexcept{
			return !(*this == rhs);
		}

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

		constexpr result_type next() noexcept{ return _engine(); }
		constexpr result_type operator()() noexcept{ return next(); }

		// Produce [0, bound) using multiply-high in the engine's native width.
		constexpr result_type next(result_type bound) noexcept{
			assert(bound != 0 && "Random::next(bound): bound must be positive.");
			return scale_to_bound(next(), bound);
		}
		constexpr result_type operator()(result_type bound) noexcept{ return next(bound); }

		// Return n random bits in the low bits of unsigned T.
		template <class T = result_type>
		constexpr T bits(unsigned n) noexcept{
			static_assert(T(-1) > T(0) && T(2) != T(1), "Random::bits<T>() requires a non-boolean unsigned T");
			assert(n > 0 && n <= bit_width<T>());
			return n <= value_bits ? take_high_bits<T>(next(), n) : gather_bits<T>(n);
		}

		template <unsigned N, class T = result_type>
		constexpr T bits() noexcept{
			static_assert(N > 0, "Random::bits<N>() needs at least one bit");
			static_assert(T(-1) > T(0) && T(2) != T(1), "Random::bits<N, T>() requires a non-boolean unsigned T");
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

		// Native-width integer in [lo, hi).
		constexpr result_type between(result_type lo, result_type hi) noexcept{
			assert(lo < hi && "Random::between(lo, hi): inverted or empty range.");
			return static_cast<result_type>(lo + next(static_cast<result_type>(hi - lo)));
		}

		constexpr bool coin_flip() noexcept{
			return (next() >> (value_bits - 1)) != 0;
		}

		[[nodiscard]] constexpr size_t index(size_t size) noexcept{
			assert(size != 0 && "Random::index(): empty collection.");
			assert(size <= static_cast<size_t>((max)()) && "Random::index(): collection is too large for this engine.");
			return static_cast<size_t>(next(static_cast<result_type>(size)));
		}

		template <class T, size_t N>
		[[nodiscard]] constexpr size_t index(const T (&)[N]) noexcept{
			return index(N);
		}

		template <class T, size_t N>
		[[nodiscard]] constexpr T& element(T (&collection)[N]) noexcept{
			return collection[index(N)];
		}

	private:
		engine_type _engine{};

		template <class T>
		static constexpr unsigned bit_width() noexcept{
			return sizeof(T) * CHAR_BIT;
		}

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
