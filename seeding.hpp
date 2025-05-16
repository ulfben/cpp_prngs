#pragma once
#include <chrono>
#include <random>
#include <thread>
#include <string_view>
// Utility functions for seeding PRNGs (Pseudo Random Number Generators).
// 
// This file provides several approaches to generating high-quality seed values,
// both at compile-time and runtime. Each entropy source has different properties
// that make it suitable for different use cases.
//
// Features:
// - Compile-time seeding via string hashing (FNV1a) and source information
// - SplitMix64 for high-quality mixing of numeric values
// - Runtime entropy from various sources (time, thread ID, hardware, etc.)
// - Utilities for converting from 64-bit to 32-bit seeds
//
// The entropy sources are designed to be composable - you can use them individually
// or combine them for maximum entropy when needed.
namespace seed {
    using u64 = std::uint64_t;
    using u32 = std::uint32_t;

    // SplitMix64 mixing function - fast and high-quality mixing of 64-bit values
    constexpr u64 splitmix64(u64 x) noexcept {
        x += 0x9e3779b97f4a7c15;
        x = (x ^ (x >> 30)) * 0xbf58476d1ce4e5b9;
        x = (x ^ (x >> 27)) * 0x94d049bb133111eb;
        return x ^ (x >> 31);
    }

    // FNV1a for string hashing
    constexpr u64 fnv1a(std::string_view str) noexcept {
        u64 hash = 14695981039346656037ULL;
        for (const char c : str) {
            hash ^= static_cast<u64>(static_cast<std::byte>(c));
            hash *= 1099511628211ULL;
        }
        return hash;
    }

    // Compile-time source information
    // Properties:
    // - Constant during compilation but varies between compilations
    // - Different for each source file
    // - Useful for creating different seeds for different compilation units
    constexpr u64 from_source() noexcept {
        return fnv1a(__FILE__ ":" __DATE__ ":" __TIME__);
    }

    // Generate unique compile-time seeds even within the same compilation unit
    // Properties:
    // - Different seed each time it's calle
    // - Useful for seeding multiple PRNGs at compile time
#ifdef __COUNTER__
    constexpr u64 unique_from_source() noexcept {
        return fnv1a(__FILE__ " " __DATE__ " " __TIME__ " " + std::to_string(__COUNTER__));
    }
#endif

    // Wall clock time
    // Properties:
    // - High resolution (typically nanoseconds)
    // - Monotonic (always increases)
    // - Reflects real-world time progression
    // - May be synchronized across machines
    // - Useful default! seeds will vary with time
    inline u64 from_time() noexcept {
        const auto now = std::chrono::high_resolution_clock::now().time_since_epoch().count();
        return splitmix64(now);
    }

    // CPU time consumed by the program
    // Properties:
    // - Varies with CPU frequency scaling
    // - Affected by system load and power states
    // - Can differ significantly from wall time in multi-threaded programs
    inline u64 from_cpu_time() noexcept {
        return splitmix64(static_cast<u64>(std::clock()));
    }

    // Thread identifier
    // Properties:
    // - Unique per thread within the process
    // - Stable during thread lifetime
    // - Useful for generating different seeds in multi-threaded contexts
    inline u64 from_thread() noexcept {
        const auto& tid = std::this_thread::get_id();
        return splitmix64(std::bit_cast<u64>(tid));
    }

    // Stack address
    // Properties:
    // - Varies between runs due to ASLR (Address Space Layout Randomization)
    // - May be predictable if ASLR is disabled
    // - Usually aligned to specific boundaries
    // - Cheap to obtain
    // - Potentially useful as supplementary entropy source
    inline u64 from_stack() noexcept {
        const auto dummy = 0;  // local variable just for its address
        return splitmix64(reinterpret_cast<std::uintptr_t>(&dummy));
    }

    // Hardware entropy
    // Properties:
    // - High-quality entropy from system source (when available)
    // - May be slow or limited on some platforms
    // - May fall back to pseudo-random implementation
    // - Best option when available but worth having alternatives
    inline u64 from_entropy() noexcept {
        std::random_device rd;
        const auto entropy = (static_cast<u64>(rd()) << 32) | rd();
        return splitmix64(entropy);
    }

    // Combines all available entropy sources
    // Properties:
    // - Maximum entropy mixing
    // - Higher overhead
    // - Useful when seed quality is critical
    // - Good for initializing large-state PRNGs
    inline u64 from_all() noexcept {
        const auto time = from_time();
        const auto cpu = from_cpu_time();
        const auto thread = from_thread();
        const auto stack = from_stack();
        const auto entropy = from_entropy();
        const auto source = from_source();
        return splitmix64(time ^ cpu ^ thread ^ stack ^ entropy ^ source);
    } 

    inline u32 to_32(u64 seed) noexcept {
        return static_cast<u32>(seed ^ (seed >> 32)); //XOR-fold to preserve entropy
    }   
}

/* Example usage:

// Compile-time seeding:
constexpr auto seed1 = seed::fnv1a("my_game_seed");
constexpr auto seed2 = seed::from_source();

// Runtime seeding 
SmallFast32 rng1(seed::to_32(seed::from_time())); // wall clock time

// Thread-specific seeding:
SmallFast64 rng2(seed::from_thread());

// Maximum entropy seeding:
PCG32 rng3(seed::from_all());
*/
