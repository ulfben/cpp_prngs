#pragma once
#include <concepts>
#include <limits>
#include <random>      // std::uniform_random_bit_generator
#include <type_traits> // std::is_unsigned_v

// Concept: RandomBitEngine
//
// This concept defines the "engine contract" used throughout the library.
//
// Baseline:
// - E models std::uniform_random_bit_generator, so it plugs into <random>
//   utilities (std::shuffle, std::uniform_int_distribution, etc).
// - E supports the standard engine operations we rely on: default construction,
//   seeding, copying, equality, and discard().
//
// Additional library promise (important for bounded generation and bit-mixing):
// - result_type is an *unsigned* integer type
// - min() == 0
// - max() == numeric_limits<result_type>::max()
//


// Concept: RandomBitEngine
//
// Defines the engine contract required by rnd::Random.
//
// An engine must:
// - Model std::uniform_random_bit_generator so it plugs into <random>
//   utilities (std::shuffle, std::uniform_int_distribution, etc).
// - Provide unsigned integral result_type and seed_type aliases.
// - Return result_type exactly from operator().
// - Produce full-width values with min() == 0 and max() == numeric_limits<result_type>::max().
//   In other words: the engine produces full-width, zero-based unsigned values in the inclusive range [min(), max()].
//   These constraints let us treat e() as uniformly distributed over all 2^w bit patterns, which is what the fast 
//   unbiased next(bound) implementation assumes.
// - Support non-throwing default, copy, and seed construction.
// - Support non-throwing generation, seeding, equality, and discard.
//
// Semantic requirements that cannot be enforced by the concept:
// - Outputs are uniformly distributed.
// - seed() restores the default-constructed state.
// - seed(value) produces the same state as construction from value.
// - discard(n) is equivalent to n consecutive calls to operator().
// - Seed and discard arguments are consumed without narrowing.

template<typename E>
concept RandomBitEngine =
requires {
    typename E::result_type;
    typename E::seed_type;
} &&
std::uniform_random_bit_generator<E> &&
std::same_as<typename E::result_type, std::invoke_result_t<E&>> &&
std::default_initializable<E> &&
std::copy_constructible<E> &&
std::constructible_from<E, typename E::seed_type> &&
std::equality_comparable<E> &&
std::is_nothrow_default_constructible_v<E> &&
std::is_nothrow_copy_constructible_v<E> &&
std::is_nothrow_constructible_v<E, typename E::seed_type> &&
std::is_unsigned_v<typename E::result_type> &&
std::numeric_limits<typename E::result_type>::is_integer &&
std::is_unsigned_v<typename E::seed_type> &&
std::numeric_limits<typename E::seed_type>::is_integer &&
(E::min() == typename E::result_type{0}) &&
(E::max() == std::numeric_limits<typename E::result_type>::max()) &&
    requires(E& e, const E& ce, typename E::seed_type seed, unsigned long long n){
        { e() } noexcept -> std::same_as<typename E::result_type>;
        { E::min() } noexcept -> std::same_as<typename E::result_type>;
        { E::max() } noexcept -> std::same_as<typename E::result_type>;
        { ce == ce } noexcept -> std::convertible_to<bool>;
        { e.seed() } noexcept -> std::same_as<void>;
        { e.seed(seed) } noexcept -> std::same_as<void>;
        { e.discard(n) } noexcept -> std::same_as<void>;
};


#ifndef VALIDATE_PRNGS
// Define VALIDATE_PRNGS to enable compile-time validation of PRNG outputs.
#define VALIDATE_PRNGS 1
#endif  

#if VALIDATE_PRNGS
#include <array>
template <typename Engine, typename T = typename Engine::result_type, std::size_t N = 6>
constexpr std::array<T, N> prng_outputs(Engine&& rng) {
    std::array<T, N> out{};
    for (auto& v : out) v = rng();
    return out;
}
#endif // VALIDATE
