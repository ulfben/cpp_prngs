# cpp_prngs

When talking about random number generation — for making games fun, not for making SSH keys — the C and C++ standard libraries come up short in many ways. The classic C `srand()/rand()` [is biased, not thread-safe, not portable, and offers limited range](https://codingnest.com/generating-random-numbers-using-c-standard-library-the-problems/#fn15). The C++ `<random>` library is [inconvenient](https://youtu.be/zUVQhcu32rg?si=G3LHsYagEHhH9UYS&t=234), [easy to misuse](https://www.pcg-random.org/posts/cpp-seeding-surprises.html), provides [poor guarantees](https://codingnest.com/generating-random-numbers-using-c-standard-library-the-problems/), and is not available at compile time.

The best pseudo-random number generator (PRNG) offered by the C++ standard library is likely the Mersenne Twister. But choosing `std::mt19937` over more efficient alternatives like [Xorshift](https://en.wikipedia.org/wiki/Xorshift) or a [PCG](https://en.wikipedia.org/wiki/Permuted_congruential_generator) variant sacrifices [a significant amount of performance](https://quuxplusone.github.io/blog/2021/11/23/xoshiro/).

Hence this repo! If you're making games and need your random number generator to be:

- small and *fast*
- deterministic across platforms (e.g., *portable!*)
- easy to seed
- executable at compile time
- feature-rich (ints, floats, coin flip, ranges, pick-from-collection, etc.)
- [compatible](https://en.cppreference.com/w/cpp/named_req/UniformRandomBitGenerator) with all of your favorite STL algorithms and distributions (`std::shuffle`, `std::sample`, `std::*_distribution`, etc.)

...just go ahead and copy-paste one of these engines and the `random.hpp` interface, and go forth and prosper. Let me know if you find bugs or add any cool new features!

---

## Getting Started

To use a PRNG:

```cpp
#include "small_fast32.hpp"
#include "random.hpp"

rnd::Random<SmallFast32> rng{1234}; // Seeded generator
int damage = rng.between(10, 20);   // Random int in [10, 20)
````

Use `Random<E>` to access convenient utilities like floats, coin flips, Gaussian samples, color packing, and compatibility with STL algorithms.

[Try it on Compiler Explorer!](https://compiler-explorer.com/z/aevcre8nj)

Want to use your own engine? Just make sure it satisfies `std::uniform_random_bit_generator`.

---

## Engines

All of these engines are very fast, produce high-quality entropy, and can execute at compile time. Each engine satisfies the C++20 [UniformRandomBitGenerator](https://en.cppreference.com/w/cpp/named_req/UniformRandomBitGenerator) concept, which is a minimal interface providing little more than *"give me some bits, please."*

* **`small_fast32.hpp`**
  My public-domain C++ port of [Bob Jenkins’ 32-bit “Small Fast”](https://burtleburtle.net/bob/rand/smallprng.html) PRNG (two-rotate).

* **`small_fast64.hpp`**
  A 64-bit three-rotate implementation of the above. Three rotates (7, 13, 37) ensure stronger avalanche behavior than a naïve two-rotate 64-bit variant.

* **`pcg32.hpp`**
  A fully `constexpr` C++ port of [Melissa O’Neill’s minimal PCG32](https://www.pcg-random.org/download.html#minimal-c-implementation). Wikipedia: [Permuted congruential generator](https://en.wikipedia.org/wiki/Permuted_congruential_generator)

* **`xoshiro256ss.hpp`**
  The [xoshiro256\*\* 1.0](https://prng.di.unimi.it/xoshiro256starstar.c) generator by David Blackman & Sebastiano Vigna (2018). Wikipedia: [Xorshift](https://en.wikipedia.org/wiki/Xorshift)

The engines are kept minimal so they can be used interchangeably with the top-level `Random<E>` template (in `random.hpp`). `Random<E>` wraps any engine — including your own — with a consistent and convenient interface tailored to game development.

---

## Random.hpp

| Method                              | Description                                                                                  |
| ----------------------------------- | -------------------------------------------------------------------------------------------- |
| `Random<E>()`                       | Default-constructs the engine E with its default seed                                        |
| `Random<E>(seed)`                   | Constructs by seeding the engine with `seed`                                                 |
| `Random<E>(engine)`                 | Constructs by taking ownership of an existing engine instance                                |
| `operator==(other)`                 | Returns `true` if two generators have identical state                                        |
| `min()`                             | Returns the engine’s minimum possible value (typically 0)                                    |
| `max()`                             | Returns the engine’s maximum possible value                                                  |
| `next()` / `operator()()`           | Returns next random number in range `[min(), max()]`                                         |
| `next(bound)` / `operator()(bound)` | Returns next random number in range `[0, bound)`                                             |
| `between(lo, hi)`                   | Returns random integer or float in `[lo, hi)` (integer if `lo, hi` are integral, else float) |
| `normalized<F>()`                   | Returns random float in `[0.0, 1.0)`                                                         |
| `signed_norm<F>()`                  | Returns random float in `[-1.0, 1.0)`                                                        |
| `coin_flip()`                       | Returns `bool` that’s `true` \~50% of the time                                               |
| `coin_flip(p)`                      | Returns `true` with probability `p` (where `p` is a float in `[0.0, 1.0]`)                   |
| `rgb8()`                            | Packs three 8-bit channels into `0xRRGGBB`                                                   |
| `rgba8()`                           | Packs four 8-bit channels into `0xRRGGBBAA`                                                  |
| `index(range)`                      | Returns a random index into any sized range                                                  |
| `iterator(range)`                   | Returns an iterator to a random element in any sized range                                   |
| `element(range)`                    | Returns a reference to a random element in any sized range                                   |
| `gaussian(mean, stddev)`            | Returns an approximate normal (mean, stddev) sample via CLT‐based sum-of-12                  |
| `discard(n)`                        | Advances the underlying engine by `n` steps (fast for engines that support it)               |
| `seed()`                            | Reseeds the engine back to its default state                                                 |
| `seed(v)`                           | Reseeds the engine with value `v`                                                            |
| `engine()` / `engine() const`       | Access the underlying engine instance (for low-level use or manual serialization)            |

---

## std\_random.hpp

If you want to see the best the standard library has to offer — but with a more useful interface — check out `std_random.hpp`. It's based on `std::mt19937`, and will by default seed the full 2,496-byte state of that beast using `std::random_device`. It can also be manually seeded for reproducibility.

The interface provides:

* `T getNumber<T>(min, max);`  // Inclusive for integrals, half-open for floats
* `char color();`              // \[0, 255]
* `bool coinToss();`           // true/false
* `float normalized();`        // \[0.0, 1.0)
* `float unit_range();`        // \[-1.0, 1.0)

[Try std\_random.hpp over at Compiler Explorer](https://compiler-explorer.com/z/fKz443bG4).

---

## seeding.h

This file demonstrates a few ideas for sourcing entropy in C++. `std::random_device` is typically hardware-based, high-quality entropy and works fine [on most platforms and configurations](https://codingnest.com/generating-random-numbers-using-c-standard-library-the-problems/). However, there are times when you might need other sources of entropy — perhaps for speed, compile-time generation, or portable devices without hardware/kernel entropy available. Use `seeding.h` for inspiration. :)

---

## string\_hash.h

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