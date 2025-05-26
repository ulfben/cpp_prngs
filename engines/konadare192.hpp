#pragma once
#include "../concepts.hpp" //for RandomBitEngine
#include <limits>
#include <cstdint>

/*
 * konadare192.hpp - a minimal header-only C++26 port of Pelle Evensen's "konadare192px++" PRNG,
 * adapted for value semantics and API consistency with Xoshiro256, PCG32, SmallFast etc.
 * Original code: https://github.com/pellevensen/PReenactiNG
 *
 * Significant changes:
 *   - Ported to single-file, idiomatic, header-only C++26
 *   - Removed dependencies on <array> and <bit> for reduced compile times
 *   - Removed heap and template usage
 *   - Simplified state handling and seeding
 *   - Renamed and restructured for consistency with other RandomBitEngines in my cpp_prngs repo
 *
 * Copyright 2022 Pelle Evensen
 * 
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * Modifications and C++26 port by Ulf Benjaminsson, 2025.
 *    https://github.com/ulfben/cpp_prngs
 */
class Konadare192 final{
   using u64 = std::uint64_t;
   static constexpr u64 INC = 0xBB67AE8584CAA73BULL;
   static constexpr u64 DEFAULT_SEED = 1;
   u64 a_{};
   u64 b_{};
   u64 c_{};

   static constexpr u64 rotl(u64 x, int r) noexcept{
      return (x << r) | (x >> (64 - r));
   }
   static constexpr u64 rotr(u64 x, int r) noexcept{
      return (x >> r) | (x << (64 - r));
   }
   
   static constexpr u64 mix(u64 a, u64 b) noexcept{ 
      // "kMixNoMul" from Evensen original repo
      u64 c = b;
      u64 x = a;
      for(u64 i = 0; i < 5; ++i){
         x ^= rotr(x, 25) ^ rotr(x, 49);
         c += INC + (c << 15) + (c << 7) + i;
         c ^= (c >> 47) ^ (c >> 23);
         x += c;
         x ^= (x >> 11) ^ (x >> 3);
      }
      return x;
   }
public:
   using result_type = std::uint64_t;

   constexpr Konadare192() : Konadare192(DEFAULT_SEED){}
   constexpr explicit Konadare192(result_type seed_val) : a_(seed_val), b_(seed_val+1), c_(seed_val+2){       
      for(int m = 0; m < 2; ++m){ //two rounds of mixing to warm up the state
         result_type t0 = mix(a_, c_);
         result_type t1 = mix(b_, a_);
         result_type t2 = mix(c_, b_);
         a_ = t0; b_ = t1; c_ = t2;
      }
      if((a_ | b_ | c_) == 0){ //Avoid all-zeros state
         a_ = 0x3C6EF372FE94F82BULL; //sqrt5
      }      
   }

   constexpr result_type next() noexcept{
      result_type out = b_ ^ c_;
      result_type a0 = a_ ^ (a_ >> 32);
      a_ += INC;
      b_ = rotr(b_ + a0, 11);
      c_ = rotl(c_ + b_, 8);
      return out;
   }
   constexpr result_type operator()() noexcept{ return next(); }

   constexpr void discard(uint64_t n) noexcept{
      while(n--){
         next();
      }
   }

   constexpr void seed(result_type value) noexcept{
      *this = Konadare192{value};
   }

   constexpr void seed() noexcept{ 
      *this = Konadare192{}; 
   }

   constexpr Konadare192 split() noexcept{
      return Konadare192{next()};
   }
   
   static constexpr result_type min() noexcept{ 
      return std::numeric_limits<result_type>::min(); 
   }
   static constexpr result_type max() noexcept{ 
      return std::numeric_limits<result_type>::max(); 
   }
   constexpr bool operator==(const Konadare192&) const = default;
};
static_assert(RandomBitEngine<Konadare192>);
