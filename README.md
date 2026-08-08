
# cpp_prngs

When generating random numbers for games - where the goal is fun, speed, and reproducibility rather than cryptographic security - the C and C++ standard facilities are often an awkward fit.

The classic C `srand()` / `rand()` interface is explicitly described by the C++ standard as a [low-quality, non-portable facility with implementation-defined data-race behavior](https://eel.is/c++draft/rand#c.math.rand). The standard leaves its underlying algorithm unspecified (meaning that `rand()` is allowed to return different numbers on different platforms given the same seed!), `RAND_MAX` is permitted to be [as low as 32,767](https://web.archive.org/web/20260410163728/https://www.codingnest.com/generating-random-numbers-using-c-standard-library-the-problems/#fn1), and common attempts to convert its output into a desired range - such as `rand() % n` - [are slow](https://github.com/ulfben/cpp_prngs/#bounded-integers) and can introduce [modulo bias](https://web.archive.org/web/20260410163728/https://www.codingnest.com/generating-random-numbers-using-c-standard-library-the-problems/).

Although C++11 introduced `<random>`, it still presents several practical problems for game developers:

**Seeding is cumbersome.** Correctly supplying engines with suitable seed material is notoriously [easy to get wrong](https://www.pcg-random.org/posts/cpp-seeding-surprises.html), and so inconvenient that it has motivated multiple C++ committee proposals, including [P0205R1](https://www.open-std.org/jtc1/sc22/wg21/docs/papers/2021/p0205r1.html) and [P0347R1](https://www.open-std.org/jtc1/sc22/wg21/docs/papers/2016/p0347r1.html).

**Standard distributions are not cross-library reproducible.** Given the same engine state, distributions such as `std::normal_distribution` are not required to produce identical results across different standard-library implementations. This can break procedural-generation consistency between platforms. See [P2059R0: Make Pseudo-random Numbers Portable](https://www.open-std.org/jtc1/sc22/wg21/docs/papers/2020/p2059r0.pdf).

**There is no compile-time support.** Standard engines and distributions are not constexpr and cannot be evaluated at compile time. Making the deterministic <random> facilities constexpr is still only proposed for C++29 in [P3791R1](https://www.open-std.org/jtc1/sc22/wg21/docs/papers/2026/p3791r1.html).

The most widely used general-purpose engine in the C++ standard library is probably the Mersenne Twister, [`std::mt19937`](https://eel.is/c++draft/rand.predef). It is a respectable generator, but `std::mt19937` requires **624 state words - typically around 2.5 KiB of internal state**. That can put substantial pressure on CPU caches. Choosing it over modern small-state generators such as [xoshiro256**](https://prng.di.unimi.it/), [PCG](https://www.pcg-random.org/paper.html), or [Romu](https://www.romu-random.org/) incurs [a significant performance cost](https://github.com/ulfben/cpp_prngs/#performance-benchmarks).

For a deep, game-focused comparison of 47 PRNGs across nine platforms, see Rhet Butler’s excellent [RNG Battle Royale (2020)](https://web.archive.org/web/20220704174727/https://rhet.dev/wheel/rng-battle-royale-47-prngs-9-consoles/). It highlights the performance, portability, state-size, and statistical-quality concerns that matter in real-world game development. Several of its top-performing generators - including Romu and SmallFast - are included here.

So, if you are making games and need a random-number generator that is:

- small (**16–32 bytes**) and [fast](https://github.com/ulfben/cpp_prngs#performance-benchmarks)
- deterministic across platforms (e.g., *portable!*)
- [easy to seed](https://github.com/ulfben/cpp_prngs#seeding)
- [feature-rich](https://github.com/ulfben/cpp_prngs#randomhpp), with integers, floats, coin flips, Gaussian samples, raw bits, and random range selection
- executable at compile time with `constexpr` and
- [compatible](https://en.cppreference.com/w/cpp/named_req/UniformRandomBitGenerator) with STL algorithms and distributions such as `std::shuffle`, `std::sample`, and `std::*_distribution`

…go ahead and copy any of these engines together with the `random.hpp` interface, and go forth and prosper. Let me know if you find bugs or add any cool new features!

[Try it on Compiler Explorer!](https://compiler-explorer.com/z/YTbGcreEe)

---

## Getting Started

To use a PRNG:

```cpp
#include "romuduojr.hpp" //the engine; pick your favorite from the provided /engines
#include "random.hpp" //the user-friendly wrapper that provides a consistent interface and utilities across all engines

using rnd::Random;

Random<RomuDuoJr> rng{1234}; // generator with fixed seed, powered by the romuduojr engine.
int damage = rng.between(10, 20);   // Random int in [10, 20)
```

Use `Random<E>` to access [convenient utilities](https://github.com/ulfben/cpp_prngs#randomhpp) like bounds, floats, coin flips, Gaussian samples, picking from containers, raw bits, and more.

### Weighted draws

Pass a range of weights to `weighted_index()` when the weights themselves form the lookup table. Each returned index corresponds to the weight at that index:

```cpp
#include <array>

constexpr std::array weights{50u, 30u, 15u, 5u};
const std::size_t tier = rng.weighted_index(weights);
// tier 0 is selected with weight 50, tier 1 with weight 30, and so on.
```

When weights are stored in a collection of objects, pass a projection to `weighted_element()` or `weighted_iterator()`. A pointer to the weight member is often all the projection you need:

```cpp
#include <array>
#include <string_view>

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

const LootDrop& drop = rng.weighted_element(loot_table, &LootDrop::weight);
```

Weights should be non-negative whole numbers. A weight of 0 means the item will never be selected. At least one weight must be greater than zero.

[Try it on Compiler Explorer!](https://compiler-explorer.com/z/YTbGcreEe)

---

## [Engines](https://github.com/ulfben/cpp_prngs/tree/main/includes/engines)
All the provided engines [are very fast](https://github.com/ulfben/cpp_prngs#performance-benchmarks):

They are also compact (16-32 bytes), produce high-quality randomness, and can even run at compile time. I recommend using the 64-bit output versions unless you have a measured performance reason not to. The 32-bit engines work fine, but their output values are smaller than `size_t` on most systems. This means they might not handle indexing very large containers (~4.29 billion elements). Such large containers are rare though and, in debug builds, the `Random<E>` code will alert you if this problem occurs.

| File Name           | Output Width | Description                                                                                                                                |
|---------------------|--------------|--------------------------------------------------------------------------------------------------------------------------------------------|
| [`romuduojr.hpp`](https://github.com/ulfben/cpp_prngs/blob/main/includes/engines/romuduojr.hpp) | 64 bits | C++ port of [Mark Overton’s RomuDuoJr](https://romu-random.org/). Winner of Rhet Butler’s [RNG Battle Royale (2020)](https://web.archive.org/web/20220704174727/https://rhet.dev/wheel/rng-battle-royale-47-prngs-9-consoles/)! |
| [`konadare192.hpp`](https://github.com/ulfben/cpp_prngs/blob/main/includes/engines/konadare192.hpp)         | 64 bits      | C++ port of [Pelle Evensen's konadare192px++](https://github.com/pellevensen/PReenactiNG). Second fastest and second smallest 64-bit PRNG in this lineup!  |
| [`quarkburst64.hpp`](https://github.com/ulfben/cpp_prngs/blob/main/includes/engines/quarkburst64.hpp) | 64 bits | A C++ port of Eightomic’s quarkburst1x64, previously published as GhostScramble64. |
| [`pcg32.hpp`](https://github.com/ulfben/cpp_prngs/blob/main/includes/engines/pcg32.hpp)         | 32 bits      | C++ port of [Melissa O’Neill’s minimal PCG32](https://www.pcg-random.org/download.html#minimal-c-implementation). Wikipedia: [Permuted congruential generator](https://en.wikipedia.org/wiki/Permuted_congruential_generator) |
| [`xoshiro256ss.hpp`](https://github.com/ulfben/cpp_prngs/blob/main/includes/engines/xoshiro256ss.hpp)  | 64 bits      | C++ port of [David Blackman & Sebastiano Vigna's xoshiro256\*\* 1.0](https://prng.di.unimi.it/) generator. Wikipedia: [Xorshift](https://en.wikipedia.org/wiki/Xorshift). |
| [`small_fast32.hpp`](https://github.com/ulfben/cpp_prngs/blob/main/includes/engines/small_fast32.hpp)  | 32 bits      | C++ port of [Bob Jenkins’ 32-bit “Small Fast”](https://burtleburtle.net/bob/rand/smallprng.html) PRNG (two-rotate). |
| [`small_fast64.hpp`](https://github.com/ulfben/cpp_prngs/blob/main/includes/engines/small_fast64.hpp)  | 64 bits      | A 64-bit three-rotate implementation of the above. Three rotates (7, 13, 37) ensure stronger avalanche behavior than a naïve two-rotate 64-bit variant. |

Each included engine is a small, self-contained random number generator. You can use an engine directly, but it deliberately provides only the basics: seeding, advancing its state, comparing states, and generating random unsigned integers.

For everyday use, wrap an engine in [`Random<E>`](https://github.com/ulfben/cpp_prngs/blob/main/includes/random.hpp). `Random<E>` adds the game-friendly interface - bounded numbers, floats, coin flips, random elements, weighted selection, Gaussian samples, and more - while letting you swap the underlying engine without changing the rest of your code.

All included engines satisfy the [`RandomBitEngine`](https://github.com/ulfben/cpp_prngs/blob/main/includes/concepts.hpp) concept and can therefore be used with `Random<E>`. They are also compatible with standard C++ facilities such as `std::shuffle` and `std::sample`.

Want to use your own engine? If it satisfies `RandomBitEngine`, you can plug it into `Random<E>` too.

---

## [random.hpp](https://github.com/ulfben/cpp_prngs/blob/main/includes/random.hpp)

| Method                              | Description                                                                                                                                                         |
| ----------------------------------- | ------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| `Random<E>()`                       | Default-constructs the engine `E` with its default seed                                                                                                             |
| `Random<E>(seed)`                   | Constructs by seeding the engine with `seed`                                                                                                                        |
| `Random<E>(engine)`                 | Constructs by copying an existing engine instance                                                                                                                   |
| `operator==(other)`                 | Returns `true` if two generators have identical state                                                                                                               |
| `min()`                             | Returns the engine’s minimum possible value (typically 0)                                                                                                           |
| `max()`                             | Returns the engine’s maximum possible value                                                                                                                         |
| `next()` / `operator()()`           | Returns the next random number in `[min(), max()]`                                                                                                                  |
| `next(bound)` / `operator()(bound)` | Random integer in `[0, bound)`, using [Lemire’s FastRange](https://lemire.me/blog/2016/06/27/a-fast-alternative-to-the-modulo-reduction/) without rejection (so: minimal bias, very fast) |
| `next<N, T>()`                      | Compile-time bounded integer in `[0, N)`, optionally returned as type `T`; optimized for power-of-2 bounds[^1]                                                      |
| `between(lo, hi)`                   | Random integer or float in `[lo, hi)` (integer if `lo, hi` are integral, else float)                                                                                |
| `bits(n)`							  | Runtime: returns `n` random bits in the low `n` bits of `T` (`1 ≤ n ≤ digits(T)`), drawing from the high bits of one or more engine outputs; `T` defaults to `result_type`[^1] |
| `bits<N, T>()`					  | Returns `N` random bits in the low `N` bits of `T`, drawing from the high bits of one or more engine outputs; constraints are checked at compile time[^1] |
| `bits_as<T>()`                      | Convenience: returns an unsigned `T` filled with high-quality random bits                                                                               |
| `normalized<F>()`                   | Returns float `F` in `[0.0, 1.0)`, using the [Inigo Quilez float hack](https://iquilezles.org/articles/sfrand/)                                                     |
| `signed_norm<F>()`                  | Returns float `F` in `[-1.0, 1.0)`                                                                                                                                  |
| `coin_flip()`                       | Fair coin flip (`true` ~50%)                                                                                                                                        |
| `coin_flip(p)`                      | Weighted coin (`true` with probability `p`, where `p` is in `[0.0, 1.0]`)                                                                                           |
| `index(range)`                      | Returns a random index into any sized range                                                                                                                         |
| `iterator(range)`                   | Returns an iterator to a random element                                                                                                                             |
| `element(range)`                    | Returns a reference to a random element                                                                                                                             |
| `weighted_index(weights)`           | Returns an index selected proportionally to (unsigned) weights; zero-weight indices are excluded                                                            |
| `weighted_iterator(range, projection)` | Returns an iterator selected proportionally to weights returned by `projection(element)`                                               |
| `weighted_element(range, projection)` | Returns a reference selected proportionally to weights returned by `projection(element)`                                                |
| `gaussian(mean, stddev)`            | Approximate normal sample via the Irwin–Hall sum-of-12 method                                                                                                       |
| `discard(n)`                        | Advances the underlying engine by `n` steps                                                                                                                         |
| `seed()`                            | Reseeds the engine back to its default state                                                                                                                        |
| `seed(v)`                           | Reseeds the engine with value `v`                                                                                                                                   |
| `split()`                           | Produces a decorrelated, forked engine (useful for parallel streams)                                                                                                |
| `engine()` / `engine() const`       | Access the underlying engine instance (for manual serialization, debugging, etc.)                                                                                   |

The weighted helpers let you pick items with different chances of being selected.

Weights should be non-negative whole numbers, such as {70u, 25u, 5u}. A weight of 0 means the item will never be selected. At least one weight must be greater than zero.


[^1]: Although `bits(n)` and `bits<N>()` *can* be used for power-of-two integer ranges, this is not their intended purpose. Prefer `next<N,T>()` instead. It chooses the same fast, unbiased bit-shift specialization, but makes your code clearer and safer.

---

## Performance Benchmarks

The [benchmark suite](benchmarks/) uses [Quick Bench](https://quick-bench.com/) to measure three representative workloads:

- [Raw engine throughput](https://quick-bench.com/q/L2igH6P-IVdiwrTdVpuEzkzoH5Q) using `next()`.
- [Bounded integer generation](https://quick-bench.com/q/6hjHn7fpVdaEmp37BcsXyW4A3K4) using `Random<E>::next(bound)`.
- [Bounded floating-point generation](https://quick-bench.com/q/GP9Zfw7YOaXteDOztQ_P2kYnmoY) using `Random<E>::between(lo, hi)`.

The bounded benchmarks exercise the public `Random<E>` interface and compare it with equivalent C and C++ standard-library approaches. Lower bars are faster.

### Engine throughput

Generating raw random numbers using `next()`:

![Engine throughput benchmark](benchmarks/results/engine-comparison-2026-08.png)

All of the included engines substantially outperform the standard-library generators in this benchmark.

### Bounded integers

Generating random integers in `[0, bound)` using `Random<E>::next(bound)`:

![Bounded integer benchmark](benchmarks/results/random-bounded-integer-2026-08.png)

### Bounded floating-point values

Generating random floats in `[lo, hi)` using `Random<E>::between(lo, hi)`:

![Bounded floating-point benchmark](benchmarks/results/random-bounded-float-2026-08.png)


The bounded benchmarks show that the convenience provided by `Random<E>` does not come at the expense of performance. QuarkBurst64, RomuDuoJr, and Konadare192 are currently the fastest engines in these comparisons.

Performance depends on the compiler, standard library, build settings, CPU, and workload. Always benchmark on your own target hardware before choosing an engine.

---

## Seeding 

All engines in this library are seeded from a single `uint64_t` value. They provide a fixed default seed, so default construction (`Random<E>()`) is always valid - but produces the *same* sequence every time.

To get varied sequences, you’ll want to provide a high-entropy seed. `std::random_device` is often used for this - it's typically backed by an operating-system entropy source and [works fine on most platforms](https://codingnest.com/generating-random-numbers-using-c-standard-library-the-problems/). But it can be slow, unavailable (e.g. on embedded systems, or at compile time), and is unsuitable when you need determinism.

In game development, determinism is often useful - for example in procedural generation, tests, or replays. In these cases, consistent seeds let you reproduce the same output across runs and platforms.

An optional standalone [`seeding.hpp`](https://gist.github.com/ulfben/76518f306880bb7a014e35832c555cf6) is available separately from this repository. It is a collection of example seeding techniques for runtime and compile-time contexts, using sources such as timestamps, thread IDs, game assets, player data, and compilation metadata.

```cpp
#include "seeding.hpp" // Download from the linked gist

//Example usage:
using rnd::Random; 

// Compile-time seeding:
constexpr auto seed1 = seed::from_text("my_game_seed");
constexpr auto seed2 = seed::from_source();           // Different for each compilation unit (source file)
constexpr auto seed3 = SEED_UNIQUE_FROM_SOURCE();     // Different for each macro expansion, even within the same source file

// Runtime seeding:
Random<SmallFast32> rng1(seed::from_time());		  // High resolution clock
Random<SmallFast64> rng2(seed::from_system_entropy());// Uses std::random_device (hardware/system entropy)

// Sources of run-to-run variation:
Random<RomuDuoJr> rng3(seed::from_thread());          // Unique per thread
Random<PCG32>     rng4(seed::from_stack());			  // Varies per run of the application, if ASLR is active
Random<PCG32>     rng5(seed::from_cpu_time());		  // Varies with CPU time consumed by the process; can reflect workload or scheduling

// Combine all available sources:
Random<Xoshiro256SS> rng6(seed::from_all());          // Combines all sources (time, thread, stack, heap, compile time, source data, hw entropy, etc.)
```

These utilities help you seed your random number generators appropriately - whether you need compile-time evaluation, reproducibility, run-to-run variation, or unpredictability.

## License

This repository is primarily licensed under the MIT License. See [LICENSE](https://github.com/ulfben/cpp_prngs/blob/main/LICENSE.md) for full details.

### Attributions and Third-Party Code

This project includes, or is based on, the following PRNG engines and reference implementations:

- **QuarkBurst64**: Independent C++ implementation under MIT of William Stafford Parsons’ quarkburst1x64, created for [Eightomic](https://github.com/eightomic/quarkburst) and released under [BSD-3-Clause](https://github.com/eightomic/quarkburst/commit/2f754cf4e18e6ecdfec17c2bda72a1a1aa531db5). Previously published as [GhostScramble](https://web.archive.org/web/20260531035702/https://github.com/williamstaffordparsons/ghostscramble/blob/master/ghostscramble.c).
- **RomuDuoJr**: Based on Rhet Butler’s C++ wrapper ([public domain](https://github.com/Almightygir/rhet_RNG/blob/main/xromu2jr.h)), itself inspired by Mark Overton’s [Romu family](https://romu-random.org/).
- **SmallFast32 / SmallFast64**: Based on Bob Jenkins’ reference implementation ([public domain](https://burtleburtle.net/bob/rand/smallprng.html)).
- **xoshiro256\*\***: Based on David Blackman & Sebastiano Vigna’s reference code ([public domain](https://prng.di.unimi.it/xoshiro256starstar.c)).
- **splitmix64**: By Sebastiano Vigna ([public domain](https://prng.di.unimi.it/splitmix64.c)).
- **PCG32**: Based on M.E. O’Neill’s reference implementation ([Apache License 2.0](https://github.com/imneme/pcg-c-basic/)).
- **konadare192px++**: By Pelle Evensen ([Apache License 2.0](https://github.com/pellevensen/PReenactiNG)).
- **moremur**: By Pelle Evensen ([public domain](https://mostlymangling.blogspot.com/2019/12/stronger-better-morer-moremur-better.html)).
- **xnasam**: By Pelle Evensen ([public domain](https://mostlymangling.blogspot.com/2020/01/nasam-not-another-strange-acronym-mixer.html)).

Where applicable, copyright and license information is included in the header of each source file.

All additional code, wrappers, and modifications © Ulf Benjaminsson, licensed under the MIT License unless otherwise noted.
