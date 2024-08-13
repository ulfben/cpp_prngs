# cpp_prngs
When talking about random number generation for making games fun (ergo: not the kind of randomness you'd use to generate a SSH key pair...), the standard library comes up short in many ways. The `<random>` library is [not particularly user friendly](https://youtu.be/zUVQhcu32rg?si=G3LHsYagEHhH9UYS&t=234), it is [easy to use wrong](https://www.youtube.com/watch?v=4_QO1nm7uJs), provides [poor guarantuees and zero portability](https://codingnest.com/generating-random-numbers-using-c-standard-library-the-problems/) and [std::mt19937 is famously slow and bloated](https://quuxplusone.github.io/blog/2021/11/23/xoshiro/). 

Hence this repo. It holds my collection of pseudo random number generators for C++. The PRNGs in this repo are all single-file, mostly constexpr and provides interfaces that are helpful to a game dev - just copy-paste any one of them whenever you need random numbers for a project. 

Enjoy, and do let me know if you run into any bugs or add any cool features!

## SmallPRNG_64.h
My public domain port of [Jenkins' smallfast prng](https://burtleburtle.net/bob/rand/smallprng.html). SmallPRNG_64.h is a 64-bit three-rotate implementation, including a pretty handy interface;

* `between(min, max)` -> [min, max] (inclusive)
* `normalized()` - 0.0-1.0
* `coinToss()` -> true or false
* `unit_range()` -> -1.0 - 1.0
* `next()` -> [0, std::numeric_limits<u64>::max()]
* `next_gaussian(mean, deviation)` -> random number following a normal distribution centered around the mean
* `get_state()` -> std::array of the rng state for saving
* `set_state(span<u64>)` : restore the state of the rng

Additionally satisfies [UniformRandomBitGenerator](https://en.cppreference.com/w/cpp/named_req/UniformRandomBitGenerator), meaning that it supports std::shuffle, std::sample, most of the std::*_distribution-classes, etc.

The entire file is about 100 lines of relatively simple code, executable at compile time, and optionally templated to support various numeric types. std::array is perhaps an unnecessarily large inclusion and is only used for get_state(). If you don't need to save and reload the state, you can easily remove it. :)

[Try SmallPRNG_64 over at compiler explorer](https://godbolt.org/z/xGvvPq8vh).

## SmallPRNG_32.h
A 32-bit 2-rotate version of the the above. You can implement the 64-bit version as a 2-rotate too (using 39 and 11 as your rotation constants), but Jenkins recommends against it due to the lower avalanche achieved. Therefore, I use three rotates for 64-bit and two rotates for 32-bit. 

[Try SmallPRNG_32 over at compiler explorer](https://godbolt.org/z/ExhWaGqGa).

## xoshiro256ss.h
The "xoshiro256** 1.0" generator. Public interface, rejection sampling and seeding utilities (see; [seeding.h](https://github.com/ulfben/cpp_prngs/blob/main/seeding.h)) by Ulf Benjaminsson (2023). 
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
