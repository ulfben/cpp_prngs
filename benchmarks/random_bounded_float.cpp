#include <rnd/random.hpp>
#include <rnd/engines/konadare192.hpp>
#include <rnd/engines/quarkburst64.hpp>
#include <rnd/engines/romuduojr.hpp>

#include <benchmark/benchmark.h>
#include <cstdint>
#include <cstdlib>
#include <random>

using namespace rnd;

namespace {

constexpr std::int64_t values_per_iteration = 1024;
constexpr std::uint64_t seed = 1234567890ULL;
constexpr float lower_bound = -10.0f;
constexpr float upper_bound = 10.0f;

template<class Engine>
void BM_RandomBoundedFloat(benchmark::State& state){
	using seed_type = typename Engine::seed_type;
	rnd::Random<Engine> rng{static_cast<seed_type>(seed)};
	double sum = 0.0;

	for([[maybe_unused]] auto _ : state){
		for(std::int64_t i = 0; i < values_per_iteration; ++i){
			sum += rng.between(lower_bound, upper_bound);
		}
	}

	benchmark::DoNotOptimize(sum);
	state.SetItemsProcessed(state.iterations() * values_per_iteration);
}

template<class Engine>
void BM_UniformRealDistribution(benchmark::State& state){
	Engine rng{static_cast<typename Engine::result_type>(seed)};
	std::uniform_real_distribution<float> distribution{
		lower_bound,
		upper_bound
	};
	double sum = 0.0;

	for([[maybe_unused]] auto _ : state){
		for(std::int64_t i = 0; i < values_per_iteration; ++i){
			sum += distribution(rng);
		}
	}

	benchmark::DoNotOptimize(sum);
	state.SetItemsProcessed(state.iterations() * values_per_iteration);
}

void BM_CstdlibRandScaled(benchmark::State& state){
	std::srand(static_cast<unsigned>(seed));
	double sum = 0.0;
	constexpr double scale =
		1.0 / (static_cast<double>(RAND_MAX) + 1.0);

	for([[maybe_unused]] auto _ : state){
		for(std::int64_t i = 0; i < values_per_iteration; ++i){
			const double normalized =
				static_cast<double>(std::rand()) * scale;
			sum += lower_bound +
				(upper_bound - lower_bound) * normalized;
		}
	}

	benchmark::DoNotOptimize(sum);
	state.SetItemsProcessed(state.iterations() * values_per_iteration);
}

BENCHMARK_TEMPLATE(BM_RandomBoundedFloat, QuarkBurst64);
BENCHMARK_TEMPLATE(BM_RandomBoundedFloat, RomuDuoJr);
BENCHMARK_TEMPLATE(BM_RandomBoundedFloat, Konadare192);
BENCHMARK_TEMPLATE(BM_UniformRealDistribution, std::mt19937_64);
BENCHMARK_TEMPLATE(BM_UniformRealDistribution, std::mt19937);
BENCHMARK_TEMPLATE(BM_UniformRealDistribution, std::minstd_rand);
BENCHMARK(BM_CstdlibRandScaled);

} // namespace
