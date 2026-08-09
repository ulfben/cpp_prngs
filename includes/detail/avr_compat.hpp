#pragma once
#include <float.h>
#include <stddef.h>
#include <stdint.h>

// The AVR toolchain has no <type_traits> or <functional>. This header provides
// only the "small" subset random_avr.hpp needs for constraints and projections.

namespace rnd::detail {

	template <class T, class U> struct avr_is_same{ static constexpr bool value = false; };
	template <class T> struct avr_is_same<T, T>{ static constexpr bool value = true; };

	template <class T> struct avr_remove_cv{ using type = T; };
	template <class T> struct avr_remove_cv<const T>{ using type = T; };
	template <class T> struct avr_remove_cv<volatile T>{ using type = T; };
	template <class T> struct avr_remove_cv<const volatile T>{ using type = T; };
	template <class T> struct avr_remove_reference{ using type = T; };
	template <class T> struct avr_remove_reference<T&>{ using type = T; };
	template <class T> struct avr_remove_reference<T&&>{ using type = T; };

	template <class T>
	using avr_remove_cvref_t = typename avr_remove_cv<typename avr_remove_reference<T>::type>::type;

	template <class T> struct avr_is_integral_base{ static constexpr bool value = false; };
	#define RND_DETAIL_AVR_INTEGRAL(T) template <> struct avr_is_integral_base<T>{ static constexpr bool value = true; }
	RND_DETAIL_AVR_INTEGRAL(bool);
	RND_DETAIL_AVR_INTEGRAL(char);
	RND_DETAIL_AVR_INTEGRAL(signed char);
	RND_DETAIL_AVR_INTEGRAL(unsigned char);
	RND_DETAIL_AVR_INTEGRAL(wchar_t);
	RND_DETAIL_AVR_INTEGRAL(char16_t);
	RND_DETAIL_AVR_INTEGRAL(char32_t);
	RND_DETAIL_AVR_INTEGRAL(short);
	RND_DETAIL_AVR_INTEGRAL(unsigned short);
	RND_DETAIL_AVR_INTEGRAL(int);
	RND_DETAIL_AVR_INTEGRAL(unsigned int);
	RND_DETAIL_AVR_INTEGRAL(long);
	RND_DETAIL_AVR_INTEGRAL(unsigned long);
	RND_DETAIL_AVR_INTEGRAL(long long);
	RND_DETAIL_AVR_INTEGRAL(unsigned long long);
	#undef RND_DETAIL_AVR_INTEGRAL

	template <class T>
	static constexpr bool avr_is_integral = avr_is_integral_base<typename avr_remove_cv<T>::type>::value;

	template <class T, bool = avr_is_integral<avr_remove_cvref_t<T>>>
	struct avr_is_unsigned_integer{ static constexpr bool value = false; };

	template <class T>
	struct avr_is_unsigned_integer<T, true>{
		using value_type = avr_remove_cvref_t<T>;
		static constexpr bool value =
			!avr_is_same<value_type, bool>::value && value_type(-1) > value_type(0);
	};

	template <bool Condition, class T = void> struct avr_enable_if{};
	template <class T> struct avr_enable_if<true, T>{ using type = T; };
	template <bool Condition, class T = void>
	using avr_enable_if_t = typename avr_enable_if<Condition, T>::type;

	template <size_t Size> struct avr_uint_of_size;
	template <> struct avr_uint_of_size<1>{ using type = uint8_t; };
	template <> struct avr_uint_of_size<2>{ using type = uint16_t; };
	template <> struct avr_uint_of_size<4>{ using type = uint32_t; };
	template <> struct avr_uint_of_size<8>{ using type = uint64_t; };
	template <class T> using avr_unsigned_t = typename avr_uint_of_size<sizeof(T)>::type;

	template <class F> struct avr_is_float_type_base{ static constexpr bool value = false; };
	template <> struct avr_is_float_type_base<float>{ static constexpr bool value = true; };
	template <> struct avr_is_float_type_base<double>{ static constexpr bool value = true; };
	template <class F>
	static constexpr bool avr_is_float_type = avr_is_float_type_base<typename avr_remove_cv<F>::type>::value;

	template <class F> struct avr_is_binary32_base{ static constexpr bool value = false; };
	template <> struct avr_is_binary32_base<float>{
		static constexpr bool value = sizeof(float) == 4 && FLT_RADIX == 2 && FLT_MANT_DIG == 24 && FLT_MAX_EXP == 128;
	};
	template <> struct avr_is_binary32_base<double>{
		static constexpr bool value = sizeof(double) == 4 && FLT_RADIX == 2 && DBL_MANT_DIG == 24 && DBL_MAX_EXP == 128;
	};
	template <class F>
	static constexpr bool avr_is_binary32 = avr_is_binary32_base<typename avr_remove_cv<F>::type>::value;

	struct avr_priority_0{};
	struct avr_priority_1 : avr_priority_0{};
	struct avr_priority_2 : avr_priority_1{};

	template <class Projection, class T>
	constexpr auto avr_invoke(Projection& projection, T& value, avr_priority_2) noexcept
		-> decltype(projection(value)){
		return projection(value);
	}

	template <class Projection, class T>
	constexpr auto avr_invoke(Projection& projection, T& value, avr_priority_1) noexcept
		-> decltype((value.*projection)()){
		return (value.*projection)();
	}

	template <class Projection, class T>
	constexpr auto avr_invoke(Projection& projection, T& value, avr_priority_0) noexcept
		-> decltype(value.*projection){
		return value.*projection;
	}

} // namespace rnd::detail
