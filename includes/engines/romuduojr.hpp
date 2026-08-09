#pragma once
#include "../detail/bit_operations.hpp"
#include <stdint.h> // AVR-libc provides <stdint.h>, but not the C++ <cstdint> wrapper.
/*
  RomuDuoJr - Modern C++ Port

  Based on "xromu2jr.h" by Rhet Butler (public domain):
  https://github.com/Almightygir/rhet_RNG/blob/main/xromu2jr.h

  "xromu2jr.h" is based on Mark Overton's Romu family:
  https://romu-random.org/

  Featured as a top performer in Rhet Butler's "RNG Battle Royale" (2020):
  https://web.archive.org/web/20220704174727/https://rhet.dev/wheel/rng-battle-royale-47-prngs-9-consoles/

  The seed initializer uses mixing derived from Pelle Evensen's public-domain NASAM mixer family:
  https://mostlymangling.blogspot.com/2020/01/nasam-not-another-strange-acronym-mixer.html

  C++ port and modifications by Ulf Benjaminsson, 2025
  https://github.com/ulfben/cpp_prngs/

  Licensed under the MIT License. See LICENSE.md for details.
*/
class RomuDuoJr final{
   using u64 = uint64_t;
   using state_type = u64;
   state_type x;
   state_type y;

   struct Direct{}; //tag for from_state()
   //private constructor to allow factory function from_state() to bypass the seeding routines.
   constexpr RomuDuoJr(state_type xstate, state_type ystate, Direct) noexcept
      : x(xstate), y(ystate){}
public:
   using result_type = u64;
   using seed_type = u64;   

   constexpr RomuDuoJr() noexcept : RomuDuoJr(0xFEEDFACEFEEDFACEULL){}

   explicit constexpr RomuDuoJr(seed_type seed) noexcept
      : x(0x9E6C63D0676A9A99ULL), y(~seed - seed){
      // Initialize x to a fixed odd constant, y to ~seed – seed.
      // Then do two rounds of NASAM-style mixing + a rotate‐multiply step on x.  
      // Rhet Butler empirically tuned this and proved it robust even with low-entropy seeds:
          // - All 32-bit seeds tested, no output cycles found in first 2^24 outputs
          // - All 16-bit seeds tested, no output cycles found in first 2^36 outputs
      // ergo: the initializer reliably avoids short-period or degenerate states, even when under-seeded.
      y *= x;
      y = y ^ (y >> 23) ^ (y >> 51);
      y *= x;
      x *= rnd::detail::rotl(y, 27);
      y = y ^ (y >> 23) ^ (y >> 51);
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
      const state_type old_x = x;
      x = y * 15241094284759029579ULL;
      y = rnd::detail::rotl(y - old_x, 27);
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

   static constexpr result_type (min)() noexcept{ // Parentheses prevent expansion of Arduino's min macro.
       return result_type{0};
   }

   static constexpr result_type (max)() noexcept{ // Parentheses prevent expansion of Arduino's max macro.
      // Equivalent to std::numeric_limits<result_type>::max(), but <limits> is not available on AVR-libc.
      return static_cast<result_type>(~result_type{0});
   }

   constexpr bool operator==(const RomuDuoJr& rhs) const noexcept{
      return x == rhs.x && y == rhs.y;
   }
   constexpr bool operator!=(const RomuDuoJr& rhs) const noexcept{
      return !(*this == rhs);
   }
};
