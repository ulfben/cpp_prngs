#pragma once
#include "../concepts.hpp" // for RandomBitEngine
#include <array>
#include <bit>
#include <cstdint>
#include <limits>

/*
    QuarkBurst4x64 - an independent C++26 implementation of the
    quarkburst4x64 algorithm by William Stafford Parsons.

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

class QuarkBurst4x64 final{
    using u64 = std::uint64_t;

    static constexpr u64 DEFAULT_SEED = 0xFEEDFACECAFEBEEFULL;
    static constexpr std::uint8_t OUTPUT_COUNT = 4;    
    static constexpr std::size_t WARMUP_BLOCKS = 3;
    static constexpr u64 INCREMENT = 111'111'111'111'111'111ULL;

    u64 a_{};
    u64 b_{};
    u64 c_{};
    u64 d_{};
    std::array<u64, OUTPUT_COUNT> output_{};
    std::uint8_t position_ = OUTPUT_COUNT;

    struct Direct{};
            
    constexpr QuarkBurst4x64(u64 a, u64 b, u64 c, u64 d, Direct) noexcept
        : a_(a), b_(b), c_(c), d_(d){}

    static constexpr u64 splitmix64(u64& state) noexcept{
        state += 0x9E3779B97F4A7C15ULL;
        u64 value = state;
        value = (value ^ (value >> 30)) * 0xBF58476D1CE4E5B9ULL;
        value = (value ^ (value >> 27)) * 0x94D049BB133111EBULL;
        return value ^ (value >> 31);
    }

    constexpr void refill() noexcept{
        a_ = std::rotl(a_, 29) ^ b_;
        b_ += INCREMENT;
        output_[0] = a_ + c_;

        c_ = std::rotl(c_, 47) + a_;
        output_[1] = (a_ + b_) ^ c_;
        
        output_[2] = a_ ^ d_;

        d_ = std::rotl(d_, 25) + a_;
        output_[3] = a_ + std::rotl(d_, 21);

        position_ = 0;
    }

public:
    using result_type = u64;
    using seed_type = u64;
    using state_type = u64;

    constexpr QuarkBurst4x64() noexcept
        : QuarkBurst4x64(DEFAULT_SEED){}

    explicit constexpr QuarkBurst4x64(seed_type seed_value) noexcept{
        a_ = splitmix64(seed_value);
        b_ = splitmix64(seed_value);
        c_ = splitmix64(seed_value);
        d_ = splitmix64(seed_value);

        // discard three full quarkburst4x64 outputs to warm up the state.
        discard(WARMUP_BLOCKS * OUTPUT_COUNT);
    }

    static constexpr QuarkBurst4x64 from_state(
        state_type a,
        state_type b,
        state_type c,
        state_type d
    ) noexcept{
        return QuarkBurst4x64{a, b, c, d, Direct{}};
    }

    constexpr void seed() noexcept{
        *this = QuarkBurst4x64{};
    }

    constexpr void seed(seed_type seed_value) noexcept{
        *this = QuarkBurst4x64{seed_value};
    }

    constexpr result_type next() noexcept{
        if(position_ == OUTPUT_COUNT){
            refill();
        }
        assert(position_ < output_.size() && "Position out of bounds");
        return output_[position_++];
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

    constexpr bool operator==(const QuarkBurst4x64& rhs) const noexcept{
        if(a_ != rhs.a_ ||
            b_ != rhs.b_ ||
            c_ != rhs.c_ ||
            d_ != rhs.d_ ||
            position_ != rhs.position_){
            return false;
        }

        // Only unconsumed cached values affect the future sequence.
        for(std::uint8_t i = position_; i < OUTPUT_COUNT; ++i){
            if(output_[i] != rhs.output_[i]){
                return false;
            }
        }

        return true;
    }
};

static_assert(RandomBitEngine<QuarkBurst4x64>);


#if VALIDATE_PRNGS

/*
    Generated directly from the upstream C implementation with:

        a = 1
        b = 2
        c = 3
        d = 4

    Six values cross the block boundary, validating both refill() and the
    scalar output cache.
*/
inline constexpr std::array<
    QuarkBurst4x64::result_type,
    6
> QuarkBurst4x64_STATE_REFERENCE{
    0x0000000020000005ULL,
    0x018B3EF7846071C9ULL,
    0x0000000020000006ULL,
    0x0005000020400002ULL,
    0x058C3EF7E46071CBULL,
    0x0D2B821E4941D490ULL,
};

static_assert(
    prng_outputs(
        QuarkBurst4x64::from_state(1, 2, 3, 4)
    ) == QuarkBurst4x64_STATE_REFERENCE,
    "QuarkBurst4x64 output does not match the upstream C implementation"
    );

    /*
        This second vector also validates:

        - SplitMix64 seed expansion
        - assignment of all four state words
        - the three-block warm-up
        - transition from the constructor to the first exposed output
    */
inline constexpr std::array<
    QuarkBurst4x64::result_type,
    6
> QuarkBurst4x64_SEED_123_REFERENCE{
    0x5A8A74892FFAE27EULL,
    0xA5285069EBB4CB09ULL,
    0x262A6F735AA804E6ULL,
    0xD68AAA40727C9C96ULL,
    0xC23456AFFE6E3855ULL,
    0x1506210380754AFDULL,
};

static_assert(
    prng_outputs(
        QuarkBurst4x64{123}
    ) == QuarkBurst4x64_SEED_123_REFERENCE,
    "QuarkBurst4x64 seeding or warm-up changed unexpectedly"
    );

#endif // VALIDATE_PRNGS