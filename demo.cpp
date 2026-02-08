#include "engines/pcg32.hpp" // PCG32 engine
#include "engines/romuduojr.hpp" // RomuDuoJr engine 
#include "random.hpp" // the Random interface, which wraps any engine to provide a rich set of random generation features
#include "seeding.hpp" // optional: seeding utilities, provides entropy from multiple sources, including at compile time. 
#include <algorithm>
#include <cassert>
#include <cstddef>
#include <print>
#include <string_view>
#include <vector>

// Source: https://github.com/ulfben/cpp_prngs/
// Note: 
// - all features of Random<E> are available for any engine that meets the RandomBitEngine concept (see: concepts.hpp)
// - Several RandomBitEngines are provided: RomuDuoJr, Konadare192, SmallFast64, xoshiro256**, SmallFast32 and PCG32. Benchmark on your platform and pick the one that's fastest.
// - all features of RandomBitEngine and Random<E> are available for both compile-time and runtime use
// - seeding.hpp is optional and brings in a lot of headers. It is intended to demonstrate several seeding approaches. Pick the seeding strategy that best suits your needs and throw the rest away if you want a more minimal setup.
// 
// Demo is available on Compiler Explorer: https://compiler-explorer.com/z/67ffKPv3G
// Benchmarks:
   // Quick Bench for generating raw random values: https://quick-bench.com/q/vWdKKNz7kEyf6kQSNnUEFOX_4DI
   // Quick Bench for generating normalized floats: https://quick-bench.com/q/GARc3WSfZu4sdVeCAMSWWPMQwSE
   // Quick Bench for generating bounded values: https://quick-bench.com/q/WHEcW9iSV7I8qB_4eb1KWOvNZU0

int main(){
	using namespace rnd;
	constexpr std::string_view str{"abcdefghijklmnopqrstuvwxyz"};
	std::vector<int> vec{1,2,3,4,5,6,7,8,9,10};

	Random<RomuDuoJr> random{seed::from_text(str)}; // create a Random<RomuDuoJr> seeded from text data (compile-time or runtime). See seeding.hpp for more options and details on seeding strategies.
	std::println("Random<RomuDuoJr>:");
	std::println("  next() [{}, {}]: {}", random.min(), random.max(), random.next());    // raw engine output: [min, max] inclusive. Same as 'random()'
	std::println("  next(100) [0, 100): {}\n", random.next(100));                        // half-open: [0, 100). Same as 'random(100)'

	std::println("  between [10, 20): {}", random.between(10, 20));                      // half-open
	std::println("  between [5.0f, 10.0f): {}\n", random.between(5.0f, 10.0f));          // half-open

	std::println("  normalized [0.0f, 1.0f): {}", random.normalized());                  // float by default (normalized<double>() for double)
	std::println("  signed_norm [-1.0f, 1.0f): {}\n", random.signed_norm());             // float by default (signed_norm<double>() for double)

	std::println("  coin_flip(): {}", random.coin_flip());                               // fair coin
	std::println("  coin_flip(0.9f): {}\n", random.coin_flip(0.9f));                     // ~90% true (weighted coin)

	std::println("  bits_as<uint8_t>(): {:08b}b", random.bits_as<std::uint8_t>());       // fill an 8-bit value with random bits
	std::println("  bits<24, uint32_t>(): #{:06x}", random.bits<24, std::uint32_t>());   // 24 random bits (0xRRGGBB)
	std::println("  bits<uint64_t>(7): {:07b}b\n", random.bits<std::uint64_t>(7));       // runtime: 55 random bits in low bits

	std::println("  gaussian(0.0, 1.0) sample: {}\n", random.gaussian(0.0, 1.0));

	std::println("  element(str): {}", random.element(str));                           // random element from a sized forward range
	size_t i = random.index(str);                                                      // random index in [0, size)
	std::println("  index(str): {} ({})", i, str[i]);

	//Random<E> satisfies UniformRandomBitGenerator so we can use 
	//it with all std algorithms, for example, std::shuffle:
	std::shuffle(vec.begin(), vec.end(), random);
	std::println("\nShuffled vector:");
	std::println("  {}", vec);

	return 0;
}