#include "engine_reference_validation.hpp"
#include <random.hpp>

volatile uint64_t output;
volatile uint16_t input = 0x8000;
volatile float float_output;
volatile double double_output;

struct LootDrop{
	uint8_t id;
	uint8_t weight;
	constexpr uint8_t get_weight() const noexcept{ return weight; }
};

template <class T, size_t N>
struct TinyArray{
	T values[N];
	constexpr T* data() noexcept{ return values; }
	constexpr const T* data() const noexcept{ return values; }
	constexpr size_t size() const noexcept{ return N; }
	constexpr T* begin() noexcept{ return values; }
	constexpr const T* begin() const noexcept{ return values; }
};

static_assert(rnd::detail::supported_uint<uint8_t>);
static_assert(rnd::detail::supported_uint<uint16_t>);
static_assert(rnd::detail::supported_uint<uint32_t>);
static_assert(rnd::detail::supported_uint<uint64_t>);
static_assert(rnd::detail::supported_integer<int8_t>);
static_assert(rnd::detail::supported_integer<int16_t>);
static_assert(rnd::detail::supported_integer<int32_t>);
static_assert(rnd::detail::supported_integer<int64_t>);
static_assert(!rnd::detail::supported_integer<bool>);
static_assert(!rnd::detail::supported_integer<char>);

#ifndef RND_FAST_FLOAT
constexpr bool validate_constexpr_double(){
	rnd::Random<SmallFast8> random;
	const double value = random.normalized<double>();
	return value >= 0.0 && value < 1.0;
}

static_assert(validate_constexpr_double(), "binary32 double must support constexpr generation");
#endif

void setup(){
	PCG32 pcg;
	Konadare192 konadare;
	QuarkBurst64 quarkburst;
	RomuDuoJr romu;
	SmallFast8 small_fast_8;
	SmallFast16 small_fast_16;
	SmallFast32 small_fast_32;
	SmallFast64 small_fast_64;
	XorShift32Star8 xorshift_32_star_8{static_cast<uint32_t>(input)};
	Xoshiro256SS xoshiro;

	output = pcg() ^ konadare() ^ quarkburst() ^ romu() ^
		small_fast_8() ^ small_fast_16() ^ small_fast_32() ^ small_fast_64() ^
		xorshift_32_star_8() ^ xoshiro();

	// Exercise the shared Random<E> frontend across all supported result widths.
	rnd::Random<PCG32> random_32;
	rnd::Random<RomuDuoJr> random_64;
	rnd::Random<SmallFast8> random_8{static_cast<uint8_t>(input)};
	rnd::Random<SmallFast16> random_16{static_cast<uint16_t>(input)};
	uint8_t values[]{1, 2, 3, 4};
	const uint8_t weights[]{0, 2, 0, 6};
	TinyArray<uint8_t, 4> fixed_values{{1, 2, 3, 4}};
	const TinyArray<uint8_t, 4> fixed_weights{{0, 2, 0, 6}};
	LootDrop loot[]{
		{1, 0},
		{2, 2},
		{3, 0},
		{4, 6}
	};
	output ^= random_8.next(uint8_t{100});
	output ^= random_8.next<16, uint8_t>();
	output ^= random_16.next(uint16_t{1000});
	output ^= random_32.next(uint32_t{100});
	output ^= random_32.between(int16_t{-10}, int16_t{20});
	output ^= random_32.bits<8, uint8_t>();
	output ^= random_32.coin_flip();
	output ^= *random_32.iterator(values, 4);
	output ^= random_32.element(values);
	output ^= random_32.element(fixed_values);
	output ^= random_32.weighted_index(weights);
	output ^= random_32.weighted_index(fixed_weights);
	output ^= random_32.weighted_element(loot, &LootDrop::weight).id;
	output ^= random_32.weighted_element(loot, 4, &LootDrop::get_weight).id;
	output ^= random_64.next(uint64_t{1000});

	float_output = random_32.normalized<float>();
	float_output += random_32.signed_norm<float>();
	float_output += random_32.between(-10.0f, 20.0f);
	float_output += random_32.coin_flip(0.75f);
	float_output += random_32.gaussian(10.0f, 2.0f);
	double_output = random_32.normalized<double>();
}

void loop(){}
