/**
 * 
 * This file implements a small and flexible generic hashing framework that allows:
 * 1. Easy swapping of hash algorithms via template parameters
 * 2. Extensible hashing for custom types through hash_append overloads
 * 3. Stateful hashing to combine multiple values
 * 
 * The default hash algorithm provided is FNV-1a (Fowler-Noll-Vo hash).
 * 
 * //Ulf Benjaminsson, 2025
 */
#include <type_traits>
#include <cstdint>
#include <compare>
#include <string_view>
#include <print>
#include <cassert>
#include <span>
#include <vector>
#include <array>
#include <optional>
#include <variant>
using namespace std::literals::string_view_literals;

namespace hash {
//-----------------------------------------------------------------------------
// Hash Function Overloads
//-----------------------------------------------------------------------------

/**
 * Hash overload for arithmetic types (integers, floating point)
 * Simply passes the raw bytes of the value to the hasher
 */
template<class Hasher, typename T> requires std::is_arithmetic_v<T>
constexpr void hash_append(Hasher& h, T n) noexcept {
    h(&n, sizeof(n));
}

/**
 * Hash overload for pointers
 * Hashes the address value itself
 */
template<class Hasher, class T>
constexpr void hash_append(Hasher& h, T* p) noexcept {
    h(&p, sizeof(p));
}

/**
 * Hash overload for std::optional
 * Hashes whether the optional contains a value and if so, hashes the value
 */
template<class Hasher, typename T>
constexpr void hash_append(Hasher& h, const std::optional<T>& opt) noexcept {
    bool has_value = opt.has_value();
    hash_append(h, has_value);
    if (has_value) {
        hash_append(h, *opt);
    }
}

/**
 * Hash overload for std::variant
 * Hashes the index of the active alternative and its value
 */
template<class Hasher, typename... Types>
constexpr void hash_append(Hasher& h, const std::variant<Types...>& v) noexcept {
    const std::size_t idx = v.index();
    hash_append(h, idx);
    std::visit([&h](const auto& value) {
        hash_append(h, value);
    }, v);
}

/**
 * Hash overload for contiguous containers using std::span
 * Works with any contiguous container (array, vector, string, etc.)
 */
template<class Hasher, typename T>
constexpr void hash_append(Hasher& h, std::span<const T> data) noexcept {  
    hash_append(h, data.size()); //size prefix to disambiguate "aa"+"a" from "aaa"
    if constexpr (std::is_trivially_copyable_v<T>) {        
        h(data.data(), data.size() * sizeof(T)); // For trivial types, hash the entire span at once        
    } else {        
        for (const auto& elem : data) {
            hash_append(h, elem); // For non-trivial types, hash each element individually
        }
    }
}

// Convenience overloads for common container types
template<class Hasher, typename T, std::size_t N>
constexpr void hash_append(Hasher& h, const std::array<T, N>& arr) noexcept {
    hash_append(h, std::span<const T>{arr});
}

template<class Hasher, typename T, typename Allocator>
constexpr void hash_append(Hasher& h, const std::vector<T>& vec) noexcept {
    hash_append(h, std::span<const T>{vec});
}

template<class Hasher>
constexpr void hash_append(Hasher& h, std::string_view s) noexcept {
    using Char = std::string_view::value_type;
    hash_append(h, std::span<const Char>{s});    
}

template<class Hasher>
constexpr void hash_append(Hasher& h, std::wstring_view s) noexcept {
    hash_append(h, std::span<const wchar_t>{s});
}

template<class Hasher>
constexpr void hash_append(Hasher& h, std::u16string_view s) noexcept {
    hash_append(h, std::span<const char16_t>{s});    
}

template<class Hasher>
constexpr void hash_append(Hasher& h, std::u32string_view s) noexcept {
    hash_append(h, std::span<const char32_t>{s});      
}

/**
 * Concept defining requirements for hash algorithms
 * Any hash algorithm used with Hasher must satisfy these requirements
 */
template<typename T>
concept HashAlgorithm = requires(T h, const void* data, std::size_t len) {
    typename T::result_type;
    { h(data, len) } noexcept -> std::same_as<typename T::result_type>;
    { h.current() } noexcept -> std::same_as<typename T::result_type>;
    { h.finalize() } noexcept -> std::same_as<typename T::result_type>;  // FNV-1a does not need finalization but other algorithms might.   
    { h.reset() } noexcept;    
};

/**
 * FNV-1a hash algorithm implementation
 * Reference: http://www.isthe.com/chongo/tech/comp/fnv/
 * For a minimal implementation see: https://github.com/ulfben/cpp_prngs/blob/main/string_hash.hpp
 * The core feature of this class is making the algorithm stateful
 * Making it very easy to hash multiple values in sequence, including aggregate and custom types.
 */
class fn1va {
    using u64 = std::uint64_t;
    static constexpr u64 FNV_64_PRIME = 1099511628211ULL;
    static constexpr u64 FNV_64_OFFSET_BASIS = 14695981039346656037ULL;
    std::size_t hash = FNV_64_OFFSET_BASIS;

public:
    using result_type = u64;
    
    constexpr fn1va() noexcept = default;
    
    constexpr fn1va(void const* key, std::size_t len) noexcept {
        operator()(key, len);
    }
    
    constexpr result_type operator()(void const* key, std::size_t len) noexcept {
        assert(key != nullptr);        
        auto const* data = static_cast<std::byte const*>(key);
        for(std::size_t i = 0; i < len; ++i) {
            hash ^= std::to_integer<u64>(data[i]);
            hash *= FNV_64_PRIME;
        }
        return hash;
    }

    constexpr result_type current() const noexcept { return hash; }
    constexpr result_type finalize() const noexcept{ return current(); } 
    constexpr void reset() noexcept { hash = FNV_64_OFFSET_BASIS; }
    
    explicit constexpr operator std::size_t() const noexcept {
        return current();
    }
    
    constexpr auto operator<=>(const fn1va& other) const noexcept = default;
};

/**
 * Generic hasher wrapper that can work with any algorithm satisfying HashAlgorithm
 * Provides a convenient interface for hashing single or multiple values
 */
template<HashAlgorithm Algo = fn1va>
class Hasher {
    Algo hasher{};
    
public:
    using result_type = typename Algo::result_type;
    
    constexpr Hasher() noexcept = default;
    
    // Variadic constructor that immediately hashes all arguments
    template<typename... Args>
    constexpr explicit Hasher(const Args&... args) noexcept {
        (hash_append(hasher, args), ...);
    }

    /**
     * Hash multiple arguments in sequence
     * Returns current hash value after processing all arguments
     */
    template<typename... Args>
    constexpr result_type operator()(const Args&... args) noexcept {
        (hash_append(hasher, args), ...);
        return current();
    }        

    constexpr result_type current() const noexcept { return hasher.current(); }
    constexpr result_type finalize() const noexcept { return hasher.finalize(); }
    constexpr void reset() noexcept { hasher.reset(); }
    
    
    constexpr auto operator<=>(const Hasher& other) const noexcept = default;        
    constexpr auto operator<=>(const result_type& other) const noexcept {
        return current() <=> other;
    }    
    // Implicit conversion to result_type
    constexpr operator result_type() const noexcept {
        return current();
    }
};

} // namespace hash

//-----------------------------------------------------------------------------
// Example Usage
//-----------------------------------------------------------------------------

// Example of adding hash support for a custom type
class MyType {
private:
    int member1 = 1;
    float member2 = 1.2f;
    char member3 = 'a';
    std::string_view member4{"korvmos"};
    std::array<int, 3> member5{1, 2, 3};
    
public:
    // Friend function for hashing - provides access to private members
    template<class Hasher>
    constexpr friend void hash_append(Hasher& h, const MyType& mt) noexcept {
        h(mt.member1, mt.member2, mt.member3, mt.member4, mt.member5);
        //OR do it manually:
        //hash_append(h, mt.member1);
        //hash_append(h, mt.member2);
        //... etc..        
    }
};

// Example usage
int main() {
    using hash::Hasher; // Default hash algorithm is FNV-1a

    //TODO: figure out a type-punning workaround to let us run in constexpr contexts.
    // std::bit_cast seems to be the intended solution, but how do I make use of it without
    // ruining the ergonomics of the Hasher interface?
    assert(Hasher("a"sv)("aa"sv) == Hasher("a"sv, "aa"sv)); //construct-and-append is the same as variadic construction    
    assert(Hasher("aaa"sv) != Hasher("a"sv, "aa"sv));  //"aaa" is different from "a" + "aa"
    assert(Hasher("test"sv) == Hasher("test"sv)); //same string should hash to the same value
    assert(Hasher("a"sv, "b"sv) != Hasher("b"sv, "a"sv)); //order matters

    const auto mt = MyType{};      
    auto hasher = Hasher{};
    hash_append(hasher, mt); //calls MyType's hash_append overload
    std::println("{}", hasher.current());
    return 0;
}
