#pragma once
#include "../concepts.hpp" //for RandomBitEngine
#include <limits>
#include <cstdint>
/*
  SmallFast64 PRNG - a modern C++ 32-bit two-rotate implementation of Jenkins Small Fast PRNG.

  Original algorithm and C code by Bob Jenkins (public domain)
  https://burtleburtle.net/bob/rand/smallprng.html

  C++ implementation by Ulf Benjaminsson, 2025,
  Licensed under the MIT License. See LICENSE.md for details.
  https://github.com/ulfben/cpp_prngs/
*/
class SmallFast32 final{
   using u32 = std::uint32_t;
   u32 a;
   u32 b;
   u32 c;
   u32 d;

   static constexpr u32 rot(u32 x, u32 k) noexcept{
      return (x << k) | (x >> (32 - k));
   }

public:
   using result_type = u32;

   constexpr SmallFast32() noexcept
      : SmallFast32(0xBADC0FFEu){}

   explicit constexpr SmallFast32(result_type seed) noexcept : a(0xf1ea5eedu), b(seed), c(seed), d(seed){
      discard(20); //warmup
   }

   constexpr void seed() noexcept{
      *this = SmallFast32{};
   }
   constexpr void seed(result_type seed) noexcept{
      *this = SmallFast32{seed};
   }

   static constexpr result_type min() noexcept{
      return std::numeric_limits<result_type>::min();
   }
   static constexpr result_type max() noexcept{
      return std::numeric_limits<result_type>::max();
   }
   constexpr result_type operator()() noexcept{
      return next();
   }

   constexpr result_type next() noexcept{
      const u32 e = a - rot(b, 27);
      a = b ^ rot(c, 17);
      b = c + d;
      c = d + e;
      d = e + a;
      return d;
   }

   constexpr void discard(unsigned long long n) noexcept{
      while(n--){
         next();
      }
   }

   constexpr SmallFast32 split() noexcept{
      return SmallFast32{next()};
   }

   constexpr bool operator==(const SmallFast32& rhs) const noexcept = default;
};
static_assert(RandomBitEngine<SmallFast32>);

#ifdef VALIDATE_PRNGS
//Reference implementation of JSF (Jenkins Small Fast) PRNG
// https://burtleburtle.net/bob/rand/smallprng.html
// used to verify the SmallFast32 implementation
using u4 = std::uint32_t;
struct ranctx{
   u4 a, b, c, d;
};
constexpr u4 ranval(ranctx& x) noexcept{
   constexpr auto rot32 = [](u4 x, unsigned k) noexcept -> u4{
      return (((x) << (k)) | ((x) >> (32 - (k))));
      };
   const u4 e = x.a - rot32(x.b, 27);
   x.a = x.b ^ rot32(x.c, 17);
   x.b = x.c + x.d;
   x.c = x.d + e;
   x.d = e + x.a;
   return x.d;
}
constexpr ranctx raninit(u4 seed) noexcept{
   ranctx x{0xf1ea5eedu, seed, seed, seed};
   for(unsigned i = 0; i < 20; ++i){
      ranval(x);
   }
   return x;
}
static constexpr auto JSF_REFERENCE = []{
   ranctx ctx = raninit(123);
   std::array<SmallFast32::result_type, 6> out{};
   for(auto& v : out){ v = ranval(ctx); }
   return out;
   }();

static_assert(prng_outputs(SmallFast32(123)) == JSF_REFERENCE, "SmallFast32 output does not match JSF reference");

#endif //VALIDATE_PRNGS