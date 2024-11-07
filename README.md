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

*  `next()` - [0, std::numeric_limits<uint32_t>::max()]
*  `next(bound)` - [0, bound)
*  `normalized()` - [0.0f - 1.0f)

[Try PCG32 over at compiler explorer](https://godbolt.org/z/WTa6GTqff)

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

## std_random.hpp 
If you want the best the standard library has to offer, but with a slightly more useful interface, check out std_random.hpp.

[Try std_random.hpp over at Compiler Explorer](https://compiler-explorer.com/z/eqMaz3hbr).
