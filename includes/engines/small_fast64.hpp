#pragma once
#include "../detail/bit_operations.hpp"
#include <limits>
#include <cstdint>
/*
  SmallFast64 PRNG - a modern C++ 64-bit three-rotate implementation of Jenkins Small Fast PRNG.

  Original algorithm and C code by Bob Jenkins (public domain)
  https://burtleburtle.net/bob/rand/smallprng.html

  C++ implementation by Ulf Benjaminsson, 2025,
  Licensed under the MIT License. See LICENSE.md for details.
  https://github.com/ulfben/cpp_prngs/
*/

class SmallFast64{
   using u64 = std::uint64_t;
   u64 a;
   u64 b;
   u64 c;
   u64 d;

public:
   using result_type = u64;
   using seed_type = u64;

   constexpr SmallFast64() noexcept
      : SmallFast64(0xBADC0FFEE0DDF00DuLL){}

   explicit constexpr SmallFast64(seed_type seed) noexcept : a(0xf1ea5eeduLL), b(seed), c(seed), d(seed){
      discard(20);// warmup: run the generator a couple of cycles to mix the state thoroughly
   }

   constexpr void seed() noexcept{
      *this = SmallFast64{};
   }
   constexpr void seed(seed_type seed) noexcept{
      *this = SmallFast64{seed};
   }

   static constexpr result_type max() noexcept{
      return std::numeric_limits<u64>::max();
   }
   static constexpr result_type min() noexcept{
       return result_type{0};
   }
   constexpr result_type next() noexcept{
       // The rotate constants (7, 13, 37) are chosen specifically for 64-bit terms, to provide
       // better avalanche characteristics, achieving 18.4 bits of avalanche after 5 rounds.
      const u64 e = a - rnd::detail::rotl(b, 7);
      a = b ^ rnd::detail::rotl(c, 13);
      b = c + rnd::detail::rotl(d, 37);
      c = d + e;
      d = e + a;
      return d;
   }

   constexpr result_type operator()() noexcept{
      return next();
   }

   constexpr void discard(result_type n) noexcept{
      while(n--){
         next();
      }
   }

   constexpr bool operator==(const SmallFast64& rhs) const noexcept{
      return a == rhs.a && b == rhs.b && c == rhs.c && d == rhs.d;
   }
   constexpr bool operator!=(const SmallFast64& rhs) const noexcept{
      return !(*this == rhs);
   }
};
