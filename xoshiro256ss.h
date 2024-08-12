#include <array>
#include <bitset>
#include <cassert>
#include <concepts>
#include <cstdint>
#include <limits>
#include <span>
#include <type_traits>

// The "xoshiro256** 1.0" generator.
// Public interface, rejection sampling and seeding utilities (see; Seeding.h) 
// by Ulf Benjaminsson (2023)
// https://ulfbenjaminsson.com/
// Based on C++ port by Arthur O'Dwyer (2021).
// https://quuxplusone.github.io/blog/2021/11/23/xoshiro/
// of the C version by David Blackman and Sebastiano Vigna (2018),
// https://prng.di.unimi.it/xoshiro256starstar.c
// splitmix64 by Sebastiano Vigna (2015) 
// https://prng.di.unimi.it/splitmix64.c

class RNG{
public:
    using u64 = std::uint_fast64_t;
    static constexpr std::size_t SEED_COUNT = 4;
    using State = std::array<u64, SEED_COUNT>;
    using Span = std::span<const u64, SEED_COUNT>;
    static constexpr auto USE_REJECTION_SAMPLING = false;
    // When enabled, all ranged integer functions will use rejection sampling to ensure a more uniform 
    // distribution of random numbers across large integer ranges. The concern with large ranges is that
    // methods like modulo reduction (randNum % range) might not evenly distribute numbers 
    // across the range, leading to bias. As a rule of thumb, if the range is more than half of 
    // the maximum output (e.g., more than 2^63), you might start to see the benefits of using 
    // rejection sampling to ensure uniformity.

    constexpr explicit RNG(u64 seed) noexcept{
        s[0] = splitmix64(seed);
        s[1] = splitmix64(s[0] + 0x9E3779B97F4A7C15uLL);
        s[2] = splitmix64(s[1] + 0x7F4A7C15uLL);  
        s[3] = splitmix64(s[2] + 0x9E3779B9uLL);
    }

    constexpr explicit RNG(Span seeds) noexcept{
        std::ranges::copy(seeds, s.begin());
    }

    static constexpr u64 max() noexcept{
        return std::numeric_limits<u64>::max();
    }

    constexpr u64 next() noexcept{
        const u64 result = rotl(s[1] * 5, 7) * 9;
        const u64 t = s[1] << 17;
        s[2] ^= s[0];
        s[3] ^= s[1];
        s[1] ^= s[2];
        s[0] ^= s[3];
        s[2] ^= t;
        s[3] = rotl(s[3], 45);
        return result;
    }

    constexpr bool coinToss() noexcept{
        return next() & 1; //checks the least significant bit
    }

    //returns a random number in the range [0, 1)
    template<std::floating_point Real>
    constexpr Real normalized() noexcept{
        return static_cast<Real>(next()) / static_cast<Real>(max());
    }

    template<std::floating_point Real>
    constexpr Real inRange(Real range) noexcept{
        return normalized<Real>() * range;
    }

    template<std::floating_point Real>
    constexpr Real inRange(Real from, Real to) noexcept{
        assert(from < to && "RNG: inverted range.");
        return from + normalized<Real>() * (to - from);
    }

    template<std::integral T>
    constexpr T inRange(T from, T to) noexcept{
        assert(from < to && "RNG: inverted range.");
        using UT = std::make_unsigned_t<T>;
        UT range = static_cast<UT>(to - from);
        return static_cast<T>(inRange(range)) + from;
    }

    template<std::integral T>
    constexpr T inRange(T range) noexcept{
        using UT = std::make_unsigned_t<T>;
        UT num = static_cast<UT>(inRange(static_cast<u64>(std::abs(range))));
        return (range < 0) ? -static_cast<T>(num) : static_cast<T>(num);
    }

    constexpr u64 inRange(u64 range) noexcept{
        if(range == 0){
            assert(false && "RNG::inRange(u64) called with empty range!");
            return 0;
        }
        if constexpr(USE_REJECTION_SAMPLING){
            return uniformRandom(range);
        }
        return next() / (max() / range);
    }

    constexpr Span state() const{
        return {s};
    }

    /* the jump() function is equivalent to 2^128 calls to next();
    it can be used to generate 2^128 non-overlapping subsequences
    for parallel computations. */
    constexpr void jump() noexcept{
        constexpr std::array<std::bitset<64>, SEED_COUNT> JUMP{
            0x180ec6d33cfd0abaULL, 0xd5a61266f0c9392cULL, 0xa9582618e03fc9aaULL, 0x39abdc4529b1661cULL
        };
        State temp{0};
        for(const auto& bits : JUMP){
            for(std::size_t b = 0; b < 64; ++b){
                if(bits.test(b)){
                    temp[0] ^= s[0];
                    temp[1] ^= s[1];
                    temp[2] ^= s[2];
                    temp[3] ^= s[3];
                }
                next();
            }
        }
        s = temp;
    }

    static constexpr u64 splitmix64(u64 x) noexcept{
        u64 z = (x + 0x9e3779b97f4a7c15uLL);
        z = (z ^ (z >> 30)) * 0xbf58476d1ce4e5b9uLL;
        z = (z ^ (z >> 27)) * 0x94d049bb133111ebuLL;
        return z ^ (z >> 31);
    }
    
private:
    State s{};

    static constexpr u64 rotl(u64 x, int k) noexcept{
        return (x << k) | (x >> (64 - k));
    }

    //uses rejection sampling to ensure fair scaling. Costlier than inRange(u64)
    //if you prefer to always use rejection sampling, set USE_REJECTION_SAMPLING to true.
    constexpr u64 uniformRandom(u64 range) noexcept{
        if(range == 0){
            assert(false && "RNG::uniformRandom called with empty range!");
            return 0;
        }
        const u64 rangeLimit = max() - range + 1;
        u64 n = next();
        while((n - (n % range)) >= rangeLimit){
            n = next();
        }
        return n % range;
    }
};
