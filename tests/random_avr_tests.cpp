#include <random_avr.hpp>
#include <stdint.h>

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
	constexpr void discard(unsigned long long count) noexcept{ while(count--) operator()(); }
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

constexpr bool validate_integer_api(){
	rnd::Random<Engine8> bounded{uint8_t{128}};
	if(bounded.next(uint8_t{100}) != uint8_t{50} || bounded.engine().draws != 1) return false;

	rnd::Random<Engine8> compile_time_bound{uint8_t{0xb0}};
	if(compile_time_bound.next<16, uint8_t>() != uint8_t{11}) return false;
	compile_time_bound.seed(uint8_t{0xb0});
	if(compile_time_bound.next<16, int16_t>() != int16_t{11}) return false;
	compile_time_bound.seed(uint8_t{128});
	if(compile_time_bound.next<10, uint8_t>() != uint8_t{5}) return false;

	rnd::Random<Engine8> bits{uint8_t{0x12}};
	if(bits.bits_as<uint16_t>() != uint16_t{0x1312} || bits.engine().draws != 2) return false;

	rnd::Random<Engine8> ranges{uint8_t{128}};
	if(ranges.between(int16_t{-10}, int16_t{10}) != int16_t{0}) return false;

	rnd::Random<Engine16> native_16{uint16_t{32768}};
	if(native_16.next(uint16_t{1000}) != uint16_t{500} || native_16.engine().draws != 1) return false;

	rnd::Random<Engine32> native_32{uint32_t{0x80000000}};
	if(native_32.next(uint32_t{1000}) != uint32_t{500} || native_32.engine().draws != 1) return false;

	rnd::Random<Engine64> native_64{uint64_t{0x8000000000000000}};
	if(native_64.next(uint64_t{1000}) != uint64_t{500} || native_64.engine().draws != 1) return false;

	rnd::Random<Engine64> maximum_64{UINT64_MAX};
	return maximum_64.next(UINT64_MAX) == UINT64_MAX - 1;
}

constexpr bool validate_collections(){
	int values[]{10, 20, 30, 40};
	rnd::Random<Engine8> random{uint8_t{192}};
	if(random.index(values) != 3) return false;

	random.seed(uint8_t{192});
	if(random.index(values, 4) != 3) return false;

	random.seed(uint8_t{192});
	if(random.iterator(values) != values + 3) return false;

	random.seed(uint8_t{192});
	if(random.element(values, 4) != 40) return false;

	const int constants[]{1, 2, 3, 4};
	random.seed(uint8_t{64});
	const int* selected = random.iterator(constants);
	return selected == constants + 1;
}

constexpr bool validate_weighted_collections(){
	const uint8_t weights[]{0, 2, 0, 6};
	rnd::Random<Engine8> random{uint8_t{0}};
	if(random.weighted_index(weights) != 1) return false;

	random.seed(uint8_t{64});
	if(random.weighted_index(weights, 4) != 3) return false;

	LootDrop loot[]{
		{10, 0},
		{20, 2},
		{30, 0},
		{40, 6}
	};
	random.seed(uint8_t{0});
	if(random.weighted_iterator(loot, &LootDrop::weight) != loot + 1) return false;

	random.seed(uint8_t{64});
	if(random.weighted_element(loot, 4, &LootDrop::get_weight).id != 40) return false;
	random.seed(uint8_t{0});
	if(random.weighted_element(loot, WeightProjection{}).id != 20) return false;

	const LootDrop constant_loot[]{
		{10, 0},
		{20, 2},
		{30, 0},
		{40, 6}
	};
	random.seed(uint8_t{0});
	const LootDrop* selected = random.weighted_iterator(constant_loot, &LootDrop::weight);
	return selected == constant_loot + 1;
}

#ifndef RND_AVR_FAST_FLOAT
constexpr bool validate_constexpr_float_api(){
	rnd::Random<Engine32> normalized{UINT32_C(0x80000000)};
	if(normalized.normalized() != 0.5f || normalized.engine().draws != 1) return false;

	rnd::Random<Engine32> signed_value{UINT32_C(0x80000000)};
	if(signed_value.signed_norm() != 0.0f) return false;

	rnd::Random<Engine32> range{UINT32_C(0x80000000)};
	if(range.between(10.0f, 20.0f) != 15.0f) return false;

	rnd::Random<Engine32> weighted_coin{UINT32_C(0x7fffffff)};
	if(!weighted_coin.coin_flip(0.5f)) return false;

	rnd::Random<Engine32> gaussian{uint32_t{0}};
	return gaussian.gaussian(3.0f, 0.0f) == 3.0f && gaussian.engine().draws == 12;
}
#endif

constexpr bool compile_time_validation(){
	if(!validate_integer_api() || !validate_collections() || !validate_weighted_collections()) return false;
#ifndef RND_AVR_FAST_FLOAT
	if(!validate_constexpr_float_api()) return false;
#endif
	return true;
}

bool validate_runtime_float_api(){
	rnd::Random<Engine32> normalized{UINT32_C(0x80000000)};
	if(normalized.normalized() != 0.5f) return false;

	rnd::Random<Engine32> range{UINT32_C(0x80000000)};
	if(range.between(-2.0f, 2.0f) != 0.0f) return false;

	rnd::Random<Engine32> high_coin{UINT32_C(0x7fffffff)};
	rnd::Random<Engine32> low_coin{UINT32_C(0x80000000)};
	if(!high_coin.coin_flip(0.5f) || low_coin.coin_flip(0.5f)) return false;

	rnd::Random<Engine32> gaussian{uint32_t{0}};
	return gaussian.gaussian(7.0f, 0.0f) == 7.0f && gaussian.engine().draws == 12;
}

static_assert(compile_time_validation(), "random_avr.hpp failed constexpr validation");

int main(){
	if(!compile_time_validation()) return 1;
	if(!validate_runtime_float_api()) return 2;

	rnd::Random<Engine8> low_coin{uint8_t{0x7f}};
	rnd::Random<Engine8> high_coin{uint8_t{0x80}};
	if(low_coin.coin_flip() || !high_coin.coin_flip()) return 3;

	rnd::Random<Engine8> first{uint8_t{42}};
	rnd::Random<Engine8> second{uint8_t{42}};
	if(first != second || first.next() != second.next()) return 4;

	first.discard(3);
	second.next();
	second.next();
	second.next();
	if(first != second) return 5;

	return 0;
}
