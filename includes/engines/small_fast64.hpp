#pragma once
#include "../concepts.hpp" //for RandomBitEngine
#include <limits>
#include <cstdint>
#include <bit> //std::rotl
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
      const u64 e = a - std::rotl(b, 7);
      a = b ^ std::rotl(c, 13);
      b = c + std::rotl(d, 37);
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

   constexpr bool operator==(const SmallFast64& rhs) const noexcept = default;
};
static_assert(RandomBitEngine<SmallFast64>);

#if VALIDATE_PRNGS
//Reference implementation of JSF (Jenkins Small Fast) PRNG
// https://burtleburtle.net/bob/rand/smallprng.html
// used to verify the SmallFast64 implementation
using u8 = std::uint64_t;
struct ranctx64{
   u8 a, b, c, d;
};
constexpr u8 ranval64(ranctx64& x) noexcept{
   constexpr auto rot64 = [](u8 x, unsigned k) noexcept -> u8{
      return (((x) << (k)) | ((x) >> (64 - (k))));
      };
   u8 e = x.a - rot64(x.b, 7);
   x.a = x.b ^ rot64(x.c, 13);
   x.b = x.c + rot64(x.d, 37);
   x.c = x.d + e;
   x.d = e + x.a;
   return x.d;
}
constexpr ranctx64 raninit64(u8 seed) noexcept{
   ranctx64 x{0xf1ea5eedu, seed, seed, seed};
   for(unsigned i = 0; i < 20; ++i){
      ranval64(x);
   }
   return x;
}
static constexpr auto JSF64_REFERENCE = []{
   ranctx64 ctx = raninit64(123);
   std::array<SmallFast64::result_type, 6> out{};
   for(auto& v : out){ v = ranval64(ctx); }
   return out;
   }();

static_assert(prng_outputs(SmallFast64(123)) == JSF64_REFERENCE, "SmallFast64 output does not match JSF reference");

#endif //VALIDATE_PRNGS