#pragma once
#include "../concepts.hpp" //for RandomBitEngine
#include <limits>
#include <cstdint>
/*
A C++ 64-bit three-rotate implementation of Jenkins Small Fast PRNG.
Original public domain C-code and writeup by Bob Jenkins https://burtleburtle.net/bob/rand/smallprng.html
C++ implementation by Ulf Benjaminsson (ulfbenjaminsson.com), also placed in the public domain. Use freely!
*/
class SmallFast64{
   using u64 = std::uint64_t;
   u64 a;
   u64 b;
   u64 c;
   u64 d;

   static constexpr u64 rot(u64 x, u64 k) noexcept{
      return (x << k) | (x >> (64 - k));
   }

public:
   using result_type = u64;

   constexpr SmallFast64() noexcept
      : SmallFast64(0xBADC0FFEE0DDF00DuLL){}

   explicit constexpr SmallFast64(result_type seed) noexcept : a(0xf1ea5eeduLL), b(seed), c(seed), d(seed){
      discard(20);// warmup: run the generator a couple of cycles to mix the state thoroughly
   }

   constexpr void seed() noexcept{
      *this = SmallFast64{};
   }
   constexpr void seed(result_type seed) noexcept{
      *this = SmallFast64{seed};
   }

   static constexpr result_type max() noexcept{
      return std::numeric_limits<u64>::max();
   }
   static constexpr result_type min() noexcept{
      return std::numeric_limits<u64>::min();
   }
   constexpr result_type next() noexcept{
       // The rotate constants (7, 13, 37) are chosen specifically for 64-bit terms, to provide
       // better avalanche characteristics, achieving 18.4 bits of avalanche after 5 rounds.
      const u64 e = a - rot(b, 7);
      a = b ^ rot(c, 13);
      b = c + rot(d, 37);
      c = d + e;
      d = e + a;
      return d;
   }

   constexpr result_type operator()() noexcept{
      return next();
   }

   constexpr void discard(unsigned long long n) noexcept{
      while(n--){
         next();
      }
   }

   constexpr bool operator==(const SmallFast64& rhs) const noexcept = default;
};
static_assert(RandomBitEngine<SmallFast64>);