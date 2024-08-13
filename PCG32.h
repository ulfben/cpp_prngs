#include <cstdint>
#include <limits>
//constexpr variant of the PCG (Permuted Congruential Generator) algorithm by Melissa O'Neill
//See: https://www.pcg-random.org/download.html#minimal-c-implementation

struct PCG32{    
    using result_type = std::uint32_t;    
    
    constexpr PCG32(std::uint64_t initstate, std::uint64_t initseq = 1) noexcept : state(initstate), inc((initseq << 1u) | 1u) {        
        for (auto i = 0; i < 20; ++i) { 
            next();
        }
    }

    constexpr result_type next() noexcept{
        const auto oldstate = state;        
        state = oldstate * 6364136223846793005ULL + (inc|1);        
        const auto xorshifted = ((oldstate >> 18u) ^ oldstate) >> 27u;
        const auto rot = oldstate >> 59u;    
        return static_cast<std::uint32_t>((xorshifted >> rot) | (xorshifted << ((-rot) & 31)));
    }
    
    constexpr result_type next(std::uint32_t bound) noexcept{
        //highly performant Lemire's algorithm (Debiased Integer Multiplication) after research by Melissa O'Neill
        // https://www.pcg-random.org/posts/bounded-rands.html
        uint32_t x = next();
        uint64_t m = uint64_t(x) * uint64_t(bound);
        uint32_t l = uint32_t(m);
        if (l < bound) {
            uint32_t t = -bound;
            if (t >= bound) {
                t -= bound;
                if (t >= bound) 
                    t %= bound;
            }
            while (l < t) {
                x = next();
                m = uint64_t(x) * uint64_t(bound);
                l = uint32_t(m);
            }
        }
        return m >> 32;
	} 
    
    constexpr result_type operator()() noexcept{    		
        return next();
    }	
    constexpr result_type operator()(std::uint32_t bound) noexcept{    		
        return next(bound);
    }
    constexpr float normalized() noexcept{ 
        return 0x1.0p-32 * next(); //again, courtesy of Melissa O'Neill. See: https://www.pcg-random.org/posts/bounded-rands.html
        //0x1.0p-32 is a binary floating point constant for 2^-32 that we use to convert a random 
        // integer value in the range [0..2^32) into a float in the unit interval 0-1        
        //A less terrifying, but also less efficient, way to achieve the same goal  is:         
        //    return static_cast<float>(std::ldexp(next(), -32));
        //see https://mumble.net/~campbell/tmp/random_real.c for more info.
        
    }
    static result_type constexpr min() noexcept {
        return std::numeric_limits<result_type>::lowest();
    }
    static result_type constexpr max() noexcept {
        return std::numeric_limits<result_type>::max();
    }

private:
    std::uint64_t state = 0;  
    std::uint64_t inc = 0; 
};

//sample usage:
/*int main() {        
    PCG32 rng(45677);
    auto f = rng.normalized();
    return static_cast<int>(f*100);
} */
