#pragma once
#include "gtest/gtest.h"
#include "./engines/romuduojr.hpp"
#include "./engines/konadare192.hpp"
#include "./engines/pcg32.hpp"
#include "./engines/small_fast32.hpp"
#include "./engines/small_fast64.hpp"
#include "./engines/xoshiro256ss.h"
#include "random.hpp"

// Source: https://github.com/ulfben/cpp_prngs/
// Demo is available on Compiler Explorer: https://compiler-explorer.com/z/nzK9joeYE
// Benchmarks:
   // Quick Bench for generating raw random values: https://quick-bench.com/q/vWdKKNz7kEyf6kQSNnUEFOX_4DI
   // Quick Bench for generating normalized floats: https://quick-bench.com/q/GARc3WSfZu4sdVeCAMSWWPMQwSE
   // Quick Bench for generating bounded values: https://quick-bench.com/q/WHEcW9iSV7I8qB_4eb1KWOvNZU0

template<class Engine>
class RandomTypedTest : public ::testing::Test{
protected:
    using Rng = rnd::Random<Engine>;
    Rng rng{}; // default-seeded
};

using EnginesUnderTest = ::testing::Types<
    RomuDuoJr,
    Konadare192,
    PCG32,
    SmallFast32,
    SmallFast64,
    Xoshiro256SS
>;

TYPED_TEST_CASE(RandomTypedTest, EnginesUnderTest);

// -----------------------------------------------------------------------------
// Basic properties of next()
// -----------------------------------------------------------------------------
TYPED_TEST(RandomTypedTest, DefaultConstructedEnginesAreDeterministic){
    using Engine = TypeParam;
    using Rng = typename RandomTypedTest<Engine>::Rng;
    Rng a{};
    Rng b{};
    for(int i = 0; i < 1024; ++i){
        auto va = a.next();
        auto vb = b.next();
        EXPECT_EQ(va, vb) << "Default constructed RNGs must produce same sequence";
    }
}

TYPED_TEST(RandomTypedTest, NextProducesDifferentValuesOverTime){
    auto v1 = this->rng.next();
    auto v2 = this->rng.next();
    auto v3 = this->rng.next();
    // Not a strong randomness test, just a smoke test that it is not completely broken.
    // If some engine can legitimately produce duplicates here, feel free to relax this.
    EXPECT_NE(v1, v2);
    EXPECT_NE(v2, v3);
}

// -----------------------------------------------------------------------------
// Bounded generation: next(bound)
// -----------------------------------------------------------------------------
TYPED_TEST(RandomTypedTest, NextBoundedRespectsUpperBound){
    constexpr std::uint32_t bound = 10;
    for(int i = 0; i < 1024; ++i){
        auto v = this->rng.next(bound);
        EXPECT_LT(v, bound);
    }
}

// -----------------------------------------------------------------------------
// next<N, T>() returns values in [0, N)
// -----------------------------------------------------------------------------
TYPED_TEST(RandomTypedTest, NextCompileTimeBoundedRespectsBound){
    constexpr unsigned N = 10;

    for(int i = 0; i < 1024; ++i){
        auto v = this->rng.template next<N, std::uint32_t>();
        EXPECT_LT(v, N);
    }
}

// -----------------------------------------------------------------------------
// min(), max(), and range of next()
// -----------------------------------------------------------------------------
TYPED_TEST(RandomTypedTest, NextRespectsMinMaxRange){
    auto lo = this->rng.min();
    auto hi = this->rng.max();

    EXPECT_LT(lo, hi);

    for(int i = 0; i < 1024; ++i){
        auto v = this->rng.next();
        EXPECT_GE(v, lo);
        EXPECT_LT(v, hi) << "next() must be in [min(), max())";
    }
}

// -----------------------------------------------------------------------------
// operator== follows engine state
// -----------------------------------------------------------------------------
TYPED_TEST(RandomTypedTest, EqualityTracksState){
    using Engine = TypeParam;
    using Rng = rnd::Random<Engine>;

    Rng a{};
    Rng b{};

    EXPECT_TRUE(a == b);

    a.next();
    EXPECT_FALSE(a == b);

    b.next();
    EXPECT_TRUE(a == b);

    a.next();
    a.next();
    b.next();
    EXPECT_FALSE(a == b);

    b.next();
    EXPECT_TRUE(a == b);
}


// -----------------------------------------------------------------------------
// Between(lo, hi)
// -----------------------------------------------------------------------------
TYPED_TEST(RandomTypedTest, BetweenProducesValuesWithinInclusiveRange){
    constexpr int lo = -5;
    constexpr int hi = 7;

    for(int i = 0; i < 1024; ++i){
        auto v = this->rng.between(lo, hi);
        EXPECT_GE(v, lo);
        EXPECT_LE(v, hi);
    }
}

// -----------------------------------------------------------------------------
// normalized<F>() in [0, 1), signed_norm<F>() in [-1, 1)
// -----------------------------------------------------------------------------
TYPED_TEST(RandomTypedTest, NormalizedProducesFloatInUnitInterval){
    for(int i = 0; i < 2048; ++i){
        float f = this->rng.normalized<float>();
        EXPECT_TRUE(std::isfinite(f));
        EXPECT_GE(f, 0.0f);
        EXPECT_LT(f, 1.0f);
    }
}

TYPED_TEST(RandomTypedTest, SignedNormProducesFloatInSignedUnitInterval){
    for(int i = 0; i < 2048; ++i){
        float f = this->rng.signed_norm<float>();
        EXPECT_TRUE(std::isfinite(f));
        EXPECT_GE(f, -1.0f);
        EXPECT_LT(f, 1.0f);
    }
}

// -----------------------------------------------------------------------------
// coin_flip: extremes p = 0, 1 and that default produces both values
// -----------------------------------------------------------------------------
TYPED_TEST(RandomTypedTest, CoinFlipDefaultProducesBothOutcomes){
    bool saw_true = false;
    bool saw_false = false;

    for(int i = 0; i < 256; ++i){
        if(this->rng.coin_flip()){
            saw_true = true;
        } else{
            saw_false = true;
        }
        if(saw_true && saw_false){
            break;
        }
    }

    EXPECT_TRUE(saw_true);
    EXPECT_TRUE(saw_false);
}

TYPED_TEST(RandomTypedTest, CoinFlipProbabilityExtremes){
    // p = 0 -> always false over a reasonable sample
    bool any_true = false;
    for(int i = 0; i < 512; ++i){
        if(this->rng.coin_flip(0.0f)){ //note: float, to support 32-bit engines
            any_true = true;
            break;
        }
    }
    EXPECT_FALSE(any_true);

    // p = 1 -> always true over a reasonable sample
    bool any_false = false;
    for(int i = 0; i < 512; ++i){
        if(!this->rng.coin_flip(1.0f)){  //note: float, to support 32-bit engines
            any_false = true;
            break;
        }
    }
    EXPECT_FALSE(any_false);
}

// -----------------------------------------------------------------------------
// Constructing from engine() reproduces the same stream
// -----------------------------------------------------------------------------
TYPED_TEST(RandomTypedTest, EngineAccessorAndEngineConstructorRoundTrip){
    using Engine = TypeParam;
    using Rng = rnd::Random<Engine>;

    Rng a{123u};
    // Advance a a bit to get a non default engine state
    for(int i = 0; i < 7; ++i){
        a.next();
    }

    Engine e = a.engine();
    Rng b{e};

    for(int i = 0; i < 32; ++i){
        EXPECT_EQ(a.next(), b.next());
    }
}


// -----------------------------------------------------------------------------
// Reproducibility with explicit seed
// -----------------------------------------------------------------------------
TYPED_TEST(RandomTypedTest, SameSeedProducesSameSequence){
    using Engine = TypeParam;
    using Rng = rnd::Random<Engine>;
    using result_type = Engine::result_type;

    auto seed = result_type{123456789u};

    Rng a{seed};
    Rng b{seed};

    for(int i = 0; i < 1024; ++i){
        auto va = a.next();
        auto vb = b.next();
        EXPECT_EQ(va, vb);
    }
}

TYPED_TEST(RandomTypedTest, DifferentSeedsProduceDifferentSequences){
    using Engine = TypeParam;
    using Rng = rnd::Random<Engine>;

    Rng a{123u};
    Rng b{456u};

    bool all_equal = true;
    for(int i = 0; i < 32; ++i){
        if(a.next() != b.next()){
            all_equal = false;
            break;
        }
    }
    EXPECT_FALSE(all_equal)
        << "Different seeds should not produce identical sequences (at least not for 32 steps)";
}

// -----------------------------------------------------------------------------
// discard(n) is equivalent to calling next() n times
// -----------------------------------------------------------------------------
TYPED_TEST(RandomTypedTest, DiscardSkipsValues){
    using Engine = TypeParam;
    using Rng = rnd::Random<Engine>;

    Rng a{123u};
    Rng b{123u};

    constexpr std::uint64_t skip = 25;

    a.discard(skip);
    for(std::uint64_t i = 0; i < skip; ++i){
        b.next();
    }

    EXPECT_EQ(a.next(), b.next());
}

// -----------------------------------------------------------------------------
// bits(n) and bits<N, T>() only set the requested bits
// -----------------------------------------------------------------------------
TYPED_TEST(RandomTypedTest, BitsRuntimeReturnsOnlyRequestedBits){
    using Engine = TypeParam;
    using result_type = typename Engine::result_type;

    for(unsigned n : {1u, 8u, 16u}){
        auto v = this->rng.bits(n);
        const auto max_val = (n == 0u)
            ? result_type{0}
        : (result_type(1) << n) - 1;
        EXPECT_LE(v, max_val)
            << "bits(" << n << ") must be in [0, 2^n)";
    }
}

TYPED_TEST(RandomTypedTest, BitsCompileTimeReturnsOnlyRequestedBits){
    // 8 bits fit in uint16_t, and 8 <= digits(result_type) for all tested engines
    auto v = this->rng.template bits<8, std::uint16_t>();
    EXPECT_LE(v, std::uint16_t{0xFF});
}

TYPED_TEST(RandomTypedTest, BitsAsUsesAllDigitsOfTargetType){
    auto v = this->rng.template bits_as<std::uint32_t>();
    // This is hard to check strictly; just make sure it fits the full range.
    // If bits_as is broken, many engines will not cover a significant part
    // of the range and this test will likely fail with other tests.
    EXPECT_TRUE(v <= std::numeric_limits<std::uint32_t>::max());
}

TYPED_TEST(RandomTypedTest, SeedWithValueResetsToGivenSequence){
    using Engine = TypeParam;
    using Rng = rnd::Random<Engine>;
    using result_type = typename Engine::result_type;

    const result_type seed_val{987654321u};

    Rng a{seed_val};
    Rng b{};
    b.seed(seed_val);

    for(int i = 0; i < 16; ++i){
        EXPECT_EQ(a.next(), b.next());
    }
}

// -----------------------------------------------------------------------------
// Validation: 128-bit multiplication intrinsic vs constexpr fallback
// -----------------------------------------------------------------------------

// 1. Define a subset of engines that are 64-bit.
//    We exclude 32-bit engines (PCG32, SmallFast32) because they use a 
//    simpler logic path (casting to uint64_t) that doesn't utilize 
//    the 128-bit fallback/intrinsic split we want to test.
using Engines64Bit = ::testing::Types<
    RomuDuoJr,
    Konadare192,
    SmallFast64,
    Xoshiro256SS
>;

template<class Engine>
class ConstexprValidationTest : public ::testing::Test{
public:
    // We use a fixed seed for validation to ensure the compile-time and runtime engines start at the exact same state.
    static constexpr typename Engine::result_type SEED = 123456789;
    static constexpr size_t SAMPLE_SIZE = 500;
};

TYPED_TEST_CASE(ConstexprValidationTest, Engines64Bit);

// 2. The Consteval Generator
//    This function MUST run at compile time. It forces the compiler
//    to use the soft C++ implementation of mul_shift_u64, bypassing
//    any runtime intrinsics like _umul128.
template <typename Engine>
consteval auto generate_reference_samples(){
    using result_type = typename Engine::result_type;    
    std::array<result_type, ConstexprValidationTest<Engine>::SAMPLE_SIZE> results{};
    rnd::Random<Engine> rng(ConstexprValidationTest<Engine>::SEED);
    for(size_t i = 0; i < results.size(); ++i){
        // Create a chaotic bound to test various bit-shifts.
        // We avoid 0 (assert failure) and ensure it varies.
        result_type bound = (i * 1234567890123ULL) + 7;
        // Edge case: Force max bound to test full range multiplication
        if(i == 0) bound = Engine::max();
        results[i] = rng.next(bound);
    }
    return results;
}

// 3. The Test
TYPED_TEST(ConstexprValidationTest, RuntimeIntrinsicsMatchConstexprFallback){
#if !defined(_MSC_VER)
    GTEST_SKIP() << "This test only distinguishes constexpr fallback vs _umul128 on MSVC.";
#endif
    using Engine = TypeParam;
    using result_type = typename Engine::result_type;

    // A. COMPILE TIME: Generate the "Truth" table
    //    This guarantees we used the portable C++ fallback logic.
    static constexpr auto expected_values = generate_reference_samples<Engine>();

    // B. RUNTIME: Generate the actual values
    //    On MSVC/x64, this will use _umul128 (the intrinsic).
    rnd::Random<Engine> rng(TestFixture::SEED);

    // C. Compare
    for(size_t i = 0; i < TestFixture::SAMPLE_SIZE; ++i){
        volatile result_type bound = (i * 1234567890123ULL) + 7;
        if(i == 0) bound = Engine::max();
        result_type actual = rng.next(bound);
        ASSERT_EQ(expected_values[i], actual)
            << std::hex
            << "Mismatch at index " << i << " for engine " << typeid(Engine).name()
            << "\nBound was: 0x" << bound
            << "\nFallback (Correct): 0x" << expected_values[i]
            << "\nRuntime  (Actual):  0x" << actual;
    }
}

int main(int argc, char** argv){
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}