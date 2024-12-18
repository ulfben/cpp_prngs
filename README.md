# cpp_prngs
When talking about random number generation (for making games fun, not for making SSH keys...) the C and C++ standard libraries comes up short in many ways. The classical C `srand()/rand()` [is biased, not thread safe, not portable and offers limited range](https://codingnest.com/generating-random-numbers-using-c-standard-library-the-problems/#fn15). The C++ `<random>` library is [inconvenient](https://youtu.be/zUVQhcu32rg?si=G3LHsYagEHhH9UYS&t=234), [easy to use wrong](https://www.pcg-random.org/posts/cpp-seeding-surprises.html), provides [poor guarantuees](https://codingnest.com/generating-random-numbers-using-c-standard-library-the-problems/) and is not available at compile time. 

The best Pseudo Random Number Generator (PRNG) offered by the C++ standard library is likely the Mersenne Twister, but opting for `std::mt19937` instead of more efficient alternatives like [Xorshift](https://en.wikipedia.org/wiki/Xorshift) or a [PCG](https://en.wikipedia.org/wiki/Permuted_congruential_generator) variant sacrifices [a significant amount of performance](https://quuxplusone.github.io/blog/2021/11/23/xoshiro/). 

Hence this repo! If you're making games and need your random number generator to be: 
- fast and small
- portable
- easy to seed
- executable at compile time
- feature-rich (ints, floats, coin flip, ranges, save and restore state, etc)
- compatible with `<algorithm>` (`std::shuffle`, `std::sample`, `std::*_distribution`, etc)

... just go ahead and copy-paste any one of these and go forth and prosper. Let me know if you find bugs or add any cool new features!

## SmallFast_32.h
My public domain port of [Jenkins' smallfast 32-bit 2-rotate prng](https://burtleburtle.net/bob/rand/smallprng.html), including a handy interface;

* `between(min, max)` -> [min, max] (inclusive)
* `normalized()` - [0.0, 1.0)
* `coinToss()` -> true or false
* `unit_range()` -> [-1.0 - 1.0)
* `next()` -> [0, `std::numeric_limits<u32>::max()`]
* `next(u32 bound)` -> [0, bound)
* `next_2(u16 bound)` -> [0, bound), [0, bound)
* `next_gaussian(mean, deviation)` -> random number following a normal distribution centered around the mean
* `get_state()` -> std::array of the rng state for saving
* `set_state(span<u32>)` : restore the state of the rng

Additionally satisfies [UniformRandomBitGenerator](https://en.cppreference.com/w/cpp/named_req/UniformRandomBitGenerator), meaning that it supports `std::shuffle`, `std::sample`, most of the `std::*_distribution`-classes, etc.

The entire file is about 100 lines of relatively simple code, executable at compile time, and optionally templated to support various numeric types. `std::array` is perhaps an unnecessarily large inclusion and is only used for `get_state()`. If you don't need to save and reload the state, you can easily remove it. :)

[Try SmallFast_32 over at compiler explorer](https://godbolt.org/z/d5G53dvaE).

## SmallFast_64.h
SmallFast_64.h is a 64-bit three-rotate implementation of the above. You can implement the 64-bit version as a 2-rotate too (using 39 and 11 as your rotation constants), but Jenkins recommends against it due to the lower avalanche achieved. Therefore, I use three rotates for 64-bit and two rotates for 32-bit. 

Provides similar interface as SmallFast32 but with larger range, plus additional bulk generation: 
* `next(u64 bound)` -> [0, bound)
* `next_2(u32 bound)` -> 2 x [0, bound)
* `next_4(u16 bound)` -> 4 x [0, bound)

[Try SmallFast_64 over at compiler explorer](https://godbolt.org/z/1o8EGo6Wv).

## PCG32.h
A constexpr variant of [Melissa O'Neill's minimal PCG](https://www.pcg-random.org/download.html#minimal-c-implementation) (Permuted Congruential Generator). Very small, very fast. Satisfies [UniformRandomBitGenerator](https://en.cppreference.com/w/cpp/named_req/UniformRandomBitGenerator) and offers the following interface:

| Method | Description |
|--------|-------------|
| `PCG32()` | Default constructor - initializes with default seed and default stream |
| `PCG32(seed, sequence = 1)` | Initialize with specific seed and optional sequence ID. The sequence parameter selects different streams of random numbers - different sequences are guaranteed to not overlap even with the same seed |
| `next()` | Returns random number in range [0, 2³²) |
| `next(bound)` | Returns random number in range [0, bound) |
| `normalized()` | Returns random float in range [0.0f, 1.0f) |
| `between(min, max)` | Returns random float in range [min, max) |
| `advance(delta)` | Advance internal state by `delta` steps, with O(log n) complexity |
| `backstep(delta)` | Reverse internal state by `delta` steps, with O(log n) complexity |
| `seed(seed, sequence = 1)` | Reset generator with new seed and optional sequence |
| `get_state()` | Returns current internal state as `std::pair<uint64_t, uint64_t>` |
| `set_state(state, sequence)` | Sets internal state directly |
| `from_state(state, sequence)` | Creates new generator from saved state |

[Try PCG32 over at compiler explorer](https://compiler-explorer.com/z/PrnP4h5Mf)

## std_random.hpp 
If you want the best the standard library has to offer, but with a more useful interface, check out std_random.hpp. 
It's based on `std::mt19937`, and will by default seed the full 2,496 byte state of using std::random_device. It can also be manually seeded for reproducability. 
The interface provides:

* T getNumber<T>(min, max);  //ranges are inclusive for integrals, half-open for floats
* char color();              // [0,255]
* bool coinToss();           // true/false
* float normalized();        // [0.0,1.0)
* float unit_range();        // [-1.0,1.0)

[Try std_random.hpp over at Compiler Explorer](https://compiler-explorer.com/z/fKz443bG4).

## xoshiro256ss.h
The "xoshiro256** 1.0" generator. Ignore this for now, use one of the above instead. I'm not happy with the interface I wrote for this.
Public interface, rejection sampling and seeding utilities (see; [seeding.h](https://github.com/ulfben/cpp_prngs/blob/main/seeding.h)) by Ulf Benjaminsson (2023). 
Based on [C++ port by Arthur O'Dwyer (2021)](https://quuxplusone.github.io/blog/2021/11/23/xoshiro/), of [the C original by David Blackman and Sebastiano Vigna (2018)](https://prng.di.unimi.it/xoshiro256starstar.c).
[splitmix64 by Sebastiano Vigna (2015)](https://prng.di.unimi.it/splitmix64.c) 

* `u64 max()` -> [maximum possible value for u64]
* `u64 next()` -> [random unsigned number]
* `bool coinToss()` -> [true or false]
* `Real normalized()` -> [0.0, 1.0)
* `Real inRange(Real range)` -> [0.0, range)
* `Real inRange(Real from, Real to)` -> [0.0, to)
* `Int inRange(Int from, Int to)` -> Int [from, to)
* `Int inRange(Int range) noexcept` -> Int [0, range or -range]
* `u64 inRange(u64 range) noexcept` -> [0u, range)
* `Span state() const` -> Span [current state]
* `void jump() noexcept` -> void [advances the state by 2^128 steps]

The `jump()` function is equivalent to 2^128 calls to `next()`. It can be used to generate 2^128 non-overlapping subsequences for parallel computations.

## seeding.h
This file demonstrates a few ideas for sourcing entropy in C++. `std::random_device` is typically hardware-based, high-quality entropy and works fine [on most platforms and configurations](https://codingnest.com/generating-random-numbers-using-c-standard-library-the-problems/). However, there are times when you might need other sources of entropy—perhaps for speed reasons, to generate seeds at compile time, or when targeting portable devices without hardware / kernel entropy evailable. Use seeding.h for inspiration. :)

## string_hash.h
A constexpr implementation of the [FNV-1a (Fowler-Noll-Vo) hash algorithm](https://en.wikipedia.org/wiki/Fowler%E2%80%93Noll%E2%80%93Vo_hash_function) with a convenient string literal operator. While not a PRNG itself, fast string hashing is useful for:
- Replacing slow string comparisons with fast integer comparisons
- Generating compile-time seeds for PRNGs from string literals (like `auto seed = "my game seed"_fnv;`, see more below)
- Creating deterministic hashes for game content when you need reproducible numbers

The implementation is about 30 lines of code and provides:
* Explicit constructor: `string_hash("some text")`  
* String literal operator: `"some text"_hash`
* Full comparison operators for use as key in STL containers

You can play around with macro expansions to generate deterministic but unique seeds at compile time:
```
// File/line information
constexpr auto file_seed = __FILE__"_hash;                  // full path of current source file
constexpr auto line_seed = std::string{__FILE__}.append(__LINE__, '_')_hash; // file+line number

// Date/time of compilation
constexpr auto time_seed = __DATE__ " " __TIME__"_hash;     // "Mar 18 2024 15:30:45"

// Function information (compiler-dependent)
constexpr auto func_seed = __FUNCTION__"_hash;              // current function name
constexpr auto pretty_func = __PRETTY_FUNCTION__"_hash;     // detailed function info including namespace/templates
```
The most commonly useful ones for game dev would probably be:

__FILE__ + __LINE__ for per-location seeds
__DATE__ + __TIME__ for per-compilation seeds
__FUNCTION__ for function-specific seeds
