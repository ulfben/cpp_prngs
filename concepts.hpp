#pragma once
#include <concepts>
#include <random> //for std::uniform_random_bit_generator
// Concept: RandomBitEngine
// 
// Ensures that every engine E is a standard-compliant uniform random 
// bit generator, compatible with <random> machinery such as std::shuffle 
// and std::uniform_int_distribution etc.
//
// Additionally enforces key parts of the standard RandomNumberEngine
// interface (except for stream operators and std::seed_seq construction),
// so that E can be seeded, copied, and advanced reliably.
//
// This concept is sufficient for use with the high-level Random-interface.
//
// TODO: consider adding save_state() and restore_state() for serialization.
template<typename E>
concept RandomBitEngine =
std::uniform_random_bit_generator<E> &&     // operator() for raw bits, min() / max() for range, result_type for bit width
std::default_initializable<E> &&            // E{} must compile
std::copy_constructible<E> &&               // E copy = other must compile
std::constructible_from<E, typename E::result_type>&& // E engine(seed_value); //constructible from a seed
std::equality_comparable<E>&&               // e1 == e2
   requires(E& e, typename E::result_type seed, unsigned long long n){
      { e.seed() } noexcept;                // reseed to default state
      { e.seed(seed) } noexcept;            // reseed with value
      { e.discard(n) } noexcept;            // advance the state, no return value
      { e.split() } noexcept -> std::same_as<E>; // returns a decorrelated, forked engine; advances this engine's state
};

#define VALIDATE_PRNGS
#ifdef VALIDATE_PRNGS
#include <array>
template <typename Engine, typename T = typename Engine::result_type, std::size_t N = 6>
constexpr std::array<T, N> prng_outputs(Engine&& rng) {
    std::array<T, N> out{};
    for (auto& v : out) v = rng();
    return out;
}
#endif // VALIDATE