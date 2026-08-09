#pragma once
#include "../includes/engines/konadare192.hpp"
#include "../includes/engines/pcg32.hpp"
#include "../includes/engines/quarkburst64.hpp"
#include "../includes/engines/romuduojr.hpp"
#include "../includes/engines/small_fast32.hpp"
#include "../includes/engines/small_fast64.hpp"
#include "../includes/engines/xoshiro256ss.hpp"
#include <stddef.h>
#include <stdint.h>

// Compile-time comparisons with the engines' reference implementations.
// Include this header in any translation unit that should perform validation.
namespace rnd::detail::validation {
	template <class T, size_t N>
	struct fixed_array{
		T values[N]{};

		constexpr T& operator[](size_t index) noexcept{ return values[index]; }
		constexpr const T& operator[](size_t index) const noexcept{ return values[index]; }
	};

	template <typename Engine, size_t N = 6>
	constexpr fixed_array<typename Engine::result_type, N> prng_outputs(Engine rng){
		fixed_array<typename Engine::result_type, N> out{};
		for(size_t i = 0; i < N; ++i) out[i] = rng();
		return out;
	}

	template <class T, class U, size_t N>
	constexpr bool arrays_equal(const fixed_array<T, N>& lhs, const fixed_array<U, N>& rhs){
		for(size_t i = 0; i < N; ++i){
			if(lhs[i] != rhs[i]) return false;
		}
		return true;
	}

	constexpr fixed_array<uint32_t, 6> pcg32_reference{{
		0xa15c02b7, 0x7b47f409, 0xba1d3330,
		0x83d2f293, 0xbfa4784b, 0xcbed606e
	}};

	// Values published by the original PCG32 implementation for seed 42, stream 54.
	// https://www.pcg-random.org/using-pcg-c-basic.html
	static_assert(arrays_equal(prng_outputs(PCG32{42u, 54u}), pcg32_reference));

	constexpr fixed_array<uint64_t, 6> quarkburst64_reference{{
		0x0000060020000002ULL, 0x0403F68CF7217209ULL,
		0xE1C95D285697B7AFULL, 0x394DE3E1A9574CE0ULL,
		0x717616275935DAEFULL, 0x03745D2F175D0105ULL
	}};

	static_assert(
		arrays_equal(prng_outputs(QuarkBurst64::from_state(1, 2, 3)), quarkburst64_reference),
		"QuarkBurst64 output does not match the archived reference implementation"
	);

	// Original RomuDuoJr transition from Mark Overton's 2020 reference code.
	// https://www.romu-random.org/code.c
	struct romu_state{
		uint64_t x;
		uint64_t y;
	};

	constexpr uint64_t romu_next(romu_state& state) noexcept{
		const uint64_t previous_x = state.x;
		state.x = 15241094284759029579ULL * state.y;
		state.y -= previous_x;
		state.y = (state.y << 27) | (state.y >> 37);
		return previous_x;
	}

	constexpr auto romu_reference_outputs(){
		romu_state state{123, 0};
		fixed_array<uint64_t, 6> out{};
		for(size_t i = 0; i < 6; ++i) out[i] = romu_next(state);
		return out;
	}

	static_assert(
		arrays_equal(prng_outputs(RomuDuoJr::from_state(123, 0)), romu_reference_outputs()),
		"RomuDuoJr output does not match the original reference implementation"
	);

	// Bob Jenkins' Small Fast reference implementation.
	// https://burtleburtle.net/bob/rand/smallprng.html
	struct jsf32_state{
		uint32_t a, b, c, d;
	};

	constexpr uint32_t jsf32_next(jsf32_state& state) noexcept{
		const uint32_t e = state.a - rnd::detail::rotl(state.b, 27);
		state.a = state.b ^ rnd::detail::rotl(state.c, 17);
		state.b = state.c + state.d;
		state.c = state.d + e;
		state.d = e + state.a;
		return state.d;
	}

	constexpr jsf32_state jsf32_init(uint32_t seed) noexcept{
		jsf32_state state{0xf1ea5eedu, seed, seed, seed};
		for(unsigned i = 0; i < 20; ++i) jsf32_next(state);
		return state;
	}

	constexpr auto jsf32_reference_outputs(){
		auto state = jsf32_init(123);
		fixed_array<uint32_t, 6> out{};
		for(size_t i = 0; i < 6; ++i) out[i] = jsf32_next(state);
		return out;
	}

	static_assert(
		arrays_equal(prng_outputs(SmallFast32{uint32_t{123}}), jsf32_reference_outputs()),
		"SmallFast32 output does not match Bob Jenkins' reference implementation"
	);

	struct jsf64_state{
		uint64_t a, b, c, d;
	};

	constexpr uint64_t jsf64_next(jsf64_state& state) noexcept{
		const uint64_t e = state.a - rnd::detail::rotl(state.b, 7);
		state.a = state.b ^ rnd::detail::rotl(state.c, 13);
		state.b = state.c + rnd::detail::rotl(state.d, 37);
		state.c = state.d + e;
		state.d = e + state.a;
		return state.d;
	}

	constexpr jsf64_state jsf64_init(uint64_t seed) noexcept{
		jsf64_state state{0xf1ea5eedu, seed, seed, seed};
		for(unsigned i = 0; i < 20; ++i) jsf64_next(state);
		return state;
	}

	constexpr auto jsf64_reference_outputs(){
		auto state = jsf64_init(123);
		fixed_array<uint64_t, 6> out{};
		for(size_t i = 0; i < 6; ++i) out[i] = jsf64_next(state);
		return out;
	}

	static_assert(
		arrays_equal(prng_outputs(SmallFast64{123}), jsf64_reference_outputs()),
		"SmallFast64 output does not match Bob Jenkins' reference implementation"
	);

	// Original xoshiro256** 1.0 transition by David Blackman and Sebastiano Vigna.
	// https://prng.di.unimi.it/xoshiro256starstar.c
	constexpr uint64_t xoshiro_next(fixed_array<uint64_t, 4>& state) noexcept{
		const uint64_t result = rnd::detail::rotl(state[1] * 5, 7) * 9;
		const uint64_t shifted = state[1] << 17;
		state[2] ^= state[0];
		state[3] ^= state[1];
		state[1] ^= state[2];
		state[0] ^= state[3];
		state[2] ^= shifted;
		state[3] = rnd::detail::rotl(state[3], 45);
		return result;
	}

	constexpr fixed_array<uint64_t, 4> xoshiro_initial_state{{
		0xFEEDFACECAFEBEEFULL, 0, 0, 0
	}};

	constexpr auto xoshiro_reference_outputs(){
		auto state = xoshiro_initial_state;
		fixed_array<uint64_t, 6> out{};
		for(size_t i = 0; i < 6; ++i) out[i] = xoshiro_next(state);
		return out;
	}

	static_assert(
		arrays_equal(prng_outputs(Xoshiro256SS::from_state(xoshiro_initial_state)), xoshiro_reference_outputs()),
		"Xoshiro256SS output does not match the original reference implementation"
	);

	// Reference transcription of Pelle Evensen's konadare192px++ transition
	// and the seed expansion used by this port.
	struct konadare_state{
		uint64_t a, b, c;
	};

	constexpr uint64_t konadare_mix(uint64_t x, uint64_t c) noexcept{
		for(uint64_t i = 0; i < 5; ++i){
				x ^= rnd::detail::rotr(x, 25) ^ rnd::detail::rotr(x, 49);
			c += 0xBB67AE8584CAA73BULL + (c << 15) + (c << 7) + i;
			c ^= (c >> 47) ^ (c >> 23);
			x += c;
			x ^= (x >> 11) ^ (x >> 3);
		}
		return x;
	}

	constexpr konadare_state konadare_init(uint64_t seed) noexcept{
		konadare_state state{seed, seed + 1, seed + 2};
		for(int round = 0; round < 2; ++round){
			const uint64_t a = konadare_mix(state.a, state.c);
			const uint64_t b = konadare_mix(state.b, state.a);
			const uint64_t c = konadare_mix(state.c, state.b);
			state = {a, b, c};
		}
		if((state.a | state.b | state.c) == 0) state.a = 0x3C6EF372FE94F82BULL;
		return state;
	}

	constexpr uint64_t konadare_next(konadare_state& state) noexcept{
		const uint64_t output = state.b ^ state.c;
		const uint64_t a = state.a ^ (state.a >> 32);
		state.a += 0xBB67AE8584CAA73BULL;
		state.b = rnd::detail::rotr(state.b + a, 11);
		state.c = rnd::detail::rotl(state.c + state.b, 8);
		return output;
	}

	constexpr auto konadare_reference_outputs(){
		auto state = konadare_init(123);
		fixed_array<uint64_t, 6> out{};
		for(size_t i = 0; i < 6; ++i) out[i] = konadare_next(state);
		return out;
	}

	static_assert(
		arrays_equal(prng_outputs(Konadare192{123}), konadare_reference_outputs()),
		"Konadare192 output does not match its reference transcription"
	);
}
