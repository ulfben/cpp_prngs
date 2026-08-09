#pragma once
#include <bit>
#include <cstdint>
#include <limits>

/*
    QuarkBurst64 - an independent C++26 implementation of the
    quarkburst1x64 algorithm by William Stafford Parsons.

    The algorithm was previously published under the name GhostScramble.

    Upstream QuarkBurst C implementation:
    Copyright (c) 2026 Eightomic <eightomic@proton.me>
    https://github.com/eightomic/quarkburst
    Licensed under the BSD 3-Clause License.

    C++26 implementation, seed expansion, value semantics, and
    RandomBitEngine integration:
    Copyright (c) 2026 Ulf Benjaminsson
    https://github.com/ulfben/cpp_prngs

    This file is licensed under the MIT License.
    See LICENSE.md for details.  
*/

class QuarkBurst64 final{
    using u64 = std::uint64_t;

    static constexpr u64 DEFAULT_SEED = 0xFEEDFACECAFEBEEFULL;
    static constexpr u64 INCREMENT = 1'111'111'111'111'111ULL;

    u64 a_{};
    u64 b_{};
    u64 c_{};

    struct Direct{};

    constexpr QuarkBurst64(u64 a, u64 b, u64 c, Direct) noexcept
        : a_(a), b_(b), c_(c){}

    static constexpr u64 splitmix64(u64& state) noexcept{
        state += 0x9E3779B97F4A7C15ULL;
        u64 value = state;
        value = (value ^ (value >> 30)) * 0xBF58476D1CE4E5B9ULL;
        value = (value ^ (value >> 27)) * 0x94D049BB133111EBULL;
        return value ^ (value >> 31);
    }

public:
    using result_type = u64;
    using seed_type = u64;
    using state_type = u64;

    constexpr QuarkBurst64() noexcept
        : QuarkBurst64(DEFAULT_SEED){}

    explicit constexpr QuarkBurst64(seed_type seed_value) noexcept{
        a_ = splitmix64(seed_value);
        b_ = splitmix64(seed_value);
        c_ = splitmix64(seed_value);

        // Diffuse the expanded seed before exposing the first result.
        discard(3);
    }

    static constexpr QuarkBurst64 from_state(
        state_type a,
        state_type b,
        state_type c
    ) noexcept{
        return QuarkBurst64{a, b, c, Direct{}};
    }

    constexpr void seed() noexcept{
        *this = QuarkBurst64{};
    }

    constexpr void seed(seed_type seed_value) noexcept{
        *this = QuarkBurst64{seed_value};
    }

    constexpr result_type next() noexcept{
        a_ = std::rotl(a_, 29) ^ b_;
        b_ += INCREMENT;
        c_ = std::rotl(c_, 41) + a_;
        return c_;
    }

    constexpr result_type operator()() noexcept{
        return next();
    }

    constexpr void discard(unsigned long long n) noexcept{
        while(n--){
            next();
        }
    }

    static constexpr result_type min() noexcept{
        return result_type{0};
    }

    static constexpr result_type max() noexcept{
        return std::numeric_limits<result_type>::max();
    }

    constexpr bool operator==(const QuarkBurst64&) const noexcept = default;
};
