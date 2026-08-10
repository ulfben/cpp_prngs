#pragma once
#include <float.h>
#include <stddef.h>
#include <stdint.h> // AVR-libc reliably provides <*.h>, but not the C++ <c*> wrappers.

// The AVR-libc toolchain does not provide the standard-library headers
// <type_traits> or <functional>. This header implements only the small subset
// needed by random_avr.hpp.
//
// With a complete C++20 standard library, the supported-type predicates below
// could be written as concepts using std::same_as and std::remove_cvref_t, and
// overloads could be constrained directly with requires-clauses. The unsigned
// counterpart of an integer could use std::make_unsigned_t, while projections
// could be called through std::invoke.

namespace rnd::detail {

// -----------------------------------------------------------------------------
// Type normalization
// -----------------------------------------------------------------------------
//
// Types reaching these helpers may carry references or const/volatile
// qualifiers that do not affect whether the underlying value type is supported.
// For example, accessing a member of a const object may give us const int&.
//
// These templates strip references and top-level const/volatile qualifiers so
// we can classify the underlying value type.

	template <class T, class U>
	struct avr_is_same{
		static constexpr bool value = false;
	};

	template <class T>
	struct avr_is_same<T, T>{
		static constexpr bool value = true;
	};

	template <class T>
	struct avr_remove_cv{
		using type = T;
	};

	template <class T>
	struct avr_remove_cv<const T>{
		using type = T;
	};

	template <class T>
	struct avr_remove_cv<volatile T>{
		using type = T;
	};

	template <class T>
	struct avr_remove_cv<const volatile T>{
		using type = T;
	};

	template <class T>
	struct avr_remove_reference{
		using type = T;
	};

	template <class T>
	struct avr_remove_reference<T&>{
		using type = T;
	};

	template <class T>
	struct avr_remove_reference<T&&>{
		using type = T;
	};

	template <class T>
	using avr_remove_cvref_t =
		typename avr_remove_cv<typename avr_remove_reference<T>::type>::type;


	// -----------------------------------------------------------------------------
	// Supported integer types
	// -----------------------------------------------------------------------------
	//
	// Next come some poor man's "concepts": predicates describing exactly which
	// fixed-width integer types this frontend supports.
	//
	// Unlike std::integral, these intentionally accept only the fixed-width integer
	// types used by random_avr.hpp. Types such as bool, char, wchar_t and char16_t
	// are not part of this API's integer contract.

	template <class T>
	static constexpr bool avr_supported_uint =
		avr_is_same<avr_remove_cvref_t<T>, uint8_t>::value ||
		avr_is_same<avr_remove_cvref_t<T>, uint16_t>::value ||
		avr_is_same<avr_remove_cvref_t<T>, uint32_t>::value ||
		avr_is_same<avr_remove_cvref_t<T>, uint64_t>::value;

	template <class T>
	static constexpr bool avr_supported_integer =
		avr_supported_uint<T> ||
		avr_is_same<avr_remove_cvref_t<T>, int8_t>::value ||
		avr_is_same<avr_remove_cvref_t<T>, int16_t>::value ||
		avr_is_same<avr_remove_cvref_t<T>, int32_t>::value ||
		avr_is_same<avr_remove_cvref_t<T>, int64_t>::value;


	// -----------------------------------------------------------------------------
	// C++17 constraint helper
	// -----------------------------------------------------------------------------
	//
	// C++20 concepts let us write constrained overloads directly. In C++17 we use
	// this tiny equivalent of std::enable_if_t instead.
	//
	// This is particularly useful when two function templates have the same shape,
	// such as the integer and floating-point overloads of between(). The condition
	// removes the inappropriate overload from consideration during substitution.

	template <bool Condition, class T = void>
	struct avr_enable_if{};

	template <class T>
	struct avr_enable_if<true, T>{
		using type = T;
	};

	template <bool Condition, class T = void>
	using avr_enable_if_t = typename avr_enable_if<Condition, T>::type;


	// -----------------------------------------------------------------------------
	// Unsigned counterpart by width
	// -----------------------------------------------------------------------------
	//
	// A hand-crafted alternative to std::make_unsigned_t.
	//
	// These templates answer:
	// "What unsigned fixed-width type has the same width as T?"
	//
	// For example:
	//
	//     avr_unsigned_t<int8_t>  => uint8_t
	//     avr_unsigned_t<int16_t> => uint16_t
	//     avr_unsigned_t<int32_t> => uint32_t
	//     avr_unsigned_t<int64_t> => uint64_t
	//
	// random_avr.hpp needs this in between(lo, hi), next<Bound, T>() and
	// integral_max<T>().

	template <size_t Size>
	struct avr_uint_of_size;

	template <>
	struct avr_uint_of_size<1>{
		using type = uint8_t;
	};

	template <>
	struct avr_uint_of_size<2>{
		using type = uint16_t;
	};

	template <>
	struct avr_uint_of_size<4>{
		using type = uint32_t;
	};

	template <>
	struct avr_uint_of_size<8>{
		using type = uint64_t;
	};

	template <class T>
	using avr_unsigned_t =
		typename avr_uint_of_size<sizeof(avr_remove_cvref_t<T>)>::type;


	// -----------------------------------------------------------------------------
	// Supported floating-point types
	// -----------------------------------------------------------------------------
	//
	// random_avr.hpp supports float and double, but some operations also require
	// the type to use the IEEE-754 binary32 representation expected by the
	// implementation.

	template <class F>
	static constexpr bool avr_supported_float =
		avr_is_same<avr_remove_cvref_t<F>, float>::value ||
		avr_is_same<avr_remove_cvref_t<F>, double>::value;

	template <class F>
	struct avr_is_binary32_base{
		static constexpr bool value = false;
	};

	template <>
	struct avr_is_binary32_base<float>{
		static constexpr bool value =
			sizeof(float) == 4 &&
			FLT_RADIX == 2 &&
			FLT_MANT_DIG == 24 &&
			FLT_MAX_EXP == 128;
	};

	template <>
	struct avr_is_binary32_base<double>{
		static constexpr bool value =
			sizeof(double) == 4 &&
			FLT_RADIX == 2 &&
			DBL_MANT_DIG == 24 &&
			DBL_MAX_EXP == 128;
	};

	template <class F>
	static constexpr bool avr_is_binary32 =
		avr_is_binary32_base<avr_remove_cvref_t<F>>::value;


	// -----------------------------------------------------------------------------
	// Projection invocation
	// -----------------------------------------------------------------------------
	//
	// A small replacement for the subset of std::invoke needed by random_avr.hpp.
	//
	// A projection may be an ordinary callable, a pointer to a member function,
	// or a pointer to a data member; each form requires different call syntax.
	//
	// For example:
	//
	//     rng.weighted_element(
	//         items,
	//         count,
	//         [](const Item& x) { return x.weight; }
	//     );                                  // callable / lambda
	//
	//     rng.weighted_element(
	//         items,
	//         count,
	//         &Item::get_weight
	//     );                                  // member function
	//
	//     rng.weighted_element(
	//         items,
	//         count,
	//         &Item::weight
	//     );                                  // data member
	//
	// These correspond to:
	//
	//     projection(value)        // callable / lambda
	//     (value.*projection)()    // member-function pointer
	//     value.*projection        // data-member pointer
	//
	// std::invoke normally unifies these different syntaxes:
	//
	//     std::invoke(projection, value);
	//
	// Since <functional> is unavailable, we handle the three forms ourselves.
	//
	// This is deliberately not a complete std::invoke implementation.
	// random_avr.hpp only needs direct objects/references, so we support the three
	// invocation forms used by its projection API.

	struct avr_priority_0{};

	struct avr_priority_1 : avr_priority_0{};

	struct avr_priority_2 : avr_priority_1{};

	template <class Projection, class T>
	constexpr auto avr_invoke(
		Projection& projection,
		T& value,
		avr_priority_2
	) noexcept -> decltype(projection(value)){
		return projection(value);
	}

	template <class Projection, class T>
	constexpr auto avr_invoke(
		Projection& projection,
		T& value,
		avr_priority_1
	) noexcept -> decltype((value.*projection)()){
		return (value.*projection)();
	}

	template <class Projection, class T>
	constexpr auto avr_invoke(
		Projection& projection,
		T& value,
		avr_priority_0
	) noexcept -> decltype(value.*projection){
		return value.*projection;
	}

} // namespace rnd::detail
