#include <array>
#include <cstdint>
#include <cassert>
#include <limits>
#include <span>
#include <cmath>
#include <utility> //for std::pair
/*
A C++ 64-bit three-rotate implementation of the famous Jenkins Small Fast PRNG. 
Original public domain C-code and writeup by Bob Jenkins https://burtleburtle.net/bob/rand/smallprng.html
C++ implementation by Ulf Benjaminsson (ulfbenjaminsson.com), also placed in the public domain. Use freely!

Satisfies 'UniformRandomBitGenerator', meaning it works well with std::shuffle, std::sample, most of the std::*_distribution-classes, etc.
*/
class SmallFast64
 {
    using u64 = uint64_t;    
    u64 a;
    u64 b;
    u64 c;
    u64 d;    

    static constexpr u64 rot(u64 x, u64 k) noexcept {
        return (x << k) | (x >> (64 - k));
    }

public:
    using result_type = u64;

    constexpr SmallFast64(u64 seed = 0xBADC0FFEE0DDF00D) noexcept : a(0xf1ea5eed), b(seed), c(seed), d(seed) {        
        // warmup: run the generator a couple of cycles to mix the state thoroughly
        for (auto i = 0; i < 20; ++i) { 
            next();
        }
    }
    constexpr SmallFast64(std::span<const u64, 4> state) noexcept : a(state[0]), b(state[1]), c(state[2]), d(state[3]) {}

    static constexpr result_type max() noexcept{
        return std::numeric_limits<u64>::max();
    }
    
    static constexpr result_type min() noexcept {
        return std::numeric_limits<u64>::lowest();
    }

  	constexpr bool coinToss() noexcept{
        return next() & 1; //checks the least significant bit
    }

    template<typename T>
    constexpr T between(T min, T max) noexcept {
        assert(min < max && "SmallFast64::between(min, max) called with inverted range.");
        if constexpr (std::is_floating_point_v<T>) {
            return min + (max - min) * normalized<T>();
        } else if constexpr (std::is_integral_v<T>) {
            using UT = std::make_unsigned_t<T>;
            UT range = static_cast<UT>(max - min);        
            assert(range != SmallFast64::max() && "SmallFast64::between() - The range is too large and may cause an overflow.");           
            //depending on your needs, consider: if(range == max) return next();    
            return min + static_cast<T>(next(range + 1));
        }
    }

    template<typename T = float>
    constexpr T normalized() noexcept {
        return static_cast<T>(next() * 0x1.0p-64);
    }

    template<typename T = float>
    constexpr T unit_range() noexcept {
        static_assert(std::is_floating_point_v<T>, "SmallFast64::unit_range can only be used with floating point types.");
        return static_cast<T>(2.0) * normalized<T>() - static_cast<T>(1.0);
    }

    constexpr result_type next() noexcept {
        // The rotate constants (7, 13, 37) are chosen specifically for 64-bit terms, to provide
        // better avalanche characteristics, achieving 18.4 bits of avalanche after 5 rounds.
        const u64 e = a - rot(b, 7); 
        a = b ^ rot(c, 13); 
        b = c + rot(d, 37);
        c = d + e;
        d = e + a;
        return d;
    }

    constexpr result_type operator()() noexcept {
        return next();
    }

	constexpr result_type next(u64 bound) noexcept {
        //can't use Lemire's algorithms since uint128_t isn't portable.		
        return static_cast<result_type>(bound * normalized());		  
	}
	
	constexpr result_type operator()(u64 bound) noexcept {
        return next(bound);
    }

    constexpr std::pair<uint32_t, uint32_t> next_2(uint32_t bound) noexcept {
	//batch generation of bounded integers. Based on https://lemire.me/blog/2024/08/17/faster-random-integer-generation-with-batching/
        u64 random64bit = next();

        // First multiplication: generate the first number in [0, bound)
        u64 long_mult = (random64bit >> 32) * static_cast<u64>(bound);
        uint32_t result1 = static_cast<uint32_t>(long_mult >> 32); // [0, bound)
        uint32_t low_bits = static_cast<uint32_t>(long_mult & 0xFFFFFFFF);

        // Second multiplication (reusing low_bits): generate the second number in [0, bound)
        long_mult = low_bits * static_cast<u64>(bound);
        uint32_t result2 = static_cast<uint32_t>(long_mult >> 32); // [0, bound)
        low_bits = static_cast<uint32_t>(long_mult & 0xFFFFFFFF);
               
        // Check and handle bias
        u64 product_bound = static_cast<u64>(bound) * static_cast<u64>(bound);
        if (low_bits < product_bound) {
            uint32_t threshold = static_cast<uint32_t>(-product_bound % product_bound);
            while (low_bits < threshold) {
                random64bit = next();
                long_mult = (random64bit >> 32) * static_cast<u64>(bound);
                result1 = static_cast<uint32_t>(long_mult >> 32);
                low_bits = static_cast<uint32_t>(long_mult & 0xFFFFFFFF);

                long_mult = low_bits * static_cast<u64>(bound);
                result2 = static_cast<uint32_t>(long_mult >> 32);
                low_bits = static_cast<uint32_t>(long_mult & 0xFFFFFFFF);
            }
        }

        return std::make_pair(result1, result2);
    }

    constexpr std::array<uint16_t, 4> next_4(uint16_t bound) noexcept {
        u64 random64bit = next();
        // First multiplication: generate the first 16-bit number in [0, bound)
        u64 long_mult = (random64bit >> 48) * static_cast<u64>(bound);
        uint16_t result1 = static_cast<uint16_t>(long_mult >> 16); // [0, bound)
        uint16_t low_bits = static_cast<uint16_t>(long_mult & 0xFFFF);

        // Second multiplication: generate the second 16-bit number in [0, bound)
        long_mult = (random64bit >> 32 & 0xFFFF) * static_cast<u64>(bound);
        uint16_t result2 = static_cast<uint16_t>(long_mult >> 16); // [0, bound)
        low_bits = static_cast<uint16_t>(long_mult & 0xFFFF);

        // Third multiplication: generate the third 16-bit number in [0, bound)
        long_mult = (random64bit >> 16 & 0xFFFF) * static_cast<u64>(bound);
        uint16_t result3 = static_cast<uint16_t>(long_mult >> 16); // [0, bound)
        low_bits = static_cast<uint16_t>(long_mult & 0xFFFF);

        // Fourth multiplication: generate the fourth 16-bit number in [0, bound)
        long_mult = (random64bit & 0xFFFF) * static_cast<u64>(bound);
        uint16_t result4 = static_cast<uint16_t>(long_mult >> 16); // [0, bound)
        low_bits = static_cast<uint16_t>(long_mult & 0xFFFF);

        // Bias handling
        u64 product_bound = static_cast<u64>(bound) * static_cast<u64>(bound);
        if (low_bits < product_bound) {
            uint16_t threshold = static_cast<uint16_t>(-product_bound % product_bound);
            while (low_bits < threshold) {
                random64bit = next();
                // Recalculate all four results
                long_mult = (random64bit >> 48) * static_cast<u64>(bound);
                result1 = static_cast<uint16_t>(long_mult >> 16);
                low_bits = static_cast<uint16_t>(long_mult & 0xFFFF);

                long_mult = (random64bit >> 32 & 0xFFFF) * static_cast<u64>(bound);
                result2 = static_cast<uint16_t>(long_mult >> 16);
                low_bits = static_cast<uint16_t>(long_mult & 0xFFFF);

                long_mult = (random64bit >> 16 & 0xFFFF) * static_cast<u64>(bound);
                result3 = static_cast<uint16_t>(long_mult >> 16);
                low_bits = static_cast<uint16_t>(long_mult & 0xFFFF);

                long_mult = (random64bit & 0xFFFF) * static_cast<u64>(bound);
                result4 = static_cast<uint16_t>(long_mult >> 16);
                low_bits = static_cast<uint16_t>(long_mult & 0xFFFF);
            }
        }
        return {result1, result2, result3, result4};
    }


    template<typename T = float>
    constexpr T next_gaussian(T mean, T stddev) noexcept {
        static_assert(std::is_floating_point_v<T>, "SmallFast64::next_guassian can only be used with a floating point type");
        static bool hasSpare = false;
        static T spare{};

        if (hasSpare) {
            hasSpare = false;
            return mean + stddev * spare;
        }

        hasSpare = true;
        T u, v, s;
        do {
            u = unit_range<T>();
            v = unit_range<T>();
            s = u * u + v * v;
        } while (s >= T(1.0) || s == T(0.0));
        s = std::sqrt(T(-2.0) * std::log(s) / s);
        spare = v * s;
        return mean + stddev * (u * s);
    }

    constexpr bool operator==(const SmallFast64& rhs) const noexcept {
        return (a == rhs.a) && (b == rhs.b) 
            && (c == rhs.c) && (d == rhs.d);
    }

    constexpr bool operator!=(const SmallFast64& rhs) const noexcept {
        return !operator==(rhs);
    }

    constexpr std::array<u64, 4> get_state() const noexcept {
        return {a, b, c, d};
    }
    constexpr void set_state(std::span<const u64, 4> s) noexcept {
        *this = SmallFast64(s);
    }
};

//sample usage:
/*int main() {
    SmallFast64 rand(223456321);

    [[maybe_unused]] auto random_unsigned = rand.between(10u, 50u); 
    [[maybe_unused]] int random_int = rand.between(-10, 10); 
    [[maybe_unused]] float random_normalized = rand.normalized(); //0.0 - 1.0
    [[maybe_unused]] float ndc = rand.unit_range();  //-1.0 - +1.0
    //[[maybe_unused]] auto gaus = rand.next_gaussian(70.0f, 10.0f);
    [[maybe_unused]] double random_double = rand.between(1.0, 5.0); 

    auto state = rand.get_state();
    rand.set_state(state);

    // std::array<int, 10> data = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9};
    // std::shuffle(data.begin(), data.end(), rand);

    [[maybe_unused]] auto [val1, val2] = rand.next_2(320u); //two bounded 32-bit values. Max bound 4,294,967,295.
    [[maybe_unused]] auto [v1, v2, v3, v4] = rand.next_4(1080); //four bounded 16-bit values. Max bound 65535
    return v4;
} */
