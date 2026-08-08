#pragma once
#include "includes/detail/validation.hpp"
#include "includes/engines/konadare192.hpp"
#include "includes/engines/pcg32.hpp"
#include "includes/engines/quarkburst64.hpp"
#include "includes/engines/romuduojr.hpp"
#include "includes/engines/small_fast32.hpp"
#include "includes/engines/small_fast64.hpp"
#include "includes/engines/xoshiro256ss.hpp"
#include <array>
#include <bit>
#include <cstdint>

// Compile-time comparisons with the engines' reference implementations.
// Include this header in any translation unit that should perform validation.
namespace rnd::detail::validation {
	constexpr std::array pcg32_reference{
		std::uint32_t{0xa15c02b7}, std::uint32_t{0x7b47f409},
		std::uint32_t{0xba1d3330}, std::uint32_t{0x83d2f293},
		std::uint32_t{0xbfa4784b}, std::uint32_t{0xcbed606e}
	};

	// Values published by the original PCG32 implementation for seed 42, stream 54.
	// https://www.pcg-random.org/using-pcg-c-basic.html
	static_assert(prng_outputs(PCG32{42u, 54u}) == pcg32_reference);

	constexpr std::array quarkburst64_reference{
		0x0000060020000002ULL, 0x0403F68CF7217209ULL,
		0xE1C95D285697B7AFULL, 0x394DE3E1A9574CE0ULL,
		0x717616275935DAEFULL, 0x03745D2F175D0105ULL
	};

	static_assert(
		prng_outputs(QuarkBurst64::from_state(1, 2, 3)) == quarkburst64_reference,
		"QuarkBurst64 output does not match the archived reference implementation"
	);

	// Original RomuDuoJr transition from Mark Overton's 2020 reference code.
	// https://www.romu-random.org/code.c
	struct romu_state{
		std::uint64_t x;
		std::uint64_t y;
	};

	constexpr std::uint64_t romu_next(romu_state& state) noexcept{
		const std::uint64_t previous_x = state.x;
		state.x = 15241094284759029579ULL * state.y;
		state.y -= previous_x;
		state.y = (state.y << 27) | (state.y >> 37);
		return previous_x;
	}

	consteval auto romu_reference_outputs(){
		romu_state state{123, 0};
		std::array<std::uint64_t, 6> out{};
		for(auto& value : out) value = romu_next(state);
		return out;
	}

	static_assert(
		prng_outputs(RomuDuoJr::from_state(123, 0)) == romu_reference_outputs(),
		"RomuDuoJr output does not match the original reference implementation"
	);

	// Bob Jenkins' Small Fast reference implementation.
	// https://burtleburtle.net/bob/rand/smallprng.html
	struct jsf32_state{
		std::uint32_t a, b, c, d;
	};

	constexpr std::uint32_t jsf32_next(jsf32_state& state) noexcept{
		const std::uint32_t e = state.a - std::rotl(state.b, 27);
		state.a = state.b ^ std::rotl(state.c, 17);
		state.b = state.c + state.d;
		state.c = state.d + e;
		state.d = e + state.a;
		return state.d;
	}

	constexpr jsf32_state jsf32_init(std::uint32_t seed) noexcept{
		jsf32_state state{0xf1ea5eedu, seed, seed, seed};
		for(unsigned i = 0; i < 20; ++i) jsf32_next(state);
		return state;
	}

	consteval auto jsf32_reference_outputs(){
		auto state = jsf32_init(123);
		std::array<std::uint32_t, 6> out{};
		for(auto& value : out) value = jsf32_next(state);
		return out;
	}

	static_assert(
		prng_outputs(SmallFast32{123u}) == jsf32_reference_outputs(),
		"SmallFast32 output does not match Bob Jenkins' reference implementation"
	);

	struct jsf64_state{
		std::uint64_t a, b, c, d;
	};

	constexpr std::uint64_t jsf64_next(jsf64_state& state) noexcept{
		const std::uint64_t e = state.a - std::rotl(state.b, 7);
		state.a = state.b ^ std::rotl(state.c, 13);
		state.b = state.c + std::rotl(state.d, 37);
		state.c = state.d + e;
		state.d = e + state.a;
		return state.d;
	}

	constexpr jsf64_state jsf64_init(std::uint64_t seed) noexcept{
		jsf64_state state{0xf1ea5eedu, seed, seed, seed};
		for(unsigned i = 0; i < 20; ++i) jsf64_next(state);
		return state;
	}

	consteval auto jsf64_reference_outputs(){
		auto state = jsf64_init(123);
		std::array<std::uint64_t, 6> out{};
		for(auto& value : out) value = jsf64_next(state);
		return out;
	}

	static_assert(
		prng_outputs(SmallFast64{123}) == jsf64_reference_outputs(),
		"SmallFast64 output does not match Bob Jenkins' reference implementation"
	);

	// Original xoshiro256** 1.0 transition by David Blackman and Sebastiano Vigna.
	// https://prng.di.unimi.it/xoshiro256starstar.c
	constexpr std::uint64_t xoshiro_next(std::array<std::uint64_t, 4>& state) noexcept{
		const std::uint64_t result = std::rotl(state[1] * 5, 7) * 9;
		const std::uint64_t shifted = state[1] << 17;
		state[2] ^= state[0];
		state[3] ^= state[1];
		state[1] ^= state[2];
		state[0] ^= state[3];
		state[2] ^= shifted;
		state[3] = std::rotl(state[3], 45);
		return result;
	}

	constexpr std::array<std::uint64_t, 4> xoshiro_initial_state{
		0xFEEDFACECAFEBEEFULL, 0, 0, 0
	};

	consteval auto xoshiro_reference_outputs(){
		auto state = xoshiro_initial_state;
		std::array<std::uint64_t, 6> out{};
		for(auto& value : out) value = xoshiro_next(state);
		return out;
	}

	static_assert(
		prng_outputs(Xoshiro256SS::from_state(xoshiro_initial_state)) == xoshiro_reference_outputs(),
		"Xoshiro256SS output does not match the original reference implementation"
	);

	// Reference transcription of Pelle Evensen's konadare192px++ transition
	// and the seed expansion used by this port.
	struct konadare_state{
		std::uint64_t a, b, c;
	};

	constexpr std::uint64_t konadare_mix(std::uint64_t x, std::uint64_t c) noexcept{
		for(std::uint64_t i = 0; i < 5; ++i){
			x ^= std::rotr(x, 25) ^ std::rotr(x, 49);
			c += 0xBB67AE8584CAA73BULL + (c << 15) + (c << 7) + i;
			c ^= (c >> 47) ^ (c >> 23);
			x += c;
			x ^= (x >> 11) ^ (x >> 3);
		}
		return x;
	}

	constexpr konadare_state konadare_init(std::uint64_t seed) noexcept{
		konadare_state state{seed, seed + 1, seed + 2};
		for(int round = 0; round < 2; ++round){
			const std::uint64_t a = konadare_mix(state.a, state.c);
			const std::uint64_t b = konadare_mix(state.b, state.a);
			const std::uint64_t c = konadare_mix(state.c, state.b);
			state = {a, b, c};
		}
		if((state.a | state.b | state.c) == 0) state.a = 0x3C6EF372FE94F82BULL;
		return state;
	}

	constexpr std::uint64_t konadare_next(konadare_state& state) noexcept{
		const std::uint64_t output = state.b ^ state.c;
		const std::uint64_t a = state.a ^ (state.a >> 32);
		state.a += 0xBB67AE8584CAA73BULL;
		state.b = std::rotr(state.b + a, 11);
		state.c = std::rotl(state.c + state.b, 8);
		return output;
	}

	consteval auto konadare_reference_outputs(){
		auto state = konadare_init(123);
		std::array<std::uint64_t, 6> out{};
		for(auto& value : out) value = konadare_next(state);
		return out;
	}

	static_assert(
		prng_outputs(Konadare192{123}) == konadare_reference_outputs(),
		"Konadare192 output does not match its reference transcription"
	);
}
