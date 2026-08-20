#pragma once
#include "../detail/bit_operations.hpp"
#include <assert.h>
#include <stdint.h> // AVR-libc provides <stdint.h>, but not the C++ <cstdint> wrapper.
/*
  SmallFast64 PRNG - a modern C++ 64-bit three-rotate implementation of Jenkins Small Fast PRNG.

  Original algorithm and C code by Bob Jenkins (public domain)
  https://burtleburtle.net/bob/rand/smallprng.html

  C++ implementation by Ulf Benjaminsson, 2025,
  Licensed under the MIT License. See LICENSE.md for details.
  https://github.com/ulfben/cpp_prngs/
*/

namespace rnd {

class SmallFast64{
   using u64 = uint64_t;

public:
   using result_type = u64;
   using seed_type = u64;
   struct state_type{
      u64 a;
      u64 b;
      u64 c;
      u64 d;
   };

private:
   struct Direct{};

   u64 a;
   u64 b;
   u64 c;
   u64 d;

   constexpr SmallFast64(state_type state, Direct) noexcept
      : a(state.a), b(state.b), c(state.c), d(state.d){
      assert((a | b | c | d) != 0 && "SmallFast64 all-zero state is invalid");
   }

public:
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

   constexpr state_type state() const noexcept{
      return {a, b, c, d};
   }

   static constexpr SmallFast64 from_state(state_type state) noexcept{
      return SmallFast64{state, Direct{}};
   }

   static constexpr result_type (max)() noexcept{ // Parentheses prevent expansion of Arduino's max macro.
      // Equivalent to std::numeric_limits<result_type>::max(), but <limits> is not available on AVR-libc.
      return static_cast<result_type>(~result_type{0});
   }
   static constexpr result_type (min)() noexcept{ // Parentheses prevent expansion of Arduino's min macro.
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

} // namespace rnd
