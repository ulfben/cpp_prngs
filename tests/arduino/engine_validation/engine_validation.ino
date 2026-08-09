#include "engine_reference_validation.hpp"
#include <random_avr.hpp>

template <class UInt>
class CountingEngine final{
public:
	using result_type = UInt;
	using seed_type = UInt;

	constexpr CountingEngine() noexcept = default;
	explicit constexpr CountingEngine(seed_type value) noexcept : _value(value){}
	static constexpr result_type (min)() noexcept{ return result_type{0}; }
	static constexpr result_type (max)() noexcept{ return static_cast<result_type>(~result_type{0}); }
	constexpr result_type operator()() noexcept{ return _value++; }
	constexpr void seed() noexcept{ _value = 0; }
	constexpr void seed(seed_type value) noexcept{ _value = value; }
	constexpr void discard(unsigned long long count) noexcept{ while(count--) operator()(); }
	constexpr bool operator==(const CountingEngine& rhs) const noexcept{ return _value == rhs._value; }
	constexpr bool operator!=(const CountingEngine& rhs) const noexcept{ return !(*this == rhs); }

private:
	result_type _value{};
};

volatile uint64_t output;
volatile uint16_t input = 0x8000;

void setup(){
	PCG32 pcg;
	Konadare192 konadare;
	QuarkBurst64 quarkburst;
	RomuDuoJr romu;
	SmallFast16 small_fast_16;
	SmallFast32 small_fast_32;
	SmallFast64 small_fast_64;
	Xoshiro256SS xoshiro;

	output = pcg() ^ konadare() ^ quarkburst() ^ romu() ^
		small_fast_16() ^ small_fast_32() ^ small_fast_64() ^ xoshiro();

	// Exercise the reduced Random<E> frontend across all supported result widths.
	rnd::Random<PCG32> random_32;
	rnd::Random<RomuDuoJr> random_64;
	rnd::Random<CountingEngine<uint8_t>> random_8{static_cast<uint8_t>(input)};
	rnd::Random<SmallFast16> random_16{static_cast<uint16_t>(input)};
	uint8_t values[]{1, 2, 3, 4};
	output ^= random_8.next(uint8_t{100});
	output ^= random_16.next(uint16_t{1000});
	output ^= random_32.next(uint32_t{100});
	output ^= random_32.between(uint32_t{10}, uint32_t{20});
	output ^= random_32.bits<8, uint8_t>();
	output ^= random_32.coin_flip();
	output ^= random_32.element(values);
	output ^= random_64.next(uint64_t{1000});
}

void loop(){}
