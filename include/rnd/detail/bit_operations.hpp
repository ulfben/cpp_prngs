#pragma once
#include <limits.h>

#if __cplusplus >= 202002L || (defined(_MSVC_LANG) && _MSVC_LANG >= 202002L)
#include <bit>
#endif

namespace rnd::detail {
	template <class T>
	[[nodiscard]] constexpr T rotl(T value, unsigned shift) noexcept{
#if defined(__cpp_lib_bitops) && __cpp_lib_bitops >= 201907L
		return std::rotl(value, static_cast<int>(shift));
#else
		constexpr unsigned digits = sizeof(T) * CHAR_BIT;
		const unsigned amount = shift % digits;
		return amount == 0 ? value : static_cast<T>((value << amount) | (value >> (digits - amount)));
#endif
	}

	template <class T>
	[[nodiscard]] constexpr T rotr(T value, unsigned shift) noexcept{
#if defined(__cpp_lib_bitops) && __cpp_lib_bitops >= 201907L
		return std::rotr(value, static_cast<int>(shift));
#else
		constexpr unsigned digits = sizeof(T) * CHAR_BIT;
		const unsigned amount = shift % digits;
		return amount == 0 ? value : static_cast<T>((value >> amount) | (value << (digits - amount)));
#endif
	}
}
