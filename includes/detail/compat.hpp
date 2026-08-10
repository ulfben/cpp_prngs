#pragma once

#include <float.h>
#include <stddef.h>
#include <stdint.h> // AVR-libc reliably provides the C headers, but not every C++ wrapper.

// This is the small portability seam behind random.hpp. On a normal desktop it
// mostly gives our implementation short, uniform names for standard facilities.
// On AVR-libc, where headers such as <type_traits> and <functional> are missing,
// it supplies only the handful of operations Random actually needs.
//
// Keeping these details here is what lets the interesting RNG code live in one
// header. It also gives students a compact example of a useful compatibility
// layer: feature detection at the edge, ordinary portable code in the middle.

// First ask the toolchain what it has. We deliberately detect the facilities,
// rather than the AVR platform name, so another constrained C++17 library can
// use the same fallback path without pretending to be AVR.
#if defined(__has_include)
	#if __has_include(<functional>) && __has_include(<iterator>) && __has_include(<type_traits>) && __has_include(<utility>)
		#define RND_DETAIL_HAS_STANDARD_COMPAT 1
	#endif
#endif

#ifndef RND_DETAIL_HAS_STANDARD_COMPAT
	#define RND_DETAIL_HAS_STANDARD_COMPAT 0
#endif

#if RND_DETAIL_HAS_STANDARD_COMPAT
	#include <functional>
	#include <iterator>
	#include <type_traits>
	#include <utility>
	#if defined(__cpp_consteval) && defined(__has_include)
		#if __has_include(<bit>)
			#include <bit>
		#endif
	#endif
#endif

// std::bit_cast is a separate feature because a complete C++17 library can have
// all of the helpers above without the C++20 constexpr bit-cast operation.
#if defined(__cpp_lib_bit_cast) && __cpp_lib_bit_cast >= 201806L
	#define RND_DETAIL_HAS_CONSTEXPR_BIT_CAST 1
#else
	#define RND_DETAIL_HAS_CONSTEXPR_BIT_CAST 0
#endif

// RND_FAST_FLOAT selects a runtime memcpy-based bit cast when constexpr
// std::bit_cast is unavailable.
#if defined(RND_FAST_FLOAT) && !RND_DETAIL_HAS_CONSTEXPR_BIT_CAST
	#include <string.h>
	#define RND_DETAIL_FLOAT_CONSTEXPR
#else
	#define RND_DETAIL_FLOAT_CONSTEXPR constexpr
#endif

namespace rnd::detail {

// -----------------------------------------------------------------------------
// Type normalization and C++17 substitution helpers
// -----------------------------------------------------------------------------
//
// Types reaching the traits below may carry references or const/volatile
// qualifiers that do not change what the underlying value actually is. For
// example, reading a member from a const object may produce const uint8_t&.
// remove_cvref_t lets the rest of the file classify uint8_t in both cases.
//
// On a full standard library these are intentionally boring aliases. That is a
// nice property: desktop builds pay no price for the fallback implementation.

#if RND_DETAIL_HAS_STANDARD_COMPAT

	template <class T>
	using remove_cv_t = typename std::remove_cv<T>::type;

	template <class T>
	using remove_cvref_t = typename std::remove_cv<typename std::remove_reference<T>::type>::type;

	template <bool Condition, class T = void>
	using enable_if_t = typename std::enable_if<Condition, T>::type;

	template <class... T>
	using void_t = std::void_t<T...>;

	template <class T, class U>
	static constexpr bool is_same = std::is_same<T, U>::value;

	// Like std::declval, this is declared but never defined or called. It exists
	// only so unevaluated decltype expressions can ask “would this compile?”.
	template <class T>
	T&& declval() noexcept;

#else

	template <class T, class U>
	struct is_same_impl{
		static constexpr bool value = false;
	};

	template <class T>
	struct is_same_impl<T, T>{
		static constexpr bool value = true;
	};

	template <class T, class U>
	static constexpr bool is_same = is_same_impl<T, U>::value;

	template <class T>
	struct remove_cv{
		using type = T;
	};

	template <class T>
	struct remove_cv<const T>{
		using type = T;
	};

	template <class T>
	struct remove_cv<volatile T>{
		using type = T;
	};

	template <class T>
	struct remove_cv<const volatile T>{
		using type = T;
	};

	template <class T>
	using remove_cv_t = typename remove_cv<T>::type;

	template <class T>
	struct remove_reference{
		using type = T;
	};

	template <class T>
	struct remove_reference<T&>{
		using type = T;
	};

	template <class T>
	struct remove_reference<T&&>{
		using type = T;
	};

	template <class T>
	using remove_cvref_t = remove_cv_t<typename remove_reference<T>::type>;

	template <bool Condition, class T = void>
	struct enable_if{};

	template <class T>
	struct enable_if<true, T>{
		using type = T;
	};

	template <bool Condition, class T = void>
	using enable_if_t = typename enable_if<Condition, T>::type;

	template <class...>
	using void_t = void;

	template <class T>
	T&& declval() noexcept;

#endif

	// -----------------------------------------------------------------------------
	// Supported integer types
	// -----------------------------------------------------------------------------
	//
	// These are our C++17-era “poor person's concepts”. The library intentionally
	// accepts the familiar fixed-width integers and rejects bool, plain char,
	// wchar_t and character types whose numeric intent would be ambiguous.
	//
	// Having one explicit contract everywhere is important: it prevents the
	// desktop implementation from quietly accepting a type that the same program
	// cannot use when it is moved to an embedded target.

	template <class T>
	static constexpr bool supported_uint =
		is_same<remove_cvref_t<T>, uint8_t> ||
		is_same<remove_cvref_t<T>, uint16_t> ||
		is_same<remove_cvref_t<T>, uint32_t> ||
		is_same<remove_cvref_t<T>, uint64_t>;

	template <class T>
	static constexpr bool supported_integer =
		supported_uint<T> ||
		is_same<remove_cvref_t<T>, int8_t> ||
		is_same<remove_cvref_t<T>, int16_t> ||
		is_same<remove_cvref_t<T>, int32_t> ||
		is_same<remove_cvref_t<T>, int64_t>;

	// -----------------------------------------------------------------------------
	// Unsigned counterpart by width
	// -----------------------------------------------------------------------------
	//
	// This is the small subset of std::make_unsigned_t that Random needs. It asks:
	// “which unsigned fixed-width type occupies the same number of bytes as T?”
	//
	//     unsigned_t<int8_t>  -> uint8_t
	//     unsigned_t<int16_t> -> uint16_t
	//     unsigned_t<int32_t> -> uint32_t
	//     unsigned_t<int64_t> -> uint64_t
	//
	// between(lo, hi), next<Bound, T>() and integral_max<T>() all use this to do
	// their arithmetic without signed overflow.

	template <size_t Size>
	struct uint_of_size;

	template <>
	struct uint_of_size<1>{ using type = uint8_t; };

	template <>
	struct uint_of_size<2>{ using type = uint16_t; };

	template <>
	struct uint_of_size<4>{ using type = uint32_t; };

	template <>
	struct uint_of_size<8>{ using type = uint64_t; };

	template <class T>
	using unsigned_t = typename uint_of_size<sizeof(remove_cvref_t<T>)>::type;

	// -----------------------------------------------------------------------------
	// Floating-point representation
	// -----------------------------------------------------------------------------
	//
	// The normalized() trick needs to know two things about F: how many random
	// mantissa bits it can hold, and which bit pattern represents 1.0 when a C++17
	// runtime bit cast is requested. Desktop double is normally IEEE binary64;
	// classic AVR double is binary32. Both are genuine, supported configurations.
	//
	// The primary template deliberately rejects everything else, including long
	// double. Supporting a new representation should be an explicit decision,
	// rather than an accidental consequence of sizeof happening to match.

	template <class F>
	struct float_traits{
		static constexpr bool supported = false;
		static constexpr unsigned mantissa_bits = 0;
	};

	template <>
	struct float_traits<float>{
		static constexpr bool supported =
			sizeof(float) == 4 && FLT_RADIX == 2 &&
			FLT_MANT_DIG == 24 && FLT_MAX_EXP == 128;
		static constexpr unsigned mantissa_bits = FLT_MANT_DIG - 1;
	};

	template <>
	struct float_traits<double>{
		static constexpr bool supported =
			FLT_RADIX == 2 &&
			((sizeof(double) == 4 && DBL_MANT_DIG == 24 && DBL_MAX_EXP == 128) ||
			 (sizeof(double) == 8 && DBL_MANT_DIG == 53 && DBL_MAX_EXP == 1024));
		static constexpr unsigned mantissa_bits = DBL_MANT_DIG - 1;
	};

	template <class F>
	static constexpr bool supported_float = float_traits<remove_cvref_t<F>>::supported;

	template <class F>
	constexpr unsigned_t<F> floating_one_bits() noexcept{
		if constexpr(sizeof(remove_cvref_t<F>) == 4){
			return UINT32_C(0x3f800000);
		}else{
			return UINT64_C(0x3ff0000000000000);
		}
	}

#if RND_DETAIL_HAS_STANDARD_COMPAT

	// -----------------------------------------------------------------------------
	// Standard-library forwarding path
	// -----------------------------------------------------------------------------
	//
	// std::data/std::size/std::begin handle both containers and built-in arrays.
	// std::invoke gives projections one spelling whether they are lambdas, member
	// functions, or data members. Wrapping them here keeps random.hpp identical on
	// both paths and makes the feature-dependent code very easy to spot.

	template <class C>
	constexpr auto collection_data(C& collection) noexcept(noexcept(std::data(collection)))
		-> decltype(std::data(collection)){
		return std::data(collection);
	}

	template <class C>
	constexpr auto collection_size(const C& collection) noexcept(noexcept(std::size(collection)))
		-> decltype(std::size(collection)){
		return std::size(collection);
	}

	template <class C>
	constexpr auto collection_begin(C& collection) noexcept(noexcept(std::begin(collection)))
		-> decltype(std::begin(collection)){
		return std::begin(collection);
	}

	template <class Projection, class T>
	constexpr decltype(auto) invoke(Projection& projection, T& value)
		noexcept(noexcept(std::invoke(projection, value))){
		return std::invoke(projection, value);
	}

	#if RND_DETAIL_HAS_CONSTEXPR_BIT_CAST
		template <class To, class From>
		constexpr To bit_cast(const From& value) noexcept{
			return std::bit_cast<To>(value);
		}
	#endif

#else

	// -----------------------------------------------------------------------------
	// Constrained-library collection access
	// -----------------------------------------------------------------------------
	//
	// These overloads provide precisely the subset of std::data, std::size and
	// std::begin needed by Random. Containers use their member functions; built-in
	// arrays use the overloads whose size N is known by the compiler.

	template <class C>
	constexpr auto collection_data(C& collection) noexcept -> decltype(collection.data()){
		return collection.data();
	}

	template <class T, size_t N>
	constexpr T* collection_data(T (&collection)[N]) noexcept{
		return collection;
	}

	template <class C>
	constexpr auto collection_size(const C& collection) noexcept -> decltype(collection.size()){
		return collection.size();
	}

	template <class T, size_t N>
	constexpr size_t collection_size(const T (&)[N]) noexcept{
		return N;
	}

	template <class C>
	constexpr auto collection_begin(C& collection) noexcept -> decltype(collection.begin()){
		return collection.begin();
	}

	template <class T, size_t N>
	constexpr T* collection_begin(T (&collection)[N]) noexcept{
		return collection;
	}

	// -----------------------------------------------------------------------------
	// Projection invocation without <functional>
	// -----------------------------------------------------------------------------
	//
	// A projection accepted by weighted_element() can take three useful forms:
	//
	//     projection(value)        // callable object or lambda
	//     (value.*projection)()    // pointer to a member function
	//     value.*projection        // pointer to a data member
	//
	// std::invoke normally hides those syntax differences. AVR-libc does not give
	// us <functional>, so the overload set below implements just these three forms.
	// It is deliberately not a complete replacement for std::invoke; direct objects
	// and references are all this API needs.
	//
	// The priority tags are a small SFINAE trick. We try the ordinary call first.
	// If that expression is ill-formed, overload resolution falls back to the
	// member-function form, and finally to the data-member form.

	struct priority_0{};
	struct priority_1 : priority_0{};
	struct priority_2 : priority_1{};

	template <class Projection, class T>
	constexpr auto invoke_impl(Projection& projection, T& value, priority_2)
		noexcept(noexcept(projection(value))) -> decltype(projection(value)){
		return projection(value);
	}

	template <class Projection, class T>
	constexpr auto invoke_impl(Projection& projection, T& value, priority_1)
		noexcept(noexcept((value.*projection)())) -> decltype((value.*projection)()){
		return (value.*projection)();
	}

	template <class Projection, class T>
	constexpr auto invoke_impl(Projection& projection, T& value, priority_0)
		noexcept(noexcept(value.*projection)) -> decltype(value.*projection){
		return value.*projection;
	}

	template <class Projection, class T>
	constexpr auto invoke(Projection& projection, T& value)
		noexcept(noexcept(invoke_impl(projection, value, priority_2{})))
		-> decltype(invoke_impl(projection, value, priority_2{})){
		return invoke_impl(projection, value, priority_2{});
	}

#endif

	// -----------------------------------------------------------------------------
	// C++17 API constraints
	// -----------------------------------------------------------------------------
	//
	// Concepts made the former desktop header's constraints pleasantly readable.
	// We still want the same behavior in a C++17 implementation: a list should not
	// masquerade as contiguous storage, a signed weight should be rejected, and a
	// throwing projection should not enter a noexcept API.
	//
	// Each primary template says “no”. A partial specialization is selected only
	// when the expressions inside void_t are well-formed, at which point we inspect
	// their types and noexcept properties. This is the classic detection idiom—the
	// slightly wordier C++17 ancestor of a requires-expression.

	template <class C, class = void>
	struct contiguous_collection{
		static constexpr bool value = false;
	};

	template <class C>
	struct contiguous_collection<C, void_t<
		decltype(collection_data(declval<C&>())),
		decltype(collection_size(declval<const C&>())),
		decltype(collection_begin(declval<C&>()))>>{
		static constexpr bool value = true;
	};

	template <class T, class Projection, class Result, class = void>
	struct valid_projection{
		static constexpr bool value = false;
	};

	template <class T, class Projection, class Result>
	struct valid_projection<T, Projection, Result, void_t<
		decltype(invoke(declval<Projection&>(), declval<T&>()))>>{
		using weight_type = remove_cvref_t<decltype(invoke(
			declval<Projection&>(), declval<T&>()))>;
		static constexpr bool value =
			noexcept(invoke(declval<Projection&>(), declval<T&>())) &&
			supported_uint<weight_type> &&
			(sizeof(weight_type) <= sizeof(Result));
	};

	template <class C, class Result, class = void>
	struct valid_weight_collection{
		static constexpr bool value = false;
	};

	template <class C, class Result>
	struct valid_weight_collection<C, Result, void_t<
		decltype(*collection_data(declval<const C&>()))>>{
		using weight_type = remove_cvref_t<decltype(*collection_data(declval<const C&>()))>;
		static constexpr bool value =
			contiguous_collection<const C>::value &&
			supported_uint<weight_type> &&
			(sizeof(weight_type) <= sizeof(Result));
	};

	template <class C, class Projection, class Result, class = void>
	struct valid_projected_collection{
		static constexpr bool value = false;
	};

	template <class C, class Projection, class Result>
	struct valid_projected_collection<C, Projection, Result, void_t<
		decltype(*collection_data(declval<C&>()))>>{
		using element_type = decltype(*collection_data(declval<C&>()));
		static constexpr bool value =
			contiguous_collection<C>::value &&
			valid_projection<element_type, Projection, Result>::value;
	};

} // namespace rnd::detail
