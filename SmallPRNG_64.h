#include <array>
#include <cstdint>
#include <cassert>
#include <limits>
#include <span>
#include <cmath>
/*
A C++ 64-bit three-rotate implementation of the famous Jenkins Small Fast PRNG. 
Original public domain C-code and writeup by Bob Jenkins https://burtleburtle.net/bob/rand/smallprng.html
C++ implementation by Ulf Benjaminsson (ulfbenjaminsson.com), also placed in the public domain. Use freely!

Satisfies 'UniformRandomBitGenerator', meaning it works well with std::shuffle, std::sample, most of the std::*_distribution-classes, etc.
*/
class SmallPRNG
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

    constexpr SmallPRNG(u64 seed) noexcept : a(0xf1ea5eed), b(seed), c(seed), d(seed) {        
        // warmup: run the generator a couple of cycles to mix the state thoroughly
        for (auto i = 0; i < 20; ++i) { 
            next();
        }
    }
    constexpr SmallPRNG(std::span<const u64, 4> state) noexcept : a(state[0]), b(state[1]), c(state[2]), d(state[3]) {}

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
        assert(min < max && "SmallPRNG::between(min, max) called with inverted range.");
        if constexpr (std::is_floating_point_v<T>) {
            return min + (max - min) * normalized<T>();
        } else if constexpr (std::is_integral_v<T>) {
            using UT = std::make_unsigned_t<T>;
            UT range = static_cast<UT>(max - min);
            UT num = next() % (range + 1);
            return min + static_cast<T>(num);
        }
    }

    template<typename T = float>
    constexpr T normalized() noexcept {
        return static_cast<T>(next()) / (static_cast<long double>(max()) + 1.0L);
    }

    template<typename T = float>
    constexpr T unit_range() noexcept {
        static_assert(std::is_floating_point_v<T>, "SmallPRNG::unit_range can only be used with floating point types.");
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

    template<typename T = float>
    constexpr T next_gaussian(T mean, T stddev) noexcept {
        static_assert(std::is_floating_point_v<T>, "SmallPRNG::next_guassian can only be used with a floating point type");
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

    constexpr std::array<u64, 4> get_state() const noexcept {
        return {a, b, c, d};
    }
    constexpr void set_state(std::span<const u64, 4> s) noexcept {
        *this = SmallPRNG(s);
    }
};

//sample usage:
/*int main() {
    SmallPRNG rand(223456321);

    [[maybe_unused]] auto random_unsigned = rand.between(10u, 50u); 
    [[maybe_unused]] int random_int = rand.between(-10, 10); 
    [[maybe_unused]] float random_normalized = rand.normalized(); //0.0 - 1.0
    [[maybe_unused]] float ndc = rand.unit_range();  //-1.0 - +1.0
    //[[maybe_unused]] auto gaus = rand.next_gaussian(70.0f, 10.0f);
    [[maybe_unused]] double random_double = rand.between(1.0, 5.0); 

    auto state = rand.get_state();
    rand.set_state(state);

    std::array<int, 10> data = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9};
    std::shuffle(data.begin(), data.end(), rand);

    return random_unsigned;
} */
