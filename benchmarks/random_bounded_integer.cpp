#include <rnd/random.hpp>
#include <rnd/engines/konadare192.hpp>
#include <rnd/engines/quarkburst64.hpp>
#include <rnd/engines/romuduojr.hpp>
#include <rnd/engines/small_fast16.hpp>
#include <rnd/engines/small_fast8.hpp>

#include <benchmark/benchmark.h>
#include <cstdint>
#include <cstdlib>
#include <random>

using namespace rnd;

namespace {

constexpr std::int64_t values_per_iteration = 1024;
constexpr std::uint64_t seed = 1234567890ULL;
constexpr std::uint64_t bound = 1000;

template<class Engine>
void BM_RandomBoundedInteger(benchmark::State& state){
	using seed_type = typename Engine::seed_type;
	rnd::Random<Engine> rng{static_cast<seed_type>(seed)};
	std::uint64_t sum = 0;

	for([[maybe_unused]] auto _ : state){
		for(std::int64_t i = 0; i < values_per_iteration; ++i){
			sum += rng.next(bound);
		}
	}

	benchmark::DoNotOptimize(sum);
	state.SetItemsProcessed(state.iterations() * values_per_iteration);
}

template<class Engine>
void BM_UniformIntDistribution(benchmark::State& state){
	Engine rng{static_cast<typename Engine::result_type>(seed)};
	std::uniform_int_distribution<std::uint64_t> distribution{0, bound - 1};
	std::uint64_t sum = 0;

	for([[maybe_unused]] auto _ : state){
		for(std::int64_t i = 0; i < values_per_iteration; ++i){
			sum += distribution(rng);
		}
	}

	benchmark::DoNotOptimize(sum);
	state.SetItemsProcessed(state.iterations() * values_per_iteration);
}

template<class Engine, class U>
void BM_RandomNativeWidthBoundedInteger(benchmark::State& state){
	using seed_type = typename Engine::seed_type;
	rnd::Random<Engine> rng{static_cast<seed_type>(seed)};
	constexpr U native_bound = U{100};
	std::uint64_t sum = 0;

	for([[maybe_unused]] auto _ : state){
		for(std::int64_t i = 0; i < values_per_iteration; ++i){
			sum += rng.next(native_bound);
		}
	}

	benchmark::DoNotOptimize(sum);
	state.SetItemsProcessed(state.iterations() * values_per_iteration);
}

template<class Engine>
void BM_RandomPromotedWidthBoundedInteger(benchmark::State& state){
	using seed_type = typename Engine::seed_type;
	rnd::Random<Engine> rng{static_cast<seed_type>(seed)};
	// Keep the promoted case genuinely wider than the engine for both narrow
	// registrations: 8 -> 16 bits and 16 -> 32 bits.
	constexpr std::uint64_t promoted_bound =
		sizeof(typename Engine::result_type) == sizeof(std::uint8_t) ? 1000 : 70000;
	std::uint64_t sum = 0;

	for([[maybe_unused]] auto _ : state){
		for(std::int64_t i = 0; i < values_per_iteration; ++i){
			sum += rng.next(promoted_bound);
		}
	}

	benchmark::DoNotOptimize(sum);
	state.SetItemsProcessed(state.iterations() * values_per_iteration);
}

void BM_CstdlibRandModulo(benchmark::State& state){
	std::srand(static_cast<unsigned>(seed));
	std::uint64_t sum = 0;

	for([[maybe_unused]] auto _ : state){
		for(std::int64_t i = 0; i < values_per_iteration; ++i){
			sum += static_cast<unsigned>(std::rand()) % bound;
		}
	}

	benchmark::DoNotOptimize(sum);
	state.SetItemsProcessed(state.iterations() * values_per_iteration);
}

BENCHMARK_TEMPLATE(BM_RandomBoundedInteger, QuarkBurst64);
BENCHMARK_TEMPLATE(BM_RandomBoundedInteger, RomuDuoJr);
BENCHMARK_TEMPLATE(BM_RandomBoundedInteger, Konadare192);
BENCHMARK_TEMPLATE2(BM_RandomNativeWidthBoundedInteger, SmallFast8, std::uint8_t);
BENCHMARK_TEMPLATE2(BM_RandomNativeWidthBoundedInteger, SmallFast16, std::uint16_t);
BENCHMARK_TEMPLATE(BM_RandomPromotedWidthBoundedInteger, SmallFast8);
BENCHMARK_TEMPLATE(BM_RandomPromotedWidthBoundedInteger, SmallFast16);
BENCHMARK_TEMPLATE(BM_UniformIntDistribution, std::mt19937_64);
BENCHMARK_TEMPLATE(BM_UniformIntDistribution, std::mt19937);
BENCHMARK_TEMPLATE(BM_UniformIntDistribution, std::minstd_rand);
BENCHMARK(BM_CstdlibRandModulo);

} // namespace
