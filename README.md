# cpp_prngs

When talking about random number generation - for making games fun, not for making SSH keys - the C and C++ standard libraries come up short in many ways. The classic C `srand()/rand()` [is biased, not thread-safe, not portable, and offers limited range](https://codingnest.com/generating-random-numbers-using-c-standard-library-the-problems/#fn15). The C++ `<random>` library is [inconvenient](https://youtu.be/zUVQhcu32rg?si=G3LHsYagEHhH9UYS&t=234), [easy to misuse](https://www.pcg-random.org/posts/cpp-seeding-surprises.html), provides [poor guarantees](https://codingnest.com/generating-random-numbers-using-c-standard-library-the-problems/), and is not available at compile time.

The best pseudo-random number generator (PRNG) offered by the C++ standard library is likely the Mersenne Twister. But choosing `std::mt19937` over more efficient alternatives like [Xorshift](https://en.wikipedia.org/wiki/Xorshift) or a [PCG](https://en.wikipedia.org/wiki/Permuted_congruential_generator) variant sacrifices [a significant amount of performance](https://quuxplusone.github.io/blog/2021/11/23/xoshiro/) in *addition* to the problems listed above.

For a deep, game-focused comparison of 47 PRNGs across 9 platforms, see Rhet Butler’s excellent [RNG Battle Royale (2020)](https://web.archive.org/web/20220704174727/https://rhet.dev/wheel/rng-battle-royale-47-prngs-9-consoles/), which highlights the performance, portability, and statistical quality concerns that matter most in real-world game development. Several of the top-performing generators featured there - including Romu and SmallFast - are included here.

And so; if you're making games and need your random number generator to be:

- small (16 or 32 bytes) and [*fast*](https://github.com/ulfben/cpp_prngs#performance-benchmarks)
- deterministic across platforms (e.g., *portable!*)
- [easy to seed](https://github.com/ulfben/cpp_prngs#seedinghpp)
- [feature-rich](https://github.com/ulfben/cpp_prngs#randomhpp) (ints, floats, coin flip, ranges, pick-from-collection, etc.)
- executable at compile time
- [compatible](https://en.cppreference.com/w/cpp/named_req/UniformRandomBitGenerator) with all of your favorite STL algorithms and distributions (`std::shuffle`, `std::sample`, `std::*_distribution`, etc.)

... go ahead and copy-paste any of these engines and the `random.hpp` interface, and go forth and prosper. Let me know if you find bugs or add any cool new features!

---

## Getting Started

To use a PRNG:

```cpp
#include "small_fast64.hpp"
#include "random.hpp"

rnd::Random<SmallFast64> rng{1234}; // Seeded generator, powered by the SmallFast64 engine.
int damage = rng.between(10, 20);   // Random int in [10, 20)
````

Use `Random<E>` to access [convenient utilities](https://github.com/ulfben/cpp_prngs#randomhpp) like floats, coin flips, Gaussian samples, picking from containers, color packing, and more.

[Try it on Compiler Explorer!](https://compiler-explorer.com/z/Tj1Gscs5P)

Want to use your own engine? It only needs to satisfy the `RandomBitEngine` concept ([concepts.hpp](https://github.com/ulfben/cpp_prngs/blob/main/concepts.hpp)).

---

## [Engines](https://github.com/ulfben/cpp_prngs/tree/main/engines)
All the provided engines [are very fast](https://github.com/ulfben/cpp_prngs#performance-benchmarks):

They are also compact (16 or 32 bytes), produce high-quality randomness, and can even run at compile time. I recommend using the 64-bit output versions unless you have a measured performance reason not to. The 32-bit engines work fine, but their output values are smaller than `size_t` on most systems. This means they might not handle indexing very large containers (over about 4.2 million elements). Such large containers are rare through and, in debug builds, the `Random<E>` code will alert you if this problem occurs.

| File Name           | Output Width | Description                                                                                                                                |
|---------------------|--------------|--------------------------------------------------------------------------------------------------------------------------------------------|
| [`romuduojr.hpp`](https://github.com/ulfben/cpp_prngs/blob/main/engines/romuduojr.hpp) | 64 bits | C++ port of [Mark Overton’s RomuDuoJr](https://romu-random.org/). Winner of Rhet Butler’s [RNG Battle Royale (2020)](https://web.archive.org/web/20220704174727/https://rhet.dev/wheel/rng-battle-royale-47-prngs-9-consoles/)! |
| [`pcg32.hpp`](https://github.com/ulfben/cpp_prngs/blob/main/engines/pcg32.hpp)         | 32 bits      | C++ port of [Melissa O’Neill’s minimal PCG32](https://www.pcg-random.org/download.html#minimal-c-implementation). Wikipedia: [Permuted congruential generator](https://en.wikipedia.org/wiki/Permuted_congruential_generator) |
| [`xoshiro256ss.hpp`](https://github.com/ulfben/cpp_prngs/blob/main/engines/xoshiro256ss.h)  | 64 bits      | C++ port of [David Blackman & Sebastiano Vigna's xoshiro256** 1.0](https://prng.di.unimi.it/) generator. Wikipedia: [Xorshift](https://en.wikipedia.org/wiki/Xorshift). |
| [`small_fast32.hpp`](https://github.com/ulfben/cpp_prngs/blob/main/engines/small_fast32.hpp)  | 32 bits      | C++ port of [Bob Jenkins’ 32-bit “Small Fast”](https://burtleburtle.net/bob/rand/smallprng.html) PRNG (two-rotate). |
| [`small_fast64.hpp`](https://github.com/ulfben/cpp_prngs/blob/main/engines/small_fast64.hpp)  | 64 bits      | A 64-bit three-rotate implementation of the above. Three rotates (7, 13, 37) ensure stronger avalanche behavior than a naïve two-rotate 64-bit variant. |

Each engine satisfies the [`RandomBitEngine`](https://github.com/ulfben/cpp_prngs/blob/main/concepts.hpp) concept, which extends the C++20 [UniformRandomBitGenerator](https://en.cppreference.com/w/cpp/named_req/UniformRandomBitGenerator) to ensure compatibility with the STL. You can use the engines directly, but `RandomBitEngine` provides only a minimal set of features: seeding, advancing, reporting `min()` and `max()`, comparison (of state) and generating random unsigned integers.

The engines are kept simple so they can be swapped easily with the [`Random<E>`](https://github.com/ulfben/cpp_prngs/blob/main/random.hpp) template. `Random<E>` wraps any engine - including your own - to provide a consistent, user-friendly interface designed for game development. See the full interface below.

---

## [random.hpp](https://github.com/ulfben/cpp_prngs/blob/main/random.hpp)

| Method                              | Description                                                                                  |
| ----------------------------------- | -------------------------------------------------------------------------------------------- |
| `Random<E>()`                       | Default-constructs the engine E with its default seed                                        |
| `Random<E>(seed)`                   | Constructs by seeding the engine with `seed`                                                 |
| `Random<E>(engine)`                 | Constructs by copying an existing engine instance                                            |
| `operator==(other)`                 | Returns `true` if two generators have identical state                                        |
| `min()`                             | Returns the engine’s minimum possible value (typically 0)                                    |
| `max()`                             | Returns the engine’s maximum possible value                                                  |
| `next()` / `operator()()`           | Returns next random number in range `[min(), max())`                                         |
| `next(bound)` / `operator()(bound)` | Next random number in `[0, bound)`, using [Lemire's FastRange method](https://lemire.me/blog/2016/06/27/a-fast-alternative-to-the-modulo-reduction/) (minimal bias, very fast) |
| `between(lo, hi)`                   | Random integer or float in `[lo, hi)` (integer if `lo, hi` are integral, else float) |
| `bits(n)`            | Returns `n` random bits at runtime (`1 ≤ n ≤ 64`), filled into the low bits.[^1]  |
| `bits<n>()`          | Returns `n` random bits at compile time (`1 ≤ n ≤ 64`), filled into the low bits.[^1]  |
| `normalized<F>()`                   | Float of type `F` in `[0.0, 1.0)`, using [Inigo Quilez float hack](https://iquilezles.org/articles/sfrand/) |
| `signed_norm<F>()`                  | Float of type `F` in `[-1.0, 1.0)` (also using the IQ hack)                   |
| `coin_flip()`                       | Fair coin, returns `true` ~50% of the time                                                  |
| `coin_flip(p)`                      | Weighted coin, returns `true` with probability `p` (where `p` is `[0.0, 1.0]`)                   |
| `rgb8()`                            | Packs three 8-bit channels into a `uint32_t` as `0xRRGGBB`                                                   |
| `rgba8()`                           | Packs four 8-bit channels into a `uint32_t` as `0xRRGGBBAA`                                                  |
| `index(range)`                      | Returns a random index into any sized collection                                             |
| `iterator(range)`                   | Returns an iterator to a random element in any sized collection                              |
| `element(range)`                    | Returns a reference to a random element in any sized collection                              |
| `gaussian(mean, stddev)`            | Returns an approximate normal sample, using the [Irwin–Hall sum-of-12](https://en.wikipedia.org/wiki/Irwin%E2%80%93Hall_distribution#Approximating_a_Normal_distribution) method                 |
| `discard(n)`                        | Advances the underlying engine by `n` steps                                                  |
| `seed()`                            | Reseeds the engine back to its default state                                                 |
| `seed(v)`                           | Reseeds the engine with value `v`                                                            |
| `engine()` / `engine() const`       | Access the underlying engine instance (for manual serialization)                             |

[^1]: `bits(n)` / `bits<n>()` are not *intended* for integer generation - you should generally use `next()`, `next(bound)` or `between(lo, hi)` instead. *However*, if your bound is a power-of-2, and *especially* if it is known at compile time, `bits` *is* an **optimal** choice (fast and unbiased). :)

---

### Performance Benchmarks

![quickbench_cpp_prngs](https://github.com/user-attachments/assets/afe1d89d-a42b-4383-9764-8efa2bf069c8)

These benchmarks use [QuickBench](https://quick-bench.com/) to let you compare performance across different use cases:

- [`next()` – raw unsigned output](https://quick-bench.com/q/placeholder-next): baseline performance
- [`next(bound)` – bounded integer using Lemire’s method](https://quick-bench.com/q/placeholder-bounded): efficient rejection-free range generation
- [`normalized<float>()` – floating-point in \[0.0, 1.0)](https://quick-bench.com/q/placeholder-float): branchless float generation (IQ trick)

Each benchmark loops over one million values and compares multiple engines side by side, including the std library and cstdlib alternatives.

---

## [`seeding.hpp`](https://github.com/ulfben/cpp_prngs/blob/main/seeding.hpp)

**TL;DR**: `seeding.hpp` is a grab bag of portable, high-entropy seeding strategies - for tools, tests, or procedural generation, at runtime or compile time.

All engines in this library are seeded from a single `uint64_t` or `uint32_t` value. They provide a hardcoded default seed, so default construction (`Random<E>()`) is always valid - but produces the *same* sequence every time.

To get varied sequences, you’ll want to provide a high-entropy seed. `std::random_device` is often used for this - it's typically hardware-backed and [works fine on most platforms](https://codingnest.com/generating-random-numbers-using-c-standard-library-the-problems/). But it can be slow, unavailable (e.g. on embedded systems, or at compile time), and is unsuitable when you need determinism.

In game development determinism is often needed. Procedural systems such as level generation, loot tables, or AI behavior, all benefit from consistent seeds that allow us to reproduce the same output across runs and platforms. This is where `seeding.hpp` shines! It provides a variety of seeding techniques that work in both runtime and compile-time contexts, letting you draw entropy from sources like timestamps, thread IDs, game assets, player data, and even compilation metadata.

```cpp
//Example usage:
using rnd::Random; 
using seed::to_32; // Convenience alias for converting a 64-bit seed to 32 bits (for smaller engines).

// Compile-time seeding:
constexpr auto seed1 = seed::from_text("my_game_seed");
constexpr auto seed2 = seed::from_source();           // Different for each compilation unit (source file)
constexpr auto seed3 = SEED_UNIQUE_FROM_SOURCE();     // Different for each macro expansion, even within the same source file

// Runtime seeding:
Random<SmallFast32> rng1(to_32(seed::from_time()));   // Wall clock time, folded down to 32 bits for SmallFast32
Random<SmallFast64> rng2(seed::from_system_entropy());// Uses std::random_device (hardware/system entropy)

// Pseudo-entropy sources:
Random<RomuDuoJr> rng3(seed::from_thread());          // Unique per thread
Random<PCG32>     rng4(to_32(seed::from_stack()));    // Varies per run of the application, if ASLR is active
Random<PCG32>     rng5(to_32(seed::from_cpu_time())); // Varies with CPU time consumed by the process; can reflect workload or scheduling

// Maximum entropy:
Random<Xoshiro256SS> rng6(seed::from_all());          // Combines all sources (time, thread, stack, entropy, etc.)
```
These utilities help you ensure that your random number generators are seeded appropriately - whether you need reproducibility, speed, or maximal entropy.

---

## [string\_hash.h](https://github.com/ulfben/cpp_prngs/blob/main/string_hash.hpp)

A `constexpr` implementation of the [FNV-1a (Fowler–Noll–Vo) hash algorithm](https://en.wikipedia.org/wiki/Fowler%E2%80%93Noll%E2%80%93Vo_hash_function), with a convenient string literal operator. While not a PRNG itself, fast string hashing is useful for:

* Replacing slow string comparisons with fast integer comparisons
* Generating compile-time seeds for PRNGs from string literals (e.g. `auto seed = "my game seed"_fnv;`)
* Creating deterministic hashes for game content when you need reproducible numbers

The implementation is \~30 lines of code and provides:

* Explicit constructor: `string_hash("some text")`
* String literal operator: `"some text"_hash`
* Full comparison operators for use as keys in STL containers

You can also generate deterministic but unique seeds at compile time using macro expansions:

```cpp
// File/line information
constexpr auto file_seed = __FILE__"_hash";                   // full path of current source file
constexpr auto line_seed = std::string{__FILE__}.append(__LINE__, '_')_hash; // file+line number

// Date/time of compilation
constexpr auto time_seed = __DATE__ " " __TIME__"_hash";      // "Mar 18 2024 15:30:45"

// Function information (compiler-dependent)
constexpr auto func_seed = __FUNCTION__"_hash";               // current function name
constexpr auto pretty_func = __PRETTY_FUNCTION__"_hash";      // detailed function info including namespace/templates
```

Commonly useful for game development:

* `__FILE__ + __LINE__` → per-location seeds
* `__DATE__ + __TIME__` → per-compilation seeds
* `__FUNCTION__`        → function-specific seeds

---

## [std\_random.hpp](https://github.com/ulfben/cpp_prngs/blob/main/std_random.hpp)

A demonstration of how to use the standard library's random facilities. `std_random.hpp` it based on `std::mt19937`, and will by default seed the full 2,496-byte state of that beast using `std::random_device`. It can also be manually seeded for reproducibility. I do not recommend using this code for all the reasons previously stated.

The interface provides:

* `T getNumber<T>(min, max);`  // Inclusive for integrals, half-open for floats
* `char color();`              // \[0, 255]
* `bool coinToss();`           // true/false
* `float normalized();`        // \[0.0, 1.0)
* `float unit_range();`        // \[-1.0, 1.0)

[Try std\_random.hpp over at Compiler Explorer](https://compiler-explorer.com/z/fKz443bG4).

---

## License

This repository is primarily licensed under the MIT License. See [LICENSE](https://github.com/ulfben/cpp_prngs/blob/main/LICENSE.md) for full details.

### Attributions and Third-Party Code

This project includes, or is based on, the following PRNG engines and reference implementations:

- **RomuDuoJr**: Based on Rhet Butler’s C++ wrapper ([public domain](https://github.com/Almightygir/rhet_RNG/blob/main/xromu2jr.h)), itself inspired by Mark Overton’s [Romu family](https://romu-random.org/).
- **SmallFast32 / SmallFast64**: Based on Bob Jenkins’ reference implementation ([public domain](https://burtleburtle.net/bob/rand/smallprng.html)).
- **xoshiro256\*\***: Based on David Blackman & Sebastiano Vigna’s reference code ([public domain](https://prng.di.unimi.it/xoshiro256starstar.c)).
- **splitmix64**: By Sebastiano Vigna ([public domain](https://prng.di.unimi.it/splitmix64.c)).
- **PCG32**: Based on M.E. O’Neill’s reference implementation ([Apache License 2.0](https://github.com/imneme/pcg-c-basic/)).
- **moremur**: By Pelle Evensen ([public domain](https://mostlymangling.blogspot.com/2019/12/stronger-better-morer-moremur-better.html)).

Where applicable, copyright and license information is included in the header of each source file.

All additional code, wrappers, and modifications © Ulf Benjaminsson, licensed under the MIT License unless otherwise noted.
