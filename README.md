# cpp_prngs
Pseudo Random Number Generators for Modern C++

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
 
The entire file is only ~100 lines of relatively simple code, executable at compile time and optionally templated to try and support whatever numeric type the user needs.
std::array is perhaps an unecessarily large include and only used for get_state(). If you don't need to save and reload state, simply remove it. :)

[Try SmallPRNG_64 over at compiler explorer](https://godbolt.org/z/oWzfhWzWd).

## SmallPRNG_32.h
A 32-bit 2-rotate version of the the above. You can implement the 64-bit version as a 2-rotate too (using 39 and 11 as your rotation constants), but Jenkins recommends against it due to the lower avalanche achieved. Ergo: I stick with 3-rotate for 64-bit, and 2-rotate for 32-bit. 

[Try SmallPRNG_32 over at compiler explorer](https://godbolt.org/z/bTv499ssW).
