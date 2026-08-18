#include <random.hpp>
#include <array>
#include <stdint.h>

enum class UnsupportedInteger : uint8_t{};

static_assert(rnd::detail::supported_uint<uint8_t>);
static_assert(rnd::detail::supported_uint<uint16_t>);
static_assert(rnd::detail::supported_uint<uint32_t>);
static_assert(rnd::detail::supported_uint<const volatile uint64_t&>);
static_assert(!rnd::detail::supported_uint<int8_t>);
static_assert(!rnd::detail::supported_uint<bool>);

static_assert(rnd::detail::supported_integer<int8_t>);
static_assert(rnd::detail::supported_integer<int16_t>);
static_assert(rnd::detail::supported_integer<const volatile int32_t&>);
static_assert(rnd::detail::supported_integer<int64_t>);
static_assert(rnd::detail::supported_integer<uint64_t>);
static_assert(!rnd::detail::supported_integer<bool>);
static_assert(!rnd::detail::supported_integer<char>);
static_assert(!rnd::detail::supported_integer<UnsupportedInteger>);

static_assert(rnd::detail::supported_float<float>);
static_assert(rnd::detail::supported_float<const double&>);
static_assert(!rnd::detail::supported_float<long double>);

// These are the tiny standard-library substitutes used by the shared header.
static_assert(rnd::detail::bit_width<uint8_t>() == 8);
static_assert(rnd::detail::bit_width<uint64_t>() == 64);
static_assert(rnd::detail::integral_max<int8_t>() == INT8_MAX);
static_assert(rnd::detail::integral_max<uint64_t>() == UINT64_MAX);
static_assert(rnd::detail::power_of_two_exponent(uint8_t{1}) == 0);
static_assert(rnd::detail::power_of_two_exponent(uint16_t{1024}) == 10);

template <class UInt>
class CountingEngine final{
public:
	using result_type = UInt;
	using seed_type = UInt;

	constexpr CountingEngine() noexcept = default;
	explicit constexpr CountingEngine(seed_type value) noexcept : _value(value){}

	static constexpr result_type (min)() noexcept{ return result_type{0}; }
	static constexpr result_type (max)() noexcept{ return static_cast<result_type>(~result_type{0}); }

	constexpr result_type operator()() noexcept{
		++draws;
		return _value++;
	}
	constexpr void seed() noexcept{ *this = CountingEngine{}; }
	constexpr void seed(seed_type value) noexcept{ *this = CountingEngine{value}; }
	constexpr void discard(unsigned long long count) noexcept{
		while(count--){
			operator()();
		}
	}
	constexpr bool operator==(const CountingEngine& rhs) const noexcept{
		return _value == rhs._value && draws == rhs.draws;
	}
	constexpr bool operator!=(const CountingEngine& rhs) const noexcept{ return !(*this == rhs); }

	unsigned draws{};

private:
	result_type _value{};
};

using Engine8 = CountingEngine<uint8_t>;
using Engine16 = CountingEngine<uint16_t>;
using Engine32 = CountingEngine<uint32_t>;
using Engine64 = CountingEngine<uint64_t>;

struct LootDrop{
	int id;
	uint8_t weight;
	constexpr uint8_t get_weight() const noexcept{ return weight; }
};

struct WeightProjection{
	constexpr uint8_t operator()(const LootDrop& drop) const noexcept{ return drop.weight; }
};

struct ThrowingWeightProjection{
	uint8_t operator()(const LootDrop& drop) const{ return drop.weight; }
};

static_assert(noexcept(rnd::detail::invoke(
	*static_cast<WeightProjection*>(nullptr),
	*static_cast<LootDrop*>(nullptr))));
static_assert(!noexcept(rnd::detail::invoke(
	*static_cast<ThrowingWeightProjection*>(nullptr),
	*static_cast<LootDrop*>(nullptr))));

constexpr bool validate_integer_api(){
	rnd::Random<Engine8> bounded{uint8_t{129}};
	if(bounded.next(uint8_t{100}) != uint8_t{50} || bounded.engine().draws != 1){
		return false;
	}

	// The first source value is rejected for this bound. This verifies both the
	// rejection step and that the redraw consumes exactly one additional value.
	rnd::Random<Engine8> rejected{uint8_t{0}};
	if(rejected.next(uint8_t{10}) != uint8_t{0} || rejected.engine().draws != 2){
		return false;
	}

	// One full 8-bit source cycle contains exactly 250 accepted values for
	// bound 10. Each of the ten outputs must therefore occur 25 times.
	rnd::Random<Engine8> exhaustive{uint8_t{0}};
	unsigned counts[10]{};
	for(unsigned i = 0; i < 250; ++i){
		++counts[exhaustive.next(uint8_t{10})];
	}
	for(const unsigned count : counts){
		if(count != 25){
			return false;
		}
	}
	if(exhaustive.engine().draws != 256){
		return false;
	}

	// For bound 129, the old multiply-high-only mapping gives two outcomes
	// twice the probability of the others. Rejection leaves exactly 129
	// accepted source values in one 8-bit cycle, one for each output.
	rnd::Random<Engine8> extreme{uint8_t{0}};
	unsigned extreme_counts[129]{};
	for(unsigned i = 0; i < 129; ++i){
		++extreme_counts[extreme.next(uint8_t{129})];
	}
	for(const unsigned count : extreme_counts){
		if(count != 1){
			return false;
		}
	}
	if(extreme.engine().draws != 256){
		return false;
	}

	rnd::Random<Engine8> compile_time_bound{uint8_t{0xb0}};
	if(compile_time_bound.next<16, uint8_t>() != uint8_t{11}){
		return false;
	}
	compile_time_bound.seed(uint8_t{0xb0});
	if(compile_time_bound.next<16, int16_t>() != int16_t{11}){
		return false;
	}
	compile_time_bound.seed(uint8_t{128});
	if(compile_time_bound.next<10, uint8_t>() != uint8_t{5}){
		return false;
	}

	rnd::Random<Engine8> bits{uint8_t{0x12}};
	if(bits.bits_as<uint16_t>() != uint16_t{0x1312} || bits.engine().draws != 2){
		return false;
	}

	rnd::Random<Engine32> byte_stream{UINT32_C(0x12345678)};
	uint8_t bytes[4]{};
	byte_stream.fill_bits(bytes, 4);
	if(bytes[0] != uint8_t{0x12} || bytes[1] != uint8_t{0x34} ||
		bytes[2] != uint8_t{0x56} || bytes[3] != uint8_t{0x78} ||
		byte_stream.engine().draws != 1){
		return false;
	}

	rnd::Random<Engine32> wide_stream{UINT32_C(0x12345678)};
	uint64_t wide_value[1]{};
	wide_stream.fill_bits(wide_value, 1);
	if(wide_value[0] != UINT64_C(0x1234567812345679) ||
		wide_stream.engine().draws != 2){
		return false;
	}

	rnd::Random<Engine64> narrow_stream{UINT64_C(0x123456789abcdef0)};
	uint16_t narrow_values[4]{};
	narrow_stream.fill_bits(narrow_values, 4);
	if(narrow_values[0] != uint16_t{0x1234} || narrow_values[1] != uint16_t{0x5678} ||
		narrow_values[2] != uint16_t{0x9abc} || narrow_values[3] != uint16_t{0xdef0} ||
		narrow_stream.engine().draws != 1){
		return false;
	}

	rnd::Random<Engine8> ranges{uint8_t{128}};
	if(ranges.between(int16_t{-10}, int16_t{10}) != int16_t{0}){
		return false;
	}

	rnd::Random<Engine16> native_16{uint16_t{32769}};
	if(native_16.next(uint16_t{1000}) != uint16_t{500} || native_16.engine().draws != 1){
		return false;
	}

	rnd::Random<Engine32> native_32{uint32_t{0x80000001}};
	if(native_32.next(uint32_t{1000}) != uint32_t{500} || native_32.engine().draws != 1){
		return false;
	}

	rnd::Random<Engine64> native_64{uint64_t{0x8000000000000001}};
	if(native_64.next(uint64_t{1000}) != uint64_t{500} || native_64.engine().draws != 1){
		return false;
	}

	rnd::Random<Engine64> maximum_64{UINT64_MAX};
	return maximum_64.next(UINT64_MAX) == UINT64_MAX - 1;
}

constexpr bool validate_collections(){
	int values[]{10, 20, 30, 40};
	rnd::Random<Engine8> random{uint8_t{192}};
	if(random.index(values) != 3){
		return false;
	}

	random.seed(uint8_t{192});
	if(random.index(size_t{4}) != 3){
		return false;
	}

	random.seed(uint8_t{192});
	if(random.iterator(values) != values + 3){
		return false;
	}

	random.seed(uint8_t{192});
	if(random.element(values, 4) != 40){
		return false;
	}

	const int constants[]{1, 2, 3, 4};
	random.seed(uint8_t{64});
	const int* selected = random.iterator(constants);
	if(selected != constants + 1){
		return false;
	}

	std::array<int, 4> array_values{{10, 20, 30, 40}};
	random.seed(uint8_t{192});
	if(random.iterator(array_values) != array_values.begin() + 3){
		return false;
	}

	const std::array<int, 4> const_array_values{{10, 20, 30, 40}};
	random.seed(uint8_t{64});
	return &random.element(const_array_values) == const_array_values.data() + 1;
}

constexpr bool validate_weighted_collections(){
	const uint8_t weights[]{0, 2, 0, 6};
	rnd::Random<Engine8> random{uint8_t{0}};
	if(random.weighted_index(weights) != 1){
		return false;
	}

	random.seed(uint8_t{64});
	if(random.weighted_index(weights, 4) != 3){
		return false;
	}

	const std::array<uint8_t, 4> array_weights{{0, 2, 0, 6}};
	random.seed(uint8_t{64});
	if(random.weighted_index(array_weights) != 3){
		return false;
	}

	LootDrop loot[]{
		{10, 0},
		{20, 2},
		{30, 0},
		{40, 6}
	};
	random.seed(uint8_t{0});
	if(random.weighted_iterator(loot, &LootDrop::weight) != loot + 1){
		return false;
	}

	random.seed(uint8_t{64});
	if(random.weighted_element(loot, 4, &LootDrop::get_weight).id != 40){
		return false;
	}
	random.seed(uint8_t{0});
	if(random.weighted_element(loot, WeightProjection{}).id != 20){
		return false;
	}

	const LootDrop constant_loot[]{
		{10, 0},
		{20, 2},
		{30, 0},
		{40, 6}
	};
	random.seed(uint8_t{0});
	const LootDrop* selected = random.weighted_iterator(constant_loot, &LootDrop::weight);
	if(selected != constant_loot + 1){
		return false;
	}

	std::array<LootDrop, 4> array_loot{{
		{10, 0},
		{20, 2},
		{30, 0},
		{40, 6}
	}};
	random.seed(uint8_t{0});
	return random.weighted_iterator(array_loot, WeightProjection{}) == array_loot.begin() + 1;
}

#ifndef RND_FAST_FLOAT
constexpr bool validate_constexpr_float_api(){
	rnd::Random<Engine32> normalized{UINT32_C(0x80000000)};
	if(normalized.normalized() != 0.5f || normalized.engine().draws != 1){
		return false;
	}

	rnd::Random<Engine32> signed_value{UINT32_C(0x80000000)};
	if(signed_value.signed_norm() != 0.0f){
		return false;
	}

	rnd::Random<Engine32> range{UINT32_C(0x80000000)};
	if(range.between(10.0f, 20.0f) != 15.0f){
		return false;
	}

	rnd::Random<Engine32> weighted_coin{UINT32_C(0x7fffffff)};
	if(!weighted_coin.coin_flip(0.5f)){
		return false;
	}

	rnd::Random<Engine32> gaussian{uint32_t{0}};
	if(gaussian.gaussian(3.0f, 0.0f) != 3.0f){
		return false;
	}

	rnd::Random<Engine64> gaussian_64{uint64_t{0}};
	if(gaussian_64.gaussian(3.0f, 0.0f) != 3.0f){
		return false;
	}

	rnd::Random<Engine16> gaussian_16{uint16_t{0}};
	if(gaussian_16.gaussian(3.0f, 0.0f) != 3.0f){
		return false;
	}

	rnd::Random<Engine8> gaussian_8{uint8_t{0}};
	if(gaussian_8.gaussian(3.0f, 0.0f) != 3.0f){
		return false;
	}

	rnd::Random<Engine32> unit_gaussian{UINT32_C(0x12345678)};
	rnd::Random<Engine32> transformed_gaussian{UINT32_C(0x12345678)};
	const float unit_sample = unit_gaussian.gaussian(0.0f, 1.0f);
	const float transformed_sample = transformed_gaussian.gaussian(3.0f, 2.0f);
	const float expected_sample = 3.0f + unit_sample * 2.0f;
	const float error = transformed_sample > expected_sample ?
		transformed_sample - expected_sample : expected_sample - transformed_sample;
	if(error > 1.0e-5f){
		return false;
	}

	rnd::Random<Engine64> gaussian_double_64{uint64_t{0}};
	if(gaussian_double_64.gaussian(3.0, 0.0) != 3.0){
		return false;
	}

	rnd::Random<Engine32> gaussian_double_32{uint32_t{0}};
	if(gaussian_double_32.gaussian(3.0, 0.0) != 3.0){
		return false;
	}

	rnd::Random<Engine64> normalized_double{UINT64_C(0x8000000000000000)};
	return normalized_double.normalized<double>() == 0.5;
}
#endif

constexpr bool compile_time_validation(){
	if(!validate_integer_api() || !validate_collections() || !validate_weighted_collections()){
		return false;
	}
#ifndef RND_FAST_FLOAT
	if(!validate_constexpr_float_api()){
		return false;
	}
#endif
	return true;
}

bool validate_runtime_float_api(){
	rnd::Random<Engine32> normalized{UINT32_C(0x80000000)};
	if(normalized.normalized() != 0.5f){
		return false;
	}

	rnd::Random<Engine32> range{UINT32_C(0x80000000)};
	if(range.between(-2.0f, 2.0f) != 0.0f){
		return false;
	}

	rnd::Random<Engine32> high_coin{UINT32_C(0x7fffffff)};
	rnd::Random<Engine32> low_coin{UINT32_C(0x80000000)};
	if(!high_coin.coin_flip(0.5f) || low_coin.coin_flip(0.5f)){
		return false;
	}

	rnd::Random<Engine32> gaussian{uint32_t{0}};
	if(gaussian.gaussian(7.0f, 0.0f) != 7.0f){
		return false;
	}

	rnd::Random<Engine32> unit_gaussian{UINT32_C(0x12345678)};
	rnd::Random<Engine32> transformed_gaussian{UINT32_C(0x12345678)};
	const float unit_sample = unit_gaussian.gaussian(0.0f, 1.0f);
	const float transformed_sample = transformed_gaussian.gaussian(3.0f, 2.0f);
	const float expected_sample = 3.0f + unit_sample * 2.0f;
	const float error = transformed_sample > expected_sample ?
		transformed_sample - expected_sample : expected_sample - transformed_sample;
	if(error > 1.0e-5f){
		return false;
	}

	rnd::Random<Engine64> normalized_double{UINT64_C(0x8000000000000000)};
	return normalized_double.normalized<double>() == 0.5;
}

static_assert(compile_time_validation(), "random.hpp failed C++17 constexpr validation");

int main(){
	if(!compile_time_validation()){
		return 1;
	}
	if(!validate_runtime_float_api()){
		return 2;
	}

	rnd::Random<Engine8> low_coin{uint8_t{0x7f}};
	rnd::Random<Engine8> high_coin{uint8_t{0x80}};
	if(low_coin.coin_flip() || !high_coin.coin_flip()){
		return 3;
	}

	rnd::Random<Engine8> first{uint8_t{42}};
	rnd::Random<Engine8> second{uint8_t{42}};
	if(first != second || first.next() != second.next()){
		return 4;
	}

	first.discard(3);
	second.next();
	second.next();
	second.next();
	if(first != second){
		return 5;
	}

	return 0;
}
