#pragma once
#include <cassert>
#include <cmath>
#include <type_traits>
#ifdef _MSC_VER
#include <intrin.h>    // for _umul128
#endif
#include "concepts.hpp" //for RandomBitEngine concept

// This is a simple random number generator interface that wraps around any engine that meets the RandomBitEngine concept.
// providing useful functions for generating random numbers, including integers, floating-point numbers, and colors
// as well as methods for Gaussian distribution, coin flips (with odds), picking from collections (index or element), etc.
// Source: https://github.com/ulfben/cpp_prngs/
// Demo is available on Compiler Explorer: https://compiler-explorer.com/z/dGj41dKa9
// Benchmark on Quick Bench: https://quick-bench.com/q/ZSBTxZHWDN34Im2Y6_4rEx9Xbpc
namespace rnd {
   template<RandomBitEngine E>
   class Random final{
      E _e{}; //the underlying engine providing random bits. This class will turn those into useful values.

     //private helper for 128-bit multiplications
     // computes the high 64 bits of a 64×64-bit multiplication.
     // Used to implement Daniel Lemire’s "fastrange" trick: https://lemire.me/blog/2016/06/27/a-fast-alternative-to-the-modulo-reduction/   
     // which maps uniformly distributed `x` in [0, 2^BITS) to [0, bound) with negligible bias.     
      template<unsigned BITS>
      constexpr std::uint64_t mul_shift_high64(std::uint64_t x, std::uint64_t bound) noexcept{
#if defined(_MSC_VER)
         // MSVC doesn't support __uint128_t; use intrinsic instead
         std::uint64_t high{};
         (void) _umul128(x, bound, &high); // low 64 bits discarded
         return high; //return the high 64 bits
#elif defined(__SIZEOF_INT128__)
         return std::uint64_t((__uint128_t(x) * __uint128_t(bound)) >> BITS);
#else
         static_assert(false, "mul_shift_high64 requires either __uint128_t or MSVC _umul128");
#endif
      }

   public:
      using engine_type = E;
      using result_type = typename E::result_type;
      static constexpr unsigned BITS = std::numeric_limits<result_type>::digits;

      constexpr Random() noexcept = default; //the engine will default initialize
      explicit constexpr Random(engine_type engine) noexcept : _e(engine){}
      explicit constexpr Random(result_type seed_val) noexcept : _e(seed_val){};
      constexpr bool operator==(const Random& rhs) const noexcept = default;

      //access to the underlying engine for manual serialization, etc.
      constexpr const E& engine() const noexcept{ return _e; }
      constexpr E& engine() noexcept{ return _e; }

      //advance the random engine n steps. 
      //some engines (like PCG32) can do this faster than linear time
      constexpr void discard(unsigned long long n) noexcept{
         _e.discard(n);
      }

      constexpr void seed() noexcept{
         _e = E{};
      }
      constexpr void seed(result_type v) noexcept{
         _e.seed(v);
      }

      static constexpr auto min() noexcept{
         return E::min();
      }
      static constexpr auto max() noexcept{
         return E::max();
      }

      // Produces a random value in the range [min(), max()]
      constexpr result_type next() noexcept{ return _e(); }
      constexpr result_type operator()() noexcept{ return next(); }

      // produces a random value in [0, bound) using Lemire's fastrange.
      // achieves very small bias without using rejection, and is much faster than naive modulo.
      constexpr result_type next(result_type bound) noexcept{
         assert(bound > 0 && "bound must be positive");

         result_type raw_value = next(); // raw_value ∈ [0, 2^BITS)

         if constexpr(BITS <= 32){ // for small engines, multiply into a 64-bit product             
            auto product = std::uint64_t(raw_value) * std::uint64_t(bound); // product is now in [0, bound * 2^BITS)
            auto result = result_type(product >> BITS); // equivalent to floor(product / 2^BITS)
            return result; // result is now in range [0, bound)
         } else if constexpr(BITS <= 64){
             // same logic, but use helper for 128-bit math, since __uint128_t isn't universally available
            return mul_shift_high64<BITS>(raw_value, bound);
         }
         // fallback for hypothethcial >64-bit engines. Naive modulo (slower, more bias)         
         return bound > 0 ? raw_value % bound : bound; // avoid division by zero in release builds
      }      
      constexpr result_type operator()(result_type bound) noexcept{ return next(bound); }

      // integer in [lo, hi)
      template<std::integral I>
      constexpr I between(I lo, I hi) noexcept{
         if(!(lo < hi)){
            assert(false && "between(lo, hi): inverted or empty range");
            return lo;
         }
         using U = std::make_unsigned_t<I>;
         U bound = U(hi) - U(lo);
         assert(bound <= E::max() && "between(lo, hi): range too large for this engine. Consider a 64-bit engine (xoshiro256ss, SmallFast64) or ensure hi–lo <= max()");
         auto safe_bound = static_cast<result_type>(bound);
         return lo + static_cast<I>(next(safe_bound));
      }

      // real in [lo, hi)
      template<std::floating_point F>
      constexpr F between(F lo, F hi) noexcept{
         return lo + (hi - lo) * normalized<F>();
      }

      // real in [0.0, 1.0)
      template<std::floating_point F>
      constexpr F normalized() noexcept{
         constexpr F inv_range = F(1) / (F(max()) + F(1));
         return F(next()) * inv_range;
      }

      // real in [-1.0, 1.0)
      template<std::floating_point F>
      constexpr F signed_norm() noexcept{
         return F(2) * normalized<F>() - F(1);
      }

      // boolean
      constexpr bool coin_flip() noexcept{ return bool(next() & 1); }

      // boolean with probability
      template<std::floating_point F>
      constexpr bool coin_flip(F probability) noexcept{
         return normalized<F>() < probability;
      }

      // 24-bit RGB packed as 0xRRGGBB
      constexpr std::uint32_t rgb8() noexcept{
         static_assert(BITS >= 24, "rgb8() requires an engine with at least 24 bits");
         return static_cast<std::uint32_t>(next() & 0x00'FF'FF'FFu);
      }

      // 32-bit RGBA packed as 0xRRGGBBAA
      constexpr std::uint32_t rgba8() noexcept{
         if constexpr(BITS >= 32){
           // one raw sample, grab low 32 bits            
            return static_cast<std::uint32_t>(next() & 0xFF'FF'FF'FFu);
         } else{
           // narrow engine: fall back to four 8-bit calls
            return (next(256) << 24)
               | (next(256) << 16)
               | (next(256) << 8)
               | next(256);
         }
      }

      // pick an index in [0, size)
      template<std::ranges::sized_range R>
      constexpr auto index(const R& collection) noexcept{
         assert(!std::ranges::empty(collection) && "Random::index(): empty collection.");
         using idx_t = std::ranges::range_size_t<R>;
         return static_cast<idx_t>(
            between<idx_t>(0,
               static_cast<idx_t>(std::ranges::size(collection))));
      }

      // get an iterator to a random element. Accepts const and non-const ranges
      template<std::ranges::forward_range R>
         requires std::ranges::sized_range<R>
      constexpr auto iterator(R&& collection) noexcept{
         assert(!std::ranges::empty(collection) && "Random::iterator(): empty collection");
         auto idx = index(collection); // index accepts const&
         auto it = std::ranges::begin(collection); // picks begin or cbegin for us
         std::advance(it, idx);
         return it;
      }

      //return a reference to a random element in a collection
      //accepts both const and non-const ranges
      template<std::ranges::forward_range R>
         requires std::ranges::sized_range<R>
      constexpr auto element(R&& collection) noexcept{
         return *iterator(std::forward<R>(collection));
      }

      template<std::floating_point F>
      constexpr F gaussian(F mean, F stddev) noexcept{
            // Based on the Central Limit Theorem; https://en.wikipedia.org/wiki/Central_limit_theorem
            // the Irwin–Hall distribution (sum of 12 U(0,1) has mean = 6, variance = 1).            
            // Subtract 6 and multiply by stddev to get an approximate N(mean, stddev) sample.
         F sum{};
         for(auto i = 0; i < 12; ++i){
            sum += normalized<F>();
         }
         return mean + (sum - F(6)) * stddev;
      }
   };
} //namespace rnd