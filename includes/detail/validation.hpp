#pragma once
#include "wide_multiply.hpp"
#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>

// Compile-time validation helpers. Production headers do not include this file.
namespace rnd::detail::validation {
	template <typename Engine, std::size_t N = 6>
	consteval std::array<typename Engine::result_type, N> prng_outputs(Engine rng){
		std::array<typename Engine::result_type, N> out{};
		for(auto& value : out) value = rng();
		return out;
	}

	constexpr std::uint64_t HI = 0x0123'4567'89AB'CDEFull;
	constexpr std::uint64_t LO = 0xFEDC'BA98'7654'3210ull;
	constexpr std::uint64_t MAX = std::numeric_limits<std::uint64_t>::max();

	consteval bool check_mul(std::uint64_t a, std::uint64_t b, std::uint64_t expected_lo, std::uint64_t expected_hi){
		const auto product = mul64_to_128_parts(a, b);
		return product.lo == expected_lo && product.hi == expected_hi;
	}

	static_assert(shr128_to_u64<64>(HI, LO) == HI);
	static_assert(shr128_to_u64<1>(HI, LO) == ((LO >> 1) | (HI << 63)));
	static_assert(shr128_to_u64<63>(HI, LO) == ((LO >> 63) | (HI << 1)));

	static_assert(check_mul(0, 0, 0, 0));
	static_assert(check_mul(MAX, 1, MAX, 0));
	static_assert(check_mul(1ULL << 32, 1ULL << 32, 0, 1));
	static_assert(check_mul(MAX, MAX, 1, 0xFFFF'FFFF'FFFF'FFFEull));
	static_assert(check_mul(MAX, 1ULL << 32, 0xFFFF'FFFF'0000'0000ull, 0x0000'0000'FFFF'FFFFull));
	static_assert(check_mul(0x0000'0001'FFFF'FFFFull, 0x0000'0001'FFFF'FFFFull, 0xFFFF'FFFC'0000'0001ull, 3));

	template <std::uint64_t> struct require_constexpr{};
	using test_shift_1 = require_constexpr<mul_shift_u64<1>(HI, LO)>;
	using test_shift_64 = require_constexpr<mul_shift_u64<64>(HI, LO)>;
}
