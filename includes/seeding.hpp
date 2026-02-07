#pragma once
#include <chrono> 
#include <cstddef>
#include <cstdint>
#include <ctime> //for std::clock
#include <memory> //for unique_ptr
#include <random>
#include <string_view>
#include <thread>
#include <type_traits>
// Source: https://github.com/ulfben/cpp_prngs/
// Utility functions for seeding Pseudo Random Number Generators.
// 
// This file provides several approaches to generating high-quality seed values,
// both at compile-time and runtime. Each entropy source has different properties
// that make it suitable for different use cases.
//
// Features:
// - Compile-time seeding via string hashing (FNV1a) and source information
// - 'moremur' for high-quality mixing of numeric values
// - Runtime entropy from various sources (time, thread ID, stack pointer, hardware, etc.)
// - Utilities for converting from 64-bit to 32-bit seeds
//
// The entropy sources are designed to be composable - you can use them individually
// or combine them for maximum entropy when needed. See `from_all()` for an example of combining multiple sources.
namespace seed {
	using u64 = std::uint64_t;
	using u32 = std::uint32_t;

	// "moremur" mixing function by Pelle Evensen (2019); see: https://mostlymangling.blogspot.com/2019/12/stronger-better-morer-moremur-better.html
	// Fast, strong 64-bit mixer for hashing and PRNG seeding; outperforms SplitMix64 in avalanche and diffusion tests.
	// Modified to add a constant to avoid the 0 fixed point and to decorrelate low-entropy sequential seeds
	[[nodiscard]] constexpr u64 moremur(u64 x) noexcept{
		x += 0x9E3779B97F4A7C15ULL; // golden ratio increment
		x ^= x >> 27;
		x *= 0x3C79AC492BA7B653ULL;
		x ^= x >> 33;
		x *= 0x1C69B3F74AC4AE35ULL;
		x ^= x >> 27;
		return x;
	}

	// xNASAM mixing function by Pelle Evensen (2020).
	// https://mostlymangling.blogspot.com/2020/01/nasam-not-another-strange-acronym-mixer.html
	//
	// Strong mixer intended for seeding and stream derivation.
	// Compared to classic Murmur/SplitMix-style finalizers (including moremur),
	// xNASAM uses a NASAM-style (rotate/xor/multiply) structure which provides
	// stronger diffusion and fewer detectable correlations in permutation tests.
	//
	// The extra parameter `c` acts as a domain-separation key 
	// allowing independent derivations (e.g. seeding vs. split streams) without
	// introducing simple linear offsets between streams. `c` must be non-zero.	
	[[nodiscard]] constexpr std::uint64_t xnasam(std::uint64_t x, std::uint64_t c = 0x534545442D3031ULL) noexcept{
		x ^= c;
		x ^= std::rotr(x, 25) ^ std::rotr(x, 47);
		x *= 0x9E6C63D0676A9A99ULL; 
		x ^= (x >> 23) ^ (x >> 51);
		x *= 0x9E6D62D06F6A9A9BULL;
		x ^= (x >> 23) ^ (x >> 51);
		return x;
	}

	constexpr u64 from_text(std::string_view str) noexcept{
		// FNV1a for string hashing        
		u64 hash = 14695981039346656037ULL;
		for(const char c : str){
			hash ^= c;
			hash *= 1099511628211ULL;
		}
		return xnasam(hash);
	}

	// Compile-time source information
	// Properties:
	// - Constant during compilation but varies between compilations
	// - Different for each source file
	// - Useful for creating different seeds for different compilation units
	constexpr u64 from_source() noexcept{
		return from_text(__FILE__ ":" __DATE__ ":" __TIME__);
	}

#ifdef __COUNTER__   
   // Generate unique compile-time seeds even within the same compilation unit
   // Properties:
   // - Different seed each time the macro is expanded
   // - Useful for generating unique seeds in the same file
   // unique_from_source(): unique value per TU (function definition).
   //   If inlined or defined in a header, value may be unique per TU.
   // SEED_UNIQUE_FROM_SOURCE: expands to a unique value per macro expansion.
#define STRINGIFY(x) #x
#define STR(x) STRINGIFY(x)
	constexpr u64 unique_from_source() noexcept{
		return from_text(__FILE__ " " __DATE__ " " __TIME__ " " STR(__COUNTER__));
	}
#define SEED_UNIQUE_FROM_SOURCE (seed::from_text(__FILE__ " " __DATE__ " " __TIME__ " " STR(__COUNTER__)))
#endif // __COUNTER__

	// Time-based entropy
	// Properties:	
	// - Uses the platform’s highest-resolution clock.
	// - Good general-purpose default: seeds will usually differ between runs.
	inline u64 from_time() noexcept{
		const auto now = std::chrono::high_resolution_clock::now().time_since_epoch().count();
		return xnasam(now);
	}

	// CPU time consumed by the program
	// Properties:
	// - Varies with CPU frequency scaling
	// - Affected by system load and power states
	// - Can differ significantly from wall time in multi-threaded programs
	inline u64 from_cpu_time() noexcept{
		return xnasam(static_cast<u64>(std::clock()));
	}

	// Thread identifier
	// Properties:
	// - Unique per thread within the process
	// - Stable during thread lifetime
	// - Useful for generating different seeds in multi-threaded contexts
	inline u64 from_thread() noexcept{
	   // Uses std::hash to portably convert thread::id to a 64-bit value.
		const std::thread::id tid = std::this_thread::get_id();
		const size_t value = std::hash<std::thread::id>{}(tid);
		return xnasam(value);
	}

	// Stack address
	// Properties:
	// - Varies between runs due to ASLR (Address Space Layout Randomization)
	// - May be predictable if ASLR is disabled
	// - Usually aligned to specific boundaries
	// - Cheap to obtain
	// - Potentially useful as supplementary entropy source
	inline u64 from_stack() noexcept{
		const u64 dummy = 0;
		return xnasam(reinterpret_cast<std::uintptr_t>(&dummy));
	}

	inline u64 from_heap() noexcept{
		std::unique_ptr<u64> dummy{new (std::nothrow) u64()};
		if(!dummy){ return from_stack(); } // Fallback to stack if allocation fails
		return xnasam(reinterpret_cast<std::uintptr_t>(dummy.get()));
	}

	inline u64 from_global() noexcept{
		// static = stored in global memory, but scoped locally to the function
		// alignas(8) ensures 3 low address bits are zero (more entropy in high bits)
		alignas(8) static const u64 dummy = 0;
		return xnasam(reinterpret_cast<std::uintptr_t>(&dummy));
	}

	// Hardware entropy
	// Properties:
	// - High-quality entropy from system source (when available)
	// - May be slow or limited on some platforms
	// - May fall back to pseudo-random implementation
	// - Best option when available but worth having alternatives
	inline u64 from_system_entropy() noexcept{
		std::random_device rd;
		const auto entropy = (static_cast<u64>(rd()) << 32) | rd();
		return xnasam(entropy);
	}

	// Absorb one entropy value into the running accumulator.
	// The value is combined and then re-mixed so that even small
	// or repeated inputs still significantly change the result.
	[[nodiscard]] constexpr u64 absorb(u64 state, u64 v) noexcept{
		state ^= v;
		state += 0x9E3779B97F4A7C15ULL;
		return xnasam(state, 0x4D49582D3031ULL); // "MIX-01"
	}

	// Aggressively mixes all available entropy sources in "paranoia mode"
	// Any single source can be weak or predictable, but the combination ensures that
	// overlapping, repeated, or low-quality inputs still produce a robust seed.
	// Properties:
	// - Maximum entropy mixing
	// - Higher overhead
	// - Useful when seed quality is critical
	constexpr u64 from_all() noexcept{
		constexpr u64 source =
#ifdef SEED_UNIQUE_FROM_SOURCE
			SEED_UNIQUE_FROM_SOURCE;
#else
			from_source();
#endif	
		u64 seed = absorb(0xD1B54A32D192ED03ULL, source); // mix entropy from source information into a non-zero initial value
		if consteval{			
			return seed;
		} else{ //runtime seeding, so more entropy is available to absorb:
			seed = absorb(seed, from_time());
			seed = absorb(seed, from_thread());
			seed = absorb(seed, from_stack());
			seed = absorb(seed, from_global());
			seed = absorb(seed, from_heap());
			seed = absorb(seed, from_system_entropy());
			seed = absorb(seed, from_cpu_time());		
		}
		return seed;
	}

	[[nodiscard]] constexpr u32 to_32(u64 seed) noexcept{
		return static_cast<u32>(seed ^ (seed >> 32)); //XOR-fold to mix high and low bits when casting to 32 bits.
	}

#undef STRINGIFY
#undef STR
}

/* Example usage:
using rnd::Random;
using seed::to_32; // Convenience alias for converting a 64-bit seed to 32 bits (for smaller engines).

// Compile-time seeding:
constexpr auto seed1 = seed::from_text("my_game_seed");
constexpr auto seed2 = seed::from_source();           // Different for each compilation unit (source file)
constexpr auto seed3 = SEED_UNIQUE_FROM_SOURCE;		  // Different for each macro expansion, even within the same source file. Only available if __COUNTER__ is supported

// Runtime seeding:
Random<SmallFast32> rng1(to_32(seed::from_time()));   // Wall clock time, folded down to 32 bits for SmallFast32
Random<SmallFast64> rng2(seed::from_system_entropy());// Uses std::random_device (hardware/system entropy)

// Pseudo-entropy sources:
Random<RomuDuoJr> rng3(seed::from_thread());          // Unique per thread
Random<PCG32>     rng4(to_32(seed::from_stack()));    // Varies per run of the application, if ASLR is active
Random<PCG32>     rng5(to_32(seed::from_cpu_time())); // Varies with CPU time consumed by the process; can reflect workload or scheduling

// Maximum entropy:
Random<Xoshiro256SS> rng6(seed::from_all());          // Combines all sources (time, thread, stack, entropy, etc.)
*/