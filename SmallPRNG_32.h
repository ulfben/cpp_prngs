#include <array>
#include <cstdint>
#include <cassert>
#include <limits>
#include <span>
#include <cmath>
/*
A C++ 32-bit two-rotate implementation of the famous Jenkins Small Fast PRNG.
Original public domain C-code and writeup by Bob Jenkins https://burtleburtle.net/bob/rand/smallprng.html
C++ implementation by Ulf Benjaminsson (ulfbenjaminsson.com), also placed in the public domain. Use freely!
*/
class SmallPRNG
 {
    using u32 = uint32_t;
    u32 a;
    u32 b;
    u32 c;
    u32 d;    

    static constexpr u32 rot(u32 x, u32 k) noexcept {
        return (x << k) | (x >> (32 - k));
    }

public:
    constexpr SmallPRNG(u32 seed) noexcept : a(0xf1ea5eed), b(seed), c(seed), d(seed) {        
        // warmup: run the generator a couple of cycles to mix the state thoroughly
        for (auto i = 0; i < 20; ++i) { 
            next();
        }
    }
    constexpr SmallPRNG(std::span<const u32, 4> state) noexcept : a(state[0]), b(state[1]), c(state[2]), d(state[3]) {}

    constexpr u32 max() noexcept{
        return std::numeric_limits<u32>::max();
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
        static_assert(std::is_floating_point_v<T>, "raunit_range can only be used with floating point types.");
        return static_cast<T>(2.0) * normalized<T>() - static_cast<T>(1.0);
    }

    constexpr u32 next() noexcept {   
        const u32 e = a - rot(b, 27); 
        a = b ^ rot(c, 17); 
        b = c + d;
        c = d + e;
        d = e + a;
        return d;
    }

    template<typename T = float>
    constexpr T next_gaussian(T mean, T stddev) noexcept {
        static_assert(std::is_floating_point_v<T>, "next_guassian can only be used with a floating point type");
        static bool hasSpare = false;
        static T spare;

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

    constexpr std::array<u32, 4> get_state() const noexcept {
        return {a, b, c, d};
    }
    constexpr void set_state(std::span<const u32, 4> s) noexcept {
        *this = SmallPRNG(s);
    }
};
//sample usage:
/*
int main() {
    SmallPRNG rand(223456721);

    [[maybe_unused]] auto random_unsigned = rand.between(10u, 50u); 
    [[maybe_unused]] int random_int = rand.between(-10, 10); 
    [[maybe_unused]] float random_normalized = rand.normalized(); //0.0 - 1.0
    [[maybe_unused]] float ndc = rand.unit_range();  //-1.0 - +1.0
    [[maybe_unused]] auto gaus = rand.next_gaussian(70.0f, 10.0f);
    [[maybe_unused]] double random_double = rand.between(1.0, 5.0); 

    auto state = rand.get_state();
    rand.set_state(state);

    return random_unsigned;
}*/
