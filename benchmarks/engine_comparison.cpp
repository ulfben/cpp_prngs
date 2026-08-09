// Compare the raw throughput of the engines available to cpp_prngs.
//
// Build this file against Google Benchmark locally, or run
// generate-quickbench.ps1 to produce self-contained source files for
// https://quick-bench.com/.

#include "../includes/engines/konadare192.hpp"
#include "../includes/engines/pcg32.hpp"
#include "../includes/engines/quarkburst64.hpp"
#include "../includes/engines/romuduojr.hpp"
#include "../includes/engines/small_fast8.hpp"
#include "../includes/engines/small_fast16.hpp"
#include "../includes/engines/small_fast32.hpp"
#include "../includes/engines/small_fast64.hpp"
#include "../includes/engines/xorshift32star8.hpp"
#include "../includes/engines/xoshiro256ss.hpp"

#include <benchmark/benchmark.h>
#include <cstdint>
#include <cstdlib>
#include <random>

namespace {

constexpr std::int64_t values_per_iteration = 1024;

template<class Engine>
void BM_EngineNext(benchmark::State& state){
	using seed_type = typename Engine::seed_type;

	Engine rng{static_cast<seed_type>(1234567890ULL)};
	std::uint64_t sum = 0;

	for([[maybe_unused]] auto _ : state){
		for(std::int64_t i = 0; i < values_per_iteration; ++i){
			sum += static_cast<std::uint64_t>(rng.next());
		}
	}

	benchmark::DoNotOptimize(sum);
	state.SetItemsProcessed(state.iterations() * values_per_iteration);
}

BENCHMARK_TEMPLATE(BM_EngineNext, PCG32);
BENCHMARK_TEMPLATE(BM_EngineNext, SmallFast8);
BENCHMARK_TEMPLATE(BM_EngineNext, SmallFast16);
BENCHMARK_TEMPLATE(BM_EngineNext, SmallFast32);
BENCHMARK_TEMPLATE(BM_EngineNext, SmallFast64);
BENCHMARK_TEMPLATE(BM_EngineNext, XorShift32Star8);
BENCHMARK_TEMPLATE(BM_EngineNext, Xoshiro256SS);
BENCHMARK_TEMPLATE(BM_EngineNext, RomuDuoJr);
BENCHMARK_TEMPLATE(BM_EngineNext, Konadare192);
BENCHMARK_TEMPLATE(BM_EngineNext, QuarkBurst64);

template<class Engine>
void BM_StandardEngine(benchmark::State& state){
	Engine rng{0};
	std::uint64_t sum = 0;

	for([[maybe_unused]] auto _ : state){
		for(std::int64_t i = 0; i < values_per_iteration; ++i){
			sum += static_cast<std::uint64_t>(rng());
		}
	}

	benchmark::DoNotOptimize(sum);
	state.SetItemsProcessed(state.iterations() * values_per_iteration);
}

BENCHMARK_TEMPLATE(BM_StandardEngine, std::mt19937);
BENCHMARK_TEMPLATE(BM_StandardEngine, std::default_random_engine);
BENCHMARK_TEMPLATE(BM_StandardEngine, std::minstd_rand);

void BM_CstdlibRand(benchmark::State& state){
	std::srand(0);
	std::uint64_t sum = 0;

	for([[maybe_unused]] auto _ : state){
		for(std::int64_t i = 0; i < values_per_iteration; ++i){
			sum += static_cast<std::uint64_t>(std::rand());
		}
	}

	benchmark::DoNotOptimize(sum);
	state.SetItemsProcessed(state.iterations() * values_per_iteration);
}

BENCHMARK(BM_CstdlibRand);

} // namespace
