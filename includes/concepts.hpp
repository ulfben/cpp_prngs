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
// In other words: the engine produces full-width, zero-based unsigned values,
// in the inclusive range [min(), max()].

// These constraints let us treat e() as uniformly distributed over all 2^w bit patterns, 
// which is what the fast unbiased next(bound) implementation assumes.
template<typename E>
concept RandomBitEngine =
std::uniform_random_bit_generator<E> &&
std::default_initializable<E> &&
std::copy_constructible<E> &&
std::constructible_from<E, typename E::result_type>&&
std::equality_comparable<E>&&
std::is_unsigned_v<typename E::result_type>&&
std::numeric_limits<typename E::result_type>::is_integer &&
(E::min() == typename E::result_type{0}) &&
(E::max() == std::numeric_limits<typename E::result_type>::max()) &&
    requires(E& e, typename E::result_type seed, unsigned long long n){
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