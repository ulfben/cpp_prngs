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
    //std::mt19937 constructor require an l-value, hence why we take seed_seq by value here
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
    
    int getInt(int from, int thru) {  // [from, thru] inclusive
        std::uniform_int_distribution<int> dist{from, thru};
        return dist(rng_);
    }
    
    template<typename T>
    typename std::enable_if_t<std::is_floating_point_v<T>, T>
    getFloat(T from = T{0}, T upto = T{1}) {  // [from, upto) exclusive
        std::uniform_real_distribution<T> dist{from, upto};
        return dist(rng_);
    }
    
    template<typename T>
    typename std::enable_if_t<std::is_integral_v<T>, T>
    getNumber(T min, T max) {  // [min, max] inclusive
        std::uniform_int_distribution<T> dist{min, max};
        return dist(rng_);
    }
    
    template<typename T>
    typename std::enable_if_t<std::is_floating_point_v<T>, T>
    getNumber(T from, T upto) {  // [from, upto) exclusive
        return getFloat(from, upto);
    }
};

/*
Example usage: 

int main(){
    Random r;
    return r.getNumber(0, 255 );
}

*/
