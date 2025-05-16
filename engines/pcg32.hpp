#pragma once
#include "../concepts.hpp" //for RandomBitEngine
#include <limits>
#include <cstdint>

// PCG32 - Permuted Congruential Generator
// Based on the minimal C implementation by Melissa O'Neill (https://www.pcg-random.org)
// C++ implementation by Ulf Benjaminsson (ulfbenjaminsson.com)
// This implementation is placed in the public domain. Use freely.
// Satisfies 'UniformRandomBitGenerator', meaning it works well with std::shuffle, std::sample, most of the std::*_distribution-classes, etc.
// Additionally satisfies 'RandomBitEngine' which I use for the Random utility wrapper

class PCG32 final{
   using u64 = std::uint64_t;
   using u32 = std::uint32_t; //Important! We must guarantee exactly 32 bits. std::uint_fast32_t is often 64-bit on modern 64-bit UNIX systems which would break this implementation
   static constexpr u64 DEFAULT_SEED = 0x853c49e6748fea9bULL;
   static constexpr u64 DEFAULT_STREAM = 0xda3e39cb94b95bdbULL;
   static constexpr u64 MULT = 6364136223846793005ULL;
   u64 state{0}; // RNG state.  All values are possible.
   u64 inc{1}; // controls which RNG sequence (stream) is selected. Must *always* be odd.

public:
   using result_type = u32;

   constexpr PCG32() noexcept
      : PCG32(DEFAULT_SEED, DEFAULT_STREAM){}

   explicit constexpr PCG32(u64 seed_val) noexcept
      : PCG32(seed_val, DEFAULT_STREAM){}

   constexpr PCG32(u64 seed_val, u64 sequence) noexcept{
      seed(seed_val, sequence);
   }

   constexpr void seed() noexcept{
      seed(DEFAULT_SEED, DEFAULT_STREAM);
   }

   //seed and a sequence selection constant (a.k.a. stream id).
   constexpr void seed(u64 seed_val, u64 sequence = DEFAULT_STREAM) noexcept{
      state = 0U;
      inc = (sequence << 1u) | 1u; //ensure inc is odd
      (void) next(); //“Warm up” the internal LCG so the first returned bits are mixed.
      state += seed_val;
      (void) next();
   }

   constexpr result_type operator()() noexcept{
      return next();
   }

   constexpr result_type next() noexcept{
      const auto oldstate = state;
      state = oldstate * MULT + inc;
      const auto xorshifted = ((oldstate >> 18u) ^ oldstate) >> 27u;
      const auto rot = oldstate >> 59u;
      return static_cast<result_type>(
         (xorshifted >> rot) |
         (xorshifted << ((~rot + 1) & 31))
      );
   }

   static constexpr result_type min() noexcept{
      return std::numeric_limits<result_type>::min();
   }
   static constexpr result_type max() noexcept{
      return std::numeric_limits<result_type>::max();
   }

   constexpr void discard(u64 delta) noexcept{
       //Based on Brown, "Random Number Generation with Arbitrary Stride,"
       // Transactions of the American Nuclear Society (Nov. 1994)       
      u64 cur_mult = MULT;
      u64 cur_plus = inc;
      u64 acc_mult = 1u;
      u64 acc_plus = 0u;
      while(delta > 0){
         if(delta & 1){
            acc_mult *= cur_mult;
            acc_plus = acc_plus * cur_mult + cur_plus;
         }
         cur_plus = (cur_mult + 1) * cur_plus;
         cur_mult *= cur_mult;
         delta /= 2;
      }
      state = acc_mult * state + acc_plus;
   }
   constexpr bool operator==(const PCG32& rhs) const noexcept = default;
};
static_assert(RandomBitEngine<PCG32>);