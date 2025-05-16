/*
A demonstration of using the C++ standard library features to generate random numbers.
*/
#include <random>
#include <type_traits>
#include <array>
#include <span>

class Random {
private:
    std::mt19937 rng_;
    
    static auto generate_seed_sequence() {        
        // we need 624 32-bit words to fully initialize MT19937's state
        std::array<std::random_device::result_type, std::mt19937::state_size> entropy_array;        
        std::random_device rd{};
        for (auto& value : entropy_array) {
            value = rd();
        }        
        return std::seed_seq(entropy_array.begin(), entropy_array.end());
    }

public:        
    explicit Random(std::seed_seq seed_seq) : rng_(seed_seq) {}

    //default ctor fully seeds the generator
    Random() : Random(generate_seed_sequence()) {}

    //custom ctors support manual seeding for reproducibility
    explicit Random(std::uint64_t seed) 
        : Random(std::seed_seq{seed}) {}        
    explicit Random(std::span<const std::uint64_t> seed_data) 
        : Random(std::seed_seq(seed_data.begin(), seed_data.end())) {}
    
    void randomize() {
        auto seed_seq = generate_seed_sequence();
        rng_.seed(seed_seq);
    }
    
    unsigned char color() {  // 0-255 inclusive
        std::uniform_int_distribution<int> dist{0, 255};
        return static_cast<unsigned char>(dist(rng_));
    }

    float normalized() {  // [0.0, 1.0)
        std::uniform_real_distribution<float> dist{0.0, 1.0};
        return dist(rng_);
    }

    bool coinToss() {  // true or false with 50% probability
        std::bernoulli_distribution dist{0.5};
        return dist(rng_);
    }

    float unit_range() {  // [-1.0, 1.0]
        std::uniform_real_distribution<float> dist{-1.0, 1.0};
        return dist(rng_);
    }
    
    template<typename T>
    T getNumber(T min, T max) {  
        if constexpr (std::is_integral_v<T>) {           
            std::uniform_int_distribution<T> dist{min, max};
            return dist(rng_);
        } else if constexpr (std::is_floating_point_v<T>) {            
            std::uniform_real_distribution<T> dist{min, max};
            return dist(rng_);
        } else {
            static_assert(std::is_integral_v<T> || std::is_floating_point_v<T>,
                         "getNumber only supports integral and floating point types");
        }
    }
};

/* usage
int main(){
    Random rng;
    
    // Integer ranges (inclusive)
    int i = rng.getNumber(1, 6);
    short s = rng.getNumber<short>(0, 100);
    unsigned long ul = rng.getNumber(0ul, 1000ul);

    // Floating point ranges (half-open)
    float f = rng.getNumber(0.0f, 1.0f);
    double d = rng.getNumber(0.0, 100.0);

    // Special cases
    auto color = rng.color();              // [0,255]
    auto coin = rng.coinToss();            // true/false
    auto norm = rng.normalized();          // [0.0,1.0)
    auto unit = rng.unit_range();          // [-1.0,1.0]
    return s;
}
*/
