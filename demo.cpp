#include <https://raw.githubusercontent.com/ulfben/cpp_prngs/refs/heads/main/includes/concepts.hpp>
#include <https://raw.githubusercontent.com/ulfben/cpp_prngs/refs/heads/main/includes/detail/wide_multiply.hpp> //constexpr fallback for 128bit multiplication on msvc
#include <https://raw.githubusercontent.com/ulfben/cpp_prngs/refs/heads/main/includes/engines/pcg32.hpp> //pcg32 engine
#include <https://raw.githubusercontent.com/ulfben/cpp_prngs/refs/heads/main/includes/engines/romuduojr.hpp> // RomuDuoJr engine 
#include <https://raw.githubusercontent.com/ulfben/cpp_prngs/refs/heads/main/includes/random.hpp> // the Random interface, which wraps any engine to provide a rich set of random generation features
#include <algorithm>
#include <array>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <print>
#include <string_view>
#include <vector>

// Source:
// https://github.com/ulfben/cpp_prngs/
//
// Random<E> wraps any engine satisfying RandomBitEngine.
//
// General-purpose engines:
// - Konadare192       // 64-bit output
// - PCG32             // 32-bit output
// - QuarkBurst64      // 64-bit output
// - RomuDuoJr         // 64-bit output
// - SmallFast32       // 32-bit output
// - SmallFast64       // 64-bit output
// - Xoshiro256SS      // 64-bit output
//
// Narrow-output engines for resource-constrained targets:
// - SmallFast8        // 8-bit output, 4-byte state
// - SmallFast16       // 16-bit output, 8-byte state
// - XorShift32Star8   // 8-bit output, 4-byte state
//
// Random<E> works with every engine above. When necessary, it combines multiple
// engine outputs to produce wider random values. Bounds, collection sizes, and
// total weights must still fit in the engine's result_type.
// 
// AVR-libc targets can instead include random_avr.hpp, a C++17 frontend that
// uses C arrays and pointer-plus-length buffers rather than C++20 ranges.
//
// The library operations are constexpr where their inputs permit constant evaluation.
//
// Demo is available on Compiler Explorer: https://compiler-explorer.com/z/zTh6nazxj
// Benchmarks: https://github.com/ulfben/cpp_prngs#performance-benchmarks

int main(){
   using namespace rnd;       
   using Engine = RomuDuoJr; // Change this to PCG32 to try the same interface with a 32-bit engine.
   using RNG = Random<Engine>;
   static_assert(RandomBitEngine<Engine>);
   static_assert(std::uniform_random_bit_generator<RNG>);
   
   constexpr std::string_view str{"abcdefghijklmnopqrstuvwxyz"};
   std::vector<int> vec{1,2,3,4,5,6,7,8,9,10};
   
   RNG random{}; // Deterministic engine-specific default seed.
   
   std::println("Random<E>:");
   std::println("  next() [{}, {}]: {}", RNG::min(), RNG::max(), random.next());        // raw engine output: [min, max] inclusive. Same as 'random()'
   std::println("  next(100) [0, 100): {}\n", random.next(100));                        // half-open: [0, 100). Same as 'random(100)'

   std::println("  between [10, 20): {}", random.between(10, 20));                      // half-open
   std::println("  between [5.0f, 10.0f): {}\n", random.between(5.0f, 10.0f));          // half-open

   std::println("  normalized [0.0f, 1.0f): {}", random.normalized());                  // float by default (normalized<double>() for double)
   std::println("  signed_norm [-1.0f, 1.0f): {}\n", random.signed_norm());             // float by default (signed_norm<double>() for double)

   std::println("  coin_flip(): {}", random.coin_flip());                               // fair coin
   std::println("  coin_flip(0.9f): {}\n", random.coin_flip(0.9f));                     // ~90% true (weighted coin)

   std::println("  bits_as<uint8_t>(): {:08b}b", random.bits_as<std::uint8_t>());       // fill an 8-bit value with random bits
   std::println("  bits<24, uint32_t>(): #{:06x}", random.bits<24, std::uint32_t>());   // 24 random bits (0xRRGGBB)
   std::println("  bits<uint64_t>(7): {:07b}b\n", random.bits<std::uint64_t>(7));       // runtime: 7 random bits in low bits

   std::println("  gaussian(0.0, 1.0) sample: {}\n", random.gaussian(0.0f, 1.0f));

   std::println("  element(str): {}", random.element(str));                             // random element from a sized forward range
   std::size_t i = random.index(str);                                                   // random index in [0, size)
   std::println("  index(str): {} ({})", i, str[i]);                                  

   // weighted_index() takes the weights directly, so it does not need a projection.
   constexpr std::array loot_tiers{"common", "uncommon", "rare", "legendary"};
   constexpr std::array<unsigned, 4> tier_weights{50u, 30u, 15u, 5u};
   const std::size_t tier = random.weighted_index(tier_weights);
   std::println("  weighted_index: {} (weight {})", loot_tiers[tier], tier_weights[tier]);

   struct LootDrop{
      std::string_view name;
      unsigned weight;
   };
   constexpr std::array loot_table{
      LootDrop{"potion", 50u},
      LootDrop{"gold", 30u},
      LootDrop{"magic sword", 15u},
      LootDrop{"dragon egg", 5u}
   };

   // For a collection of objects, the projection maps each object to its weight.
   const LootDrop& drop = random.weighted_element(loot_table, &LootDrop::weight);
   std::println("  weighted_element: {} (weight {})", drop.name, drop.weight);

   //Random<E> satisfies UniformRandomBitGenerator so we can use 
   //it with all std algorithms, for example, std::shuffle:
   std::shuffle(vec.begin(), vec.end(), random); 
   std::println("\nShuffled vector:");
   std::println("  {}", vec);     

   return 0;
}
