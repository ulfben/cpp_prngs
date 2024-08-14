#include <array>
#include <cstdint>
#include <cassert>
#include <limits>
#include <span>
#include <cmath>
/*
A C++ 32-bit two-rotate implementation of the famous Jenkins Small Fast PRNG.
Original public domain C-code and writeup by Bob Jenkins https://burtleburtle.net/bob/rand/SmallFast32.html
C++ implementation by Ulf Benjaminsson (ulfbenjaminsson.com), also placed in the public domain. Use freely!

Satisfies 'UniformRandomBitGenerator', meaning it works well with std::shuffle, std::sample, most of the std::*_distribution-classes, etc.
*/
class SmallFast32
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
    using result_type = u32;

    constexpr SmallFast32(u32 seed = 0xBADC0FFE) noexcept : a(0xf1ea5eed), b(seed), c(seed), d(seed) {        
        // warmup: run the generator a couple of cycles to mix the state thoroughly
        for (auto i = 0; i < 20; ++i) { 
            next();
        }
    }
    constexpr SmallFast32(std::span<const u32, 4> state) noexcept : a(state[0]), b(state[1]), c(state[2]), d(state[3]) {}

    static constexpr result_type max() noexcept{
        return std::numeric_limits<u32>::max();
    }
    
    static constexpr result_type min() noexcept {
        return std::numeric_limits<u32>::lowest();
    }

  	constexpr bool coinToss() noexcept{
        return next() & 1; //checks the least significant bit
    }

    template<typename T>
    constexpr T between(T min, T max) noexcept {
        assert(min < max && "SmallFast32::between(min, max) called with inverted range.");
        if constexpr (std::is_floating_point_v<T>) {
            return min + (max - min) * normalized<T>();
        } else if constexpr (std::is_integral_v<T>) {
            using UT = std::make_unsigned_t<T>;
            UT range = static_cast<UT>(max - min); 
            assert(range != SmallFast32::max() && "SmallFast32::between() - The range is too large and may cause an overflow.");           
            //depending on your needs, consider: if(range == max) return next();
            return min + static_cast<T>(next(range+1));
        }
    }

    template<typename T = float>
    constexpr T normalized() noexcept {
        return static_cast<T>(next() * 0x1.0p-32f);
    }

    template<typename T = float>
    constexpr T unit_range() noexcept {
        static_assert(std::is_floating_point_v<T>, "SmallFast32::unit_range can only be used with floating point types.");
        return static_cast<T>(2.0) * normalized<T>() - static_cast<T>(1.0);
    }

    constexpr result_type next() noexcept {
        const u32 e = a - rot(b, 27); 
        a = b ^ rot(c, 17); 
        b = c + d;
        c = d + e;
        d = e + a;
        return d;
    }

    constexpr result_type operator()() noexcept {
        return next();
    }
	
	constexpr result_type next(u32 bound) noexcept {
		//Lemires' algorithm. See https://www.pcg-random.org/posts/bounded-rands.html
		// And: https://codingnest.com/random-distributions-are-not-one-size-fits-all-part-2/		
		  uint64_t long_mult = next() * uint64_t(bound);
		  u32 low_bits = static_cast<u32>(long_mult);		  
		  if (low_bits < bound) {
			u32 rejection_threshold = -bound % bound;		
			while (low_bits < rejection_threshold) {
			  long_mult = next() * uint64_t(bound);
			  low_bits = u32(long_mult);
			}
		  }
		  return long_mult >> 32;
	}
	
	constexpr result_type operator()(u32 bound) noexcept {
        return next(bound);
    }

    template<typename T = float>
    constexpr T next_gaussian(T mean, T stddev) noexcept {
        static_assert(std::is_floating_point_v<T>, "SmallFast32::next_guassian can only be used with a floating point type");
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

    constexpr bool operator==(const SmallFast32& rhs) const noexcept {
        return (a == rhs.a) && (b == rhs.b) 
            && (c == rhs.c) && (d == rhs.d);
    }

    constexpr bool operator!=(const SmallFast32& rhs) const noexcept {
        return !operator==(rhs);
    }

    constexpr std::array<u32, 4> get_state() const noexcept {
        return {a, b, c, d};
    }
    constexpr void set_state(std::span<const u32, 4> s) noexcept {
        *this = SmallFast32(s);
    }
};

/*sample usage:
int main() {
    SmallPRNG rand(223456322);

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
}*/
