#include "engine_reference_validation.hpp"
#include "wide_multiply_validation.hpp"
#include "gtest/gtest.h"
#include <rnd/random.hpp>
#include <array>
#include <cmath>
#include <cstdint>
#include <list>
#include <limits>
#include <span>
#include <type_traits>
#include <utility>
#include <vector>

using namespace rnd;

// Source: https://github.com/ulfben/cpp_prngs/
// Demo is available on Compiler Explorer: https://compiler-explorer.com/z/YTbGcreEe
// Benchmarks:
   // Quick Bench for generating raw random values: https://quick-bench.com/q/L2igH6P-IVdiwrTdVpuEzkzoH5Q
   // Quick Bench for generating bounded floats:    https://quick-bench.com/q/GP9Zfw7YOaXteDOztQ_P2kYnmoY
   // Quick Bench for generating bounded integers:  https://quick-bench.com/q/6hjHn7fpVdaEmp37BcsXyW4A3K4

template<class Engine>
class RandomTypedTest : public ::testing::Test{
protected:
    using Rng = rnd::Random<Engine>;
    Rng rng{}; // default-seeded
};

template<class Rng, class Range>
concept CanGetRandomIterator = requires(Rng& rng, Range&& range){
    rng.iterator(std::forward<Range>(range));
};

template<class Rng, class Range>
concept CanGetRandomElement = requires(Rng& rng, Range&& range){
    rng.element(std::forward<Range>(range));
};

template<class Rng, class Range>
concept CanGetWeightedIndex = requires(Rng& rng, Range&& range){
    rng.weighted_index(std::forward<Range>(range));
};

template<class Rng, class Weight>
concept CanGetWeightedIndexFromPointer = requires(Rng& rng, const Weight* weights){
    rng.weighted_index(weights, std::size_t{1});
};

template<class Rng, class Range, class Projection>
concept CanGetWeightedIterator = requires(Rng& rng, Range&& range, Projection projection){
    rng.weighted_iterator(std::forward<Range>(range), projection);
};

template<class Rng, class Range, class Projection>
concept CanGetWeightedElement = requires(Rng& rng, Range&& range, Projection projection){
    rng.weighted_element(std::forward<Range>(range), projection);
};

template<class Rng, class T, class Projection>
concept CanGetWeightedIteratorFromPointer = requires(
    Rng& rng, T* collection, Projection projection){
    rng.weighted_iterator(collection, std::size_t{1}, projection);
};

struct WeightedValue{
    int value;
    std::uint8_t weight;
};

struct WideWeightedValue{
    int value;
    std::uint64_t weight;
};

struct FloatingWeightProjection{
    constexpr float operator()(const WeightedValue& item) const noexcept{
        return static_cast<float>(item.weight);
    }
};

struct SignedWeightProjection{
    constexpr int operator()(const WeightedValue& item) const noexcept{
        return item.weight;
    }
};

struct ThrowingWeightProjection{
    constexpr std::uint8_t operator()(const WeightedValue& item) const{
        return item.weight;
    }
};

class NonAssignableSeedEngine{
public:
    using result_type = std::uint32_t;
    using seed_type = std::uint64_t;

    constexpr NonAssignableSeedEngine() noexcept = default;
    explicit constexpr NonAssignableSeedEngine(seed_type seed) noexcept
        : state_(static_cast<result_type>(seed)){}
    constexpr NonAssignableSeedEngine(const NonAssignableSeedEngine&) noexcept = default;
    constexpr NonAssignableSeedEngine& operator=(const NonAssignableSeedEngine&) = delete;

    static constexpr result_type min() noexcept{ return 0; }
    static constexpr result_type max() noexcept{ return std::numeric_limits<result_type>::max(); }

    constexpr result_type operator()() noexcept{ return state_++; }
    constexpr void seed() noexcept{ state_ = default_state; }
    constexpr void seed(seed_type seed) noexcept{ state_ = static_cast<result_type>(seed); }
    constexpr void discard(unsigned long long n) noexcept{ state_ += static_cast<result_type>(n); }
    constexpr bool operator==(const NonAssignableSeedEngine&) const noexcept = default;

private:
    static constexpr result_type default_state = 0x12345678u;
    result_type state_ = default_state;
};

class MissingSeedTypeEngine{
public:
    using result_type = std::uint32_t;

    constexpr MissingSeedTypeEngine() noexcept = default;
    explicit constexpr MissingSeedTypeEngine(result_type) noexcept{}
    static constexpr result_type min() noexcept{ return 0; }
    static constexpr result_type max() noexcept{ return std::numeric_limits<result_type>::max(); }
    constexpr result_type operator()() noexcept{ return 0; }
    constexpr void seed() noexcept{}
    constexpr void seed(result_type) noexcept{}
    constexpr void discard(unsigned long long) noexcept{}
    constexpr bool operator==(const MissingSeedTypeEngine&) const noexcept = default;
};

static_assert(RandomBitEngine<NonAssignableSeedEngine>);
static_assert(!RandomBitEngine<MissingSeedTypeEngine>);
static_assert(!std::assignable_from<NonAssignableSeedEngine&, NonAssignableSeedEngine>);
static_assert(!std::is_same_v<PCG32::result_type, PCG32::seed_type>);

template <class Engine>
consteval bool engineStateApiIsConstexpr(){
    using seed_type = typename Engine::seed_type;
    static_assert(std::is_aggregate_v<typename Engine::state_type>);
    static_assert(std::is_trivially_copyable_v<typename Engine::state_type>);
    static_assert(noexcept(std::declval<const Engine&>().state()));
    static_assert(noexcept(Engine::from_state(std::declval<typename Engine::state_type>())));

    Engine original{seed_type{123}};
    for(int i = 0; i < 9; ++i){
        (void) original();
    }
    const auto snapshot = original.state();
    Engine restored = Engine::from_state(snapshot);
    if(!(original == restored)){
        return false;
    }
    for(int i = 0; i < 32; ++i){
        if(original() != restored()){
            return false;
        }
    }
    return true;
}

static_assert(engineStateApiIsConstexpr<RomuDuoJr>());
static_assert(engineStateApiIsConstexpr<Konadare192>());
static_assert(engineStateApiIsConstexpr<PCG32>());
static_assert(engineStateApiIsConstexpr<SmallFast8>());
static_assert(engineStateApiIsConstexpr<SmallFast16>());
static_assert(engineStateApiIsConstexpr<SmallFast32>());
static_assert(engineStateApiIsConstexpr<SmallFast64>());
static_assert(engineStateApiIsConstexpr<XorShift32Star8>());
static_assert(engineStateApiIsConstexpr<Xoshiro256SS>());
static_assert(engineStateApiIsConstexpr<QuarkBurst64>());

consteval bool weightedHelpersAreConstexpr(){
    rnd::Random<PCG32> rng{123u};
    std::array<unsigned, 3> weights{0, 7, 0};    
    std::array<WeightedValue, 3> values{{
        {10, 0},
        {20, 7},
        {30, 0}
    }};

    return rng.weighted_index(weights) == 1 &&        
        rng.weighted_element(values, &WeightedValue::weight).value == 20;
}

static_assert(weightedHelpersAreConstexpr());

using EnginesUnderTest = ::testing::Types<
    RomuDuoJr,
    Konadare192,
    PCG32,
    SmallFast8,
    SmallFast16,
    SmallFast32,
    SmallFast64,
    XorShift32Star8,
    Xoshiro256SS,
    QuarkBurst64    
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
    for(int i = 0; i < 2048; ++i){
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
    for(int i = 0; i < 2048; ++i){
        auto v = this->rng.next(bound);
        EXPECT_LT(v, bound);
    }
}

TYPED_TEST(RandomTypedTest, BoundedOperatorsAreEquivalentAndHandleBoundaryBounds){
    using Rng = rnd::Random<TypeParam>;
    using result_type = typename TypeParam::result_type;

    for(result_type bound : {result_type{1}, result_type{2}, result_type{16}, TypeParam::max()}){
        Rng next_rng{123u};
        Rng call_rng{123u};
        for(int i = 0; i < 64; ++i){
            const auto via_next = next_rng.next(bound);
            EXPECT_EQ(via_next, call_rng(bound));
            EXPECT_LT(via_next, bound);
        }
    }
}

// -----------------------------------------------------------------------------
// next<N, T>() returns values in [0, N)
// -----------------------------------------------------------------------------
TYPED_TEST(RandomTypedTest, NextCompileTimeBoundedRespectsBound){
    constexpr std::uint32_t N = 10;

    for(int i = 0; i < 2048; ++i){
        auto v = this->rng.template next<N, std::uint32_t>();
        EXPECT_LT(v, N);
    }
}

TYPED_TEST(RandomTypedTest, CompileTimeBoundedHandlesOneAndPowerOfTwo){
    for(int i = 0; i < 64; ++i){
        EXPECT_EQ((this->rng.template next<1, std::uint8_t>()), 0u);
        EXPECT_LT((this->rng.template next<16, std::uint8_t>()), 16u);
    }
}

// -----------------------------------------------------------------------------
// min(), max(), and range of next()
// -----------------------------------------------------------------------------
TYPED_TEST(RandomTypedTest, NextRespectsMinMaxRange){
    using Engine = TypeParam;
    using result_type = typename Engine::result_type;

    const result_type lo = this->rng.min();
    const result_type hi = this->rng.max();

    EXPECT_LT(lo, hi);

    for(int i = 0; i < 2048; ++i){
        const result_type v = this->rng.next();
        EXPECT_GE(v, lo);
        EXPECT_LE(v, hi) << "next() must be in [min(), max()]";
    }
}

TYPED_TEST(RandomTypedTest, CallOperatorMatchesNext){
    using Rng = rnd::Random<TypeParam>;
    Rng via_next{123u};
    Rng via_call{123u};
    for(int i = 0; i < 64; ++i){
        EXPECT_EQ(via_next.next(), via_call());
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
// Between(lo, hi) produced values in [lo, hi)
// -----------------------------------------------------------------------------
TYPED_TEST(RandomTypedTest, BetweenProducesExclusiveRange){
    constexpr int lo = -5;
    constexpr int hi = 7;

    for(int i = 0; i < 2048; ++i){
        auto v = this->rng.between(lo, hi);
        EXPECT_GE(v, lo);
        EXPECT_LT(v, hi);
    }
}

TEST(RandomBoundaryTest, BetweenHandlesWideSignedIntRanges){
    rnd::Random<SmallFast32> rng{};
    constexpr auto min = std::numeric_limits<std::int32_t>::min();
    constexpr auto max = std::numeric_limits<std::int32_t>::max();

    for(int i = 0; i < 1024; ++i){
        const auto wide = rng.between(min, max);
        EXPECT_GE(wide, min);
        EXPECT_LT(wide, max);

        const auto near_min = rng.between(min, min + 100);
        EXPECT_GE(near_min, min);
        EXPECT_LT(near_min, min + 100);

        const auto near_max = rng.between(max - 100, max);
        EXPECT_GE(near_max, max - 100);
        EXPECT_LT(near_max, max);
    }
}

TEST(RandomBoundaryTest, BetweenHandlesWideSignedInt64Ranges){
    rnd::Random<SmallFast64> rng{};
    constexpr auto min = std::numeric_limits<std::int64_t>::min();
    constexpr auto max = std::numeric_limits<std::int64_t>::max();

    for(int i = 0; i < 1024; ++i){
        const auto wide = rng.between(min, max);
        EXPECT_GE(wide, min);
        EXPECT_LT(wide, max);

        const auto near_min = rng.between(min, min + 100);
        EXPECT_GE(near_min, min);
        EXPECT_LT(near_min, min + 100);

        const auto near_max = rng.between(max - 100, max);
        EXPECT_GE(near_max, max - 100);
        EXPECT_LT(near_max, max);
    }
}

template <class I>
void expectSignedBetweenBoundaryRanges(){
    rnd::Random<SmallFast64> rng{uint64_t{0x123456789abcdef0}};
    const I min = (std::numeric_limits<I>::min)();
    const I max = (std::numeric_limits<I>::max)();

    const auto expect_range = [&rng](I lo, I hi){
        for(int i = 0; i < 512; ++i){
            const I value = rng.between(lo, hi);
            EXPECT_GE(value, lo);
            EXPECT_LT(value, hi);
        }
    };

    expect_range(min, I{-1});
    expect_range(I{-4}, I{5});
    expect_range(min, static_cast<I>(min + I{4}));
    expect_range(static_cast<I>(max - I{4}), max);
    expect_range(min, max);
}

TEST(RandomBoundaryTest, BetweenHandlesSignedBoundaryRangesForEveryWidth){
    expectSignedBetweenBoundaryRanges<std::int8_t>();
    expectSignedBetweenBoundaryRanges<std::int16_t>();
    expectSignedBetweenBoundaryRanges<std::int32_t>();
    expectSignedBetweenBoundaryRanges<std::int64_t>();
}

TYPED_TEST(RandomTypedTest, BetweenFloatingPointProducesExclusiveRange){
    for(int i = 0; i < 2048; ++i){
        const float v = this->rng.between(-2.5f, 4.25f);
        EXPECT_TRUE(std::isfinite(v));
        EXPECT_GE(v, -2.5f);
        EXPECT_LT(v, 4.25f);
    }
}

// -----------------------------------------------------------------------------
// normalized<F>() in [0, 1), signed_norm<F>() in [-1, 1)
// -----------------------------------------------------------------------------
TYPED_TEST(RandomTypedTest, NormalizedProducesFloatInUnitInterval){
    for(int i = 0; i < 2048; ++i){
        float f = this->rng.template normalized<float>();
        EXPECT_TRUE(std::isfinite(f));
        EXPECT_GE(f, 0.0f);
        EXPECT_LT(f, 1.0f);
    }
}

TYPED_TEST(RandomTypedTest, NormalizedProducesDoubleInUnitInterval){
    for(int i = 0; i < 2048; ++i){
        const double value = this->rng.template normalized<double>();
        EXPECT_TRUE(std::isfinite(value));
        EXPECT_GE(value, 0.0);
        EXPECT_LT(value, 1.0);
    }
}

TYPED_TEST(RandomTypedTest, SignedNormProducesFloatInSignedUnitInterval){
    for(int i = 0; i < 2048; ++i){
        float f = this->rng.template signed_norm<float>();
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

TYPED_TEST(RandomTypedTest, CoinFlipUsesHighBit){
    using Rng = rnd::Random<TypeParam>;
    using result_type = typename TypeParam::result_type;
    constexpr auto shift = std::numeric_limits<result_type>::digits - 1;
    Rng coin_rng{123u};
    Rng raw_rng{123u};

    for(int i = 0; i < 256; ++i){
        const bool expected = ((raw_rng.next() >> shift) & 1u) != 0;
        EXPECT_EQ(coin_rng.coin_flip(), expected);
    }
}

TYPED_TEST(RandomTypedTest, CoinFlipProbabilityExtremes){
    // p = 0 -> always false over a reasonable sample
    bool any_true = false;
    for(int i = 0; i < 512; ++i){
        if(this->rng.coin_flip(0.0)){ //note: float, to support 32-bit engines
            any_true = true;
            break;
        }
    }
    EXPECT_FALSE(any_true);

    // p = 1 -> always true over a reasonable sample
    bool any_false = false;
    for(int i = 0; i < 512; ++i){
        if(!this->rng.coin_flip(1.0)){  //note: float, to support 32-bit engines
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

TYPED_TEST(RandomTypedTest, EngineAccessorsPreserveConstnessAndExposeState){
    using Rng = rnd::Random<TypeParam>;
    static_assert(std::is_same_v<decltype(std::declval<Rng&>().engine()), TypeParam&>);
    static_assert(std::is_same_v<decltype(std::declval<const Rng&>().engine()), const TypeParam&>);

    Rng wrapped{123u};
    Rng expected{123u};
    wrapped.engine()();
    expected.next();
    EXPECT_EQ(wrapped, expected);
}

TYPED_TEST(RandomTypedTest, StateSnapshotRoundTripReproducesFutureSequence){
    using Engine = TypeParam;
    using Rng = rnd::Random<Engine>;
    using seed_type = typename Engine::seed_type;

    Engine original{seed_type{123}};
    original.discard(17);
    const auto snapshot = original.state();
    Engine restored = Engine::from_state(snapshot);

    EXPECT_EQ(original, restored);
    for(int i = 0; i < 128; ++i){
        EXPECT_EQ(original(), restored());
    }

    Rng wrapped{Engine::from_state(snapshot)};
    Rng replay{Engine::from_state(snapshot)};
    for(int i = 0; i < 64; ++i){
        EXPECT_EQ(wrapped.next(), replay.next());
    }
}


// -----------------------------------------------------------------------------
// Reproducibility with explicit seed
// -----------------------------------------------------------------------------
TYPED_TEST(RandomTypedTest, SameSeedProducesSameSequence){
    using Engine = TypeParam;
    using Rng = rnd::Random<Engine>;

    auto seed = typename Engine::seed_type{123u};

    Rng a{seed};
    Rng b{seed};

    for(int i = 0; i < 2048; ++i){
        auto va = a.next();
        auto vb = b.next();
        EXPECT_EQ(va, vb);
    }
}

TYPED_TEST(RandomTypedTest, DifferentSeedsProduceDifferentSequences){
    using Engine = TypeParam;
    using Rng = rnd::Random<Engine>;

    Rng a{123u};
    Rng b{231u};

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

TEST(RandomDiscardTest, AdvancesPcg32BeyondItsResultTypeWidth){
    rnd::Random<PCG32> advanced{std::uint64_t{123}};
    const rnd::Random<PCG32> original = advanced;

    advanced.discard(1ull << 32);

    EXPECT_FALSE(advanced == original);
}

TYPED_TEST(RandomTypedTest, SeedWithoutValueRestoresDefaultState){
    using Rng = rnd::Random<TypeParam>;
    Rng rng{123u};
    for(int i = 0; i < 17; ++i) rng.next();
    rng.seed();

    Rng default_rng{};
    for(int i = 0; i < 64; ++i){
        EXPECT_EQ(rng.next(), default_rng.next());
    }
}

TEST(RandomSeedTest, SeedWithoutValueSupportsNonAssignableEngines){
    rnd::Random<NonAssignableSeedEngine> rng{std::uint64_t{42}};
    rng.next();
    rng.seed();

    rnd::Random<NonAssignableSeedEngine> default_rng{};
    EXPECT_EQ(rng.next(), default_rng.next());
}

// -----------------------------------------------------------------------------
// bits(n) and bits<N, T>() only set the requested bits
// -----------------------------------------------------------------------------
TYPED_TEST(RandomTypedTest, BitsRuntimeReturnsOnlyRequestedBits){
    using Engine = TypeParam;
    using result_type = typename Engine::result_type;

    for(unsigned n : {1u, 8u}){
        result_type v = this->rng.bits(n);
        const auto max_val = n == std::numeric_limits<result_type>::digits
            ? std::numeric_limits<result_type>::max()
            : static_cast<result_type>((result_type{1} << n) - 1);
        EXPECT_LE(v, max_val)
            << "bits(" << n << ") must be in [0, 2^n)";
    }

    if constexpr(std::numeric_limits<result_type>::digits >= 16){
        result_type v = this->rng.bits(16);
        EXPECT_LE(v, static_cast<result_type>(0xFFFFu));
    }
}

TYPED_TEST(RandomTypedTest, BitsCompileTimeReturnsOnlyRequestedBits){
    // 8 bits fit in uint16_t, and 8 <= digits(result_type) for all tested engines
    auto v = this->rng.template bits<8, std::uint16_t>();
    EXPECT_LE(v, std::uint16_t{0xFF});
}

TYPED_TEST(RandomTypedTest, BitsUseHighBitsAndRuntimeMatchesCompileTime){
    using Rng = rnd::Random<TypeParam>;
    using result_type = typename TypeParam::result_type;
    constexpr unsigned width = std::numeric_limits<result_type>::digits;

    Rng raw_rng{123u};
    Rng runtime_rng{123u};
    Rng compile_rng{123u};
    const result_type expected = raw_rng.next() >> (width - 8);
    EXPECT_EQ(runtime_rng.template bits<result_type>(8), expected);
    EXPECT_EQ((compile_rng.template bits<8, result_type>()), expected);
}

TYPED_TEST(RandomTypedTest, BitsCanFillATypeWiderThanEngineOutput){
    using Rng = rnd::Random<TypeParam>;
    using result_type = typename TypeParam::result_type;
    Rng bits_rng{123u};
    Rng expected_rng{123u};

    constexpr unsigned width = std::numeric_limits<result_type>::digits;
    std::uint64_t expected{};
    for(unsigned filled = 0; filled < 64; filled += width){
        expected |= std::uint64_t{expected_rng.next()} << filled;
    }
    EXPECT_EQ(bits_rng.template bits_as<std::uint64_t>(), expected);
    EXPECT_EQ(bits_rng, expected_rng) << "bits_as must consume exactly enough engine outputs";
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

    const typename Engine::seed_type seed_val{123u};

    Rng a{seed_val};
    Rng b{};
    b.seed(seed_val);

    for(int i = 0; i < 16; ++i){
        EXPECT_EQ(a.next(), b.next());
    }
}


// -----------------------------------------------------------------------------
// Collection helpers, normal_approx(), and child()
// -----------------------------------------------------------------------------
TYPED_TEST(RandomTypedTest, CollectionHelpersReturnValidMutableAndConstElements){
    using Rng = rnd::Random<TypeParam>;

    static_assert(CanGetRandomIterator<Rng, std::vector<int>&>);
    static_assert(CanGetRandomElement<Rng, std::vector<int>&>);
    static_assert(!CanGetRandomIterator<Rng, std::vector<int>>);
    static_assert(!CanGetRandomElement<Rng, std::vector<int>>);
    static_assert(CanGetRandomIterator<Rng, std::span<int>&>);
    static_assert(CanGetRandomElement<Rng, std::span<int>&>);
    static_assert(!CanGetRandomIterator<Rng, std::span<int>>);
    static_assert(!CanGetRandomElement<Rng, std::span<int>>);
    static_assert(!CanGetRandomIterator<Rng, std::list<int>&>);
    static_assert(!CanGetRandomElement<Rng, std::list<int>&>);

    std::array<int, 5> values{10, 20, 30, 40, 50};
    const auto& const_values = values;

    static_assert(std::is_same_v<decltype(this->rng.element(values)), int&>);
    static_assert(std::is_same_v<decltype(this->rng.element(const_values)), const int&>);

    std::vector<int> vector_values{10, 20, 30};
    const auto& const_vector_values = vector_values;
    static_assert(std::is_same_v<decltype(this->rng.iterator(vector_values)),
        std::vector<int>::iterator>);
    static_assert(std::is_same_v<decltype(this->rng.iterator(const_vector_values)),
        std::vector<int>::const_iterator>);
    static_assert(std::random_access_iterator<decltype(this->rng.iterator(vector_values))>);

    for(int i = 0; i < 128; ++i){
        const auto idx = this->rng.index(values);
        EXPECT_LT(idx, values.size());

        const auto it = this->rng.iterator(values);
        EXPECT_GE(std::distance(values.begin(), it), 0);
        EXPECT_LT(std::distance(values.begin(), it), static_cast<std::ptrdiff_t>(values.size()));

        int& element = this->rng.element(values);
        EXPECT_GE(&element, values.data());
        EXPECT_LT(&element, values.data() + values.size());

        int* pointer = this->rng.iterator(values.data(), values.size());
        EXPECT_GE(pointer, values.data());
        EXPECT_LT(pointer, values.data() + values.size());

        int& pointer_element = this->rng.element(values.data(), values.size());
        EXPECT_GE(&pointer_element, values.data());
        EXPECT_LT(&pointer_element, values.data() + values.size());
    }
}

TEST(RandomBoundaryTest, NarrowEngineSupportsWiderIntegerRequestsAndCollections){
    using Rng = rnd::Random<SmallFast8>;
    Rng rng{std::uint8_t{123}};

    static_assert(std::is_same_v<decltype(rng.next(std::uint16_t{300})), std::uint16_t>);
    static_assert(std::is_same_v<decltype(rng.next(std::uint64_t{300})), std::uint64_t>);
    static_assert(std::is_same_v<decltype(rng.next<1000>()), std::uint16_t>);

    for(int i = 0; i < 256; ++i){
        EXPECT_LT(rng.next(std::uint16_t{300}), std::uint16_t{300});
        EXPECT_LT(rng.next(std::uint64_t{1000000}), std::uint64_t{1000000});
        EXPECT_GE(rng.between(std::uint32_t{1000}, std::uint32_t{1300}), std::uint32_t{1000});
        EXPECT_LT(rng.between(std::uint32_t{1000}, std::uint32_t{1300}), std::uint32_t{1300});
        EXPECT_LT(rng.index(std::size_t{300}), std::size_t{300});
    }

    EXPECT_LT((rng.next<300, std::uint16_t>()), std::uint16_t{300});
    EXPECT_LT(rng.next<1000>(), std::uint16_t{1000});

    std::array<int, 300> values{};
    for(int i = 0; i < 64; ++i){
        EXPECT_LT(rng.index(values), values.size());
    }
}

TYPED_TEST(RandomTypedTest, WeightedHelpersHaveSafeContiguousCollectionAndWeightConstraints){
    using Rng = rnd::Random<TypeParam>;

    static_assert(CanGetWeightedIndexFromPointer<Rng, unsigned>);
    static_assert(!CanGetWeightedIndexFromPointer<Rng, int>);
    static_assert(CanGetWeightedIndex<Rng, std::array<std::uint8_t, 3>>);
    static_assert(!CanGetWeightedIndexFromPointer<Rng, float>);
    static_assert(CanGetWeightedIndexFromPointer<Rng, std::uint64_t>);

    static_assert(CanGetWeightedIterator<
        Rng, std::vector<WeightedValue>&, decltype(&WeightedValue::weight)>);
    static_assert(CanGetWeightedElement<
        Rng, std::span<WeightedValue>&, decltype(&WeightedValue::weight)>);
    static_assert(!CanGetWeightedIterator<
        Rng, std::vector<WeightedValue>, decltype(&WeightedValue::weight)>);
    static_assert(!CanGetWeightedElement<
        Rng, std::vector<WeightedValue>, decltype(&WeightedValue::weight)>);
    static_assert(!CanGetWeightedIterator<
        Rng, std::list<WeightedValue>&, decltype(&WeightedValue::weight)>);
    static_assert(!CanGetWeightedIteratorFromPointer<
        Rng, WeightedValue, FloatingWeightProjection>);
    static_assert(!CanGetWeightedIteratorFromPointer<
        Rng, WeightedValue, SignedWeightProjection>);
    static_assert(!CanGetWeightedIteratorFromPointer<
        Rng, WeightedValue, ThrowingWeightProjection>);

    std::array<WeightedValue, 3> values{{
        {10, 0},
        {20, 1},
        {30, 0}
    }};
    const auto& const_values = values;

    static_assert(std::is_same_v<decltype(this->rng.weighted_element(
        values, &WeightedValue::weight)), WeightedValue&>);
    static_assert(std::is_same_v<decltype(this->rng.weighted_element(
        const_values, &WeightedValue::weight)), const WeightedValue&>);

    EXPECT_EQ(this->rng.weighted_iterator(values, &WeightedValue::weight), values.begin() + 1);
    EXPECT_EQ(this->rng.weighted_iterator(
        values.data(), values.size(), &WeightedValue::weight), values.data() + 1);
    const std::uint8_t weights[]{0, 1, 0};
    EXPECT_EQ(this->rng.weighted_index(weights, std::size(weights)), std::size_t{1});
    WeightedValue& selected = this->rng.weighted_element(values, &WeightedValue::weight);
    EXPECT_EQ(&selected, &values[1]);
}

TYPED_TEST(RandomTypedTest, WeightedIndexMatchesTheUnderlyingBoundedDraw){
    using Rng = rnd::Random<TypeParam>;
    using result_type = typename TypeParam::result_type;

    Rng weighted_rng{123u};
    Rng target_rng{123u};
    const std::array<std::uint8_t, 4> weights{0, 2, 5, 3};

    for(int i = 0; i < 256; ++i){
        const result_type target = target_rng.next(result_type{10});
        const std::size_t expected = target < 2 ? 1 : target < 7 ? 2 : 3;
        EXPECT_EQ(weighted_rng.weighted_index(weights), expected);
    }
}

TYPED_TEST(RandomTypedTest, WeightedIteratorMatchesTheUnderlyingBoundedDraw){
    using Rng = rnd::Random<TypeParam>;
    using result_type = typename TypeParam::result_type;

    Rng weighted_rng{231u};
    Rng target_rng{231u};
    std::array<WeightedValue, 4> values{{
        {10, 0},
        {20, 2},
        {30, 5},
        {40, 3}
    }};

    for(int i = 0; i < 256; ++i){
        const result_type target = target_rng.next(result_type{10});
        const std::size_t expected = target < 2 ? 1 : target < 7 ? 2 : 3;
        EXPECT_EQ(
            weighted_rng.weighted_iterator(values, &WeightedValue::weight),
            values.begin() + static_cast<std::ptrdiff_t>(expected)
        );
    }
}

TYPED_TEST(RandomTypedTest, WeightedIndexAccumulatesWithoutNarrowingOrOverflow){
    using Rng = rnd::Random<TypeParam>;
    using result_type = typename TypeParam::result_type;

    if constexpr(std::numeric_limits<result_type>::digits < 16){
        Rng weighted_rng{123u};
        Rng target_rng{123u};
        const std::array<std::uint8_t, 2> weights{200, 55};

        for(int i = 0; i < 64; ++i){
            const result_type target = target_rng.next(result_type{255});
            const std::size_t expected = target < 200 ? 0 : 1;
            EXPECT_EQ(weighted_rng.weighted_index(weights), expected);
        }
    } else{
        Rng weighted_rng{789u};
        Rng target_rng{789u};
        const std::array<std::uint8_t, 2> weights{200, 100};

        for(int i = 0; i < 64; ++i){
            const result_type target = target_rng.next(result_type{300});
            const std::size_t expected = target < 200 ? 0 : 1;
            EXPECT_EQ(weighted_rng.weighted_index(weights), expected);
        }
    }
}

TEST(RandomBoundaryTest, NarrowEngineAcceptsWideWeightsAndTotals){
    using Rng = rnd::Random<SmallFast8>;
    const std::array<std::uint16_t, 2> weights{{200, 100}};

    Rng weighted_rng{std::uint8_t{123}};
    Rng target_rng{std::uint8_t{123}};
    for(int i = 0; i < 256; ++i){
        const std::uint64_t target = target_rng.next(std::uint64_t{300});
        const std::size_t expected = target < 200 ? 0 : 1;
        EXPECT_EQ(weighted_rng.weighted_index(weights), expected);
    }

    const std::array<WideWeightedValue, 2> values{{
        {10, 200},
        {20, 100}
    }};
    Rng projected_rng{std::uint8_t{123}};
    Rng projected_target_rng{std::uint8_t{123}};
    for(int i = 0; i < 64; ++i){
        const std::uint64_t target = projected_target_rng.next(std::uint64_t{300});
        const std::size_t expected = target < 200 ? 0 : 1;
        EXPECT_EQ(
            projected_rng.weighted_element(values, &WideWeightedValue::weight).value,
            values[expected].value
        );
    }
}

TYPED_TEST(RandomTypedTest, NormalApproxWithZeroDeviationReturnsMean){
    for(int i = 0; i < 32; ++i){
        EXPECT_EQ(this->rng.normal_approx(12.5f, 0.0f), 12.5f);
    }
}

TYPED_TEST(RandomTypedTest, ChildIsDeterministic){
    using Engine = TypeParam;
    using Rng = rnd::Random<Engine>;
    
    Rng a{123u};
    Rng b{123u};

    EXPECT_EQ(a.child(), b.child());
    EXPECT_EQ(a, b);
}

TYPED_TEST(RandomTypedTest, ConsecutiveChildrenEventuallyDiffer){
    using Engine = TypeParam;
    using Rng = rnd::Random<Engine>;
    
    Rng parent{123u};

    auto first = parent.child();
    bool all_equal = true;
    for(int i = 0; i < 7; ++i){
        if(parent.child() != first){
            all_equal = false;
            break;
        }
    }

    EXPECT_FALSE(all_equal);
}

TYPED_TEST(RandomTypedTest, ChildAdvancesParent){
    using Engine = TypeParam;
    using Rng = rnd::Random<Engine>;
    
    Rng parent{123u};
    Rng original = parent;

    [[maybe_unused]] auto child = parent.child();

    EXPECT_FALSE(parent == original);
}

TEST(EngineSpecificRandomTest, PCG32SplitRemainsAnEngineSpecificOperation){
    PCG32 a{123u};
    PCG32 b{123u};

    const auto child_a = a.split();
    const auto child_b = b.split();

    EXPECT_EQ(child_a, child_b);
    EXPECT_EQ(a, b);
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
    Xoshiro256SS,
    QuarkBurst64    
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
