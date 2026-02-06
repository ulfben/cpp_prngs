#pragma once
#include "../concepts.hpp" //for RandomBitEngine
#include <cstdint>
#include <limits>
#include <numeric> //for std::rotl
/*
  RomuDuoJr - Modern C++ Port

  Based on "xromu2jr.h" by Rhet Butler (public domain)
  https://github.com/Almightygir/rhet_RNG/blob/main/xromu2jr.h

  "xromu2jr.h" is based on Mark Overton’s Romu family: https://romu-random.org/
  Featured as a top performer in Rhet Butler’s “RNG Battle Royale” (2020):
  https://web.archive.org/web/20220704174727/https://rhet.dev/wheel/rng-battle-royale-47-prngs-9-consoles/

  C++ port and modifications by Ulf Benjaminsson, 2025
  https://github.com/ulfben/cpp_prngs/

  Licensed under the MIT License. See LICENSE.md for details.
*/
class RomuDuoJr final{
   using u64 = std::uint64_t;
   u64 x;
   u64 y;

   // NASAM-style mixing (Pelle Evensen): diffuses entropy across word 
   // https://mostlymangling.blogspot.com/2020/01/nasam-not-another-strange-acronym-mixer.html    
   static constexpr u64 mix(u64 y) noexcept{
      return y ^ (y >> 23) ^ (y >> 51);
   }
   struct Direct{}; //tag for from_state()
   //private constructor to allow factory function from_state() to bypass the seeding routines.
   constexpr RomuDuoJr(u64 xstate, u64 ystate, Direct) noexcept
      : x(xstate), y(ystate){}
public:
   using result_type = u64;
   using seed_type = u64;
   using state_type = u64; //TODO: why is this public? 

   constexpr RomuDuoJr() noexcept : RomuDuoJr(0xFEEDFACEFEEDFACEULL){}

   explicit constexpr RomuDuoJr(seed_type seed) noexcept
      : x(0x9E6C63D0676A9A99ULL), y(~seed - seed){
      // Initialize x to a fixed odd constant, y to ~seed – seed.
      // Then do two rounds of NASAM mixing + a rotate‐multiply step on x.  
      // This is proven robust even with low-entropy seeds:
          // - All 32-bit seeds tested, no output cycles found in first 2^24 bytes
          // - All 16-bit seeds tested, no output cycles found in first 2^36 bytes
      // ergo: the initializer reliably avoids short-period or degenerate states, even when under-seeded.
      y *= x;
      y = mix(y);
      y *= x;
      x *= std::rotl(y, 27);
      y = mix(y);
   }

   constexpr void seed() noexcept{
      *this = RomuDuoJr{};
   }

   constexpr void seed(seed_type seed) noexcept{
      *this = RomuDuoJr{seed};
   }
   //factory function to create a RomuDuoJr from a state, bypassing the seeding routines.
   static constexpr RomuDuoJr from_state(state_type xstate, state_type ystate) noexcept{
      return RomuDuoJr{xstate, ystate, Direct{}};
   }

   constexpr result_type next() noexcept{
      const u64 old_x = x;
      x = y * 0xD3833E804F4C574BULL;
      y = std::rotl(y - old_x, 27);
      return old_x;
   }

   constexpr result_type operator()() noexcept{
      return next();
   }

   constexpr void discard(result_type n) noexcept{
      while(n--){
         next();
      }
   }

   static constexpr result_type min() noexcept{
      return std::numeric_limits<result_type>::min();
   }

   static constexpr result_type max() noexcept{
      return std::numeric_limits<result_type>::max();
   }

   constexpr bool operator==(const RomuDuoJr& rhs) const noexcept = default;
};
static_assert(RandomBitEngine<RomuDuoJr>);

#ifdef VALIDATE_PRNGS
// Original implementation of RomuDuoJr from Mark Overton´s 2020 paperm for validation purposes 
// see: https://www.romu-random.org/code.c
// adjusted for constexpr evaluation, but otherwise unchanged
#define ROTL(d,lrot) ((d<<(lrot)) | (d>>(8*sizeof(d)-(lrot))))
struct romu_state{
   uint64_t xState, yState;  // set to nonzero seed
};
constexpr uint64_t romuDuoJr_random(romu_state& s) noexcept {
   uint64_t xp = s.xState;
   s.xState = 15241094284759029579u * s.yState;
   s.yState = s.yState - xp;  s.yState = ROTL(s.yState,27);
   return xp;
}

static constexpr auto ROMUDUJR_REFERENCE = []{
   romu_state s{123, 0};
   std::array<RomuDuoJr::result_type, 6> out{};
   for(auto& v : out){ v = romuDuoJr_random(s); }
   return out;
   }();

static_assert(prng_outputs(RomuDuoJr::from_state(123, 0)) == ROMUDUJR_REFERENCE, "RomuDuoJr output does not match romuDuoJr reference");

#endif //VALIDATE_PRNGS