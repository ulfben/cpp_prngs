#pragma once
#include "../detail/bit_operations.hpp"
#include <stdint.h> // AVR-libc provides <stdint.h>, but not the C++ <cstdint> wrapper.

/*
    QuarkBurst64 - an independent modern C++ implementation of the
    quarkburst1x64 algorithm by William Stafford Parsons.

    The algorithm was previously published under the name GhostScramble.

    Upstream QuarkBurst C implementation:
    Copyright (c) 2026 Eightomic <eightomic@proton.me>
    https://github.com/eightomic/quarkburst
    Licensed under the BSD 3-Clause License.

    C++ implementation, seed expansion, value semantics, and
    RandomBitEngine integration:
    Copyright (c) 2026 Ulf Benjaminsson
    https://github.com/ulfben/cpp_prngs

    This file is licensed under the MIT License.
    See LICENSE.md for details.  
*/

namespace rnd {

class QuarkBurst64 final{
    using u64 = uint64_t;

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
    struct state_type{
        u64 a;
        u64 b;
        u64 c;
    };

    constexpr QuarkBurst64() noexcept
        : QuarkBurst64(DEFAULT_SEED){}

    explicit constexpr QuarkBurst64(seed_type seed_value) noexcept{
        a_ = splitmix64(seed_value);
        b_ = splitmix64(seed_value);
        c_ = splitmix64(seed_value);

        // Diffuse the expanded seed before exposing the first result.
        discard(3);
    }

    constexpr state_type state() const noexcept{
        return {a_, b_, c_};
    }

    static constexpr QuarkBurst64 from_state(state_type state) noexcept{
        return QuarkBurst64{state.a, state.b, state.c, Direct{}};
    }

    constexpr void seed() noexcept{
        *this = QuarkBurst64{};
    }

    constexpr void seed(seed_type seed_value) noexcept{
        *this = QuarkBurst64{seed_value};
    }

    constexpr result_type next() noexcept{
        a_ = rnd::detail::rotl(a_, 29) ^ b_;
        b_ += INCREMENT;
        c_ = rnd::detail::rotl(c_, 41) + a_;
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

    static constexpr result_type (min)() noexcept{ // Parentheses prevent expansion of Arduino's min macro.
        return result_type{0};
    }

    static constexpr result_type (max)() noexcept{ // Parentheses prevent expansion of Arduino's max macro.
        // Equivalent to std::numeric_limits<result_type>::max(), but <limits> is not available on AVR-libc.
        return static_cast<result_type>(~result_type{0});
    }

    constexpr bool operator==(const QuarkBurst64& rhs) const noexcept{
        return a_ == rhs.a_ && b_ == rhs.b_ && c_ == rhs.c_;
    }
    constexpr bool operator!=(const QuarkBurst64& rhs) const noexcept{
        return !(*this == rhs);
    }
};

} // namespace rnd
