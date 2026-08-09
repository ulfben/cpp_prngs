#pragma once
#include <limits>
#include <type_traits>

#if __cplusplus >= 202002L || (defined(_MSVC_LANG) && _MSVC_LANG >= 202002L)
#include <bit>
#endif

namespace rnd::detail {
	template <class T>
	[[nodiscard]] constexpr T rotl(T value, unsigned shift) noexcept{
		static_assert(std::is_unsigned_v<T>, "rotl requires an unsigned integer type");
#if defined(__cpp_lib_bitops) && __cpp_lib_bitops >= 201907L
		return std::rotl(value, static_cast<int>(shift));
#else
		constexpr unsigned digits = std::numeric_limits<T>::digits;
		const unsigned amount = shift % digits;
		return amount == 0 ? value : static_cast<T>((value << amount) | (value >> (digits - amount)));
#endif
	}

	template <class T>
	[[nodiscard]] constexpr T rotr(T value, unsigned shift) noexcept{
		static_assert(std::is_unsigned_v<T>, "rotr requires an unsigned integer type");
#if defined(__cpp_lib_bitops) && __cpp_lib_bitops >= 201907L
		return std::rotr(value, static_cast<int>(shift));
#else
		constexpr unsigned digits = std::numeric_limits<T>::digits;
		const unsigned amount = shift % digits;
		return amount == 0 ? value : static_cast<T>((value >> amount) | (value << (digits - amount)));
#endif
	}
}
