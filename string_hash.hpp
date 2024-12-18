#include <string_view>
//
// Implementation of the 64-bit variant of FNV-1a (Fowler-Noll-Vo) 
// non-cryptographic hash function.
// 
// References:
// - https://en.wikipedia.org/wiki/Fowler%E2%80%93Noll%E2%80%93Vo_hash_function
// - http://www.isthe.com/chongo/tech/comp/fnv/index.html
// - https://datatracker.ietf.org/doc/html/draft-eastlake-fnv-29
// 
// Note: The FNV algorithm and this implementation are in the public domain.
// Thanks to @lippuu for introducing me to FNV and this handy interface for it! 
struct string_hash {
    // FNV-1a algorithm constants (64-bit variant)
    using u64 = std::uint64_t;
    static constexpr u64 FNV_64_PRIME = 1099511628211ULL;
    static constexpr u64 FNV_64_OFFSET_BASIS = 14695981039346656037ULL;
  
    static constexpr u64 fnv1a(std::string_view str, u64 basis = FNV_64_OFFSET_BASIS) noexcept {
        for (const char c : str) {
            basis ^= static_cast<uint64_t>(std::byte(c));
            basis *= FNV_64_PRIME;
        }
        return basis;
    }
        
    constexpr explicit string_hash(std::string_view str) noexcept 
        : value(fnv1a(str)) {}
    
    // enable comparison operators, for use of string_hash as key in std::map, std::set, etc.
    constexpr auto operator<=>(const string_hash&) const noexcept = default;

    u64 value{0};
};

// User-defined literal for compile time hashing of string literals
constexpr auto operator""_hash(const char* str, std::size_t len) noexcept {
    return string_hash::fnv1a(std::string_view(str, len));
}
