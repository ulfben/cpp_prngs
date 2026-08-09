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

constexpr bool compile_time_validation(){
	rnd::Random<Engine8> bounded{uint8_t{128}};
	if(bounded.next(uint8_t{100}) != uint8_t{50} || bounded.engine().draws != 1) return false;

	rnd::Random<Engine8> bits{uint8_t{0x12}};
	if(bits.bits_as<uint16_t>() != uint16_t{0x1312} || bits.engine().draws != 2) return false;

	rnd::Random<Engine8> ranges{uint8_t{128}};
	if(ranges.between(uint8_t{10}, uint8_t{20}) != uint8_t{15}) return false;

	rnd::Random<Engine8> collections{uint8_t{192}};
	int values[]{10, 20, 30, 40};
	if(collections.index(values) != 3) return false;
	collections.seed(uint8_t{192});
	if(collections.element(values) != 40) return false;

	rnd::Random<Engine16> native_16{uint16_t{32768}};
	if(native_16.next(uint16_t{1000}) != uint16_t{500} || native_16.engine().draws != 1) return false;

	rnd::Random<Engine32> native_32{uint32_t{0x80000000}};
	if(native_32.next(uint32_t{1000}) != uint32_t{500} || native_32.engine().draws != 1) return false;

	rnd::Random<Engine64> native_64{uint64_t{0x8000000000000000}};
	if(native_64.next(uint64_t{1000}) != uint64_t{500} || native_64.engine().draws != 1) return false;

	rnd::Random<Engine64> maximum_64{UINT64_MAX};
	return maximum_64.next(UINT64_MAX) == UINT64_MAX - 1;
}

static_assert(compile_time_validation(), "random_avr.hpp failed constexpr validation");

int main(){
	if(!compile_time_validation()) return 1;

	rnd::Random<Engine8> low_coin{uint8_t{0x7f}};
	rnd::Random<Engine8> high_coin{uint8_t{0x80}};
	if(low_coin.coin_flip() || !high_coin.coin_flip()) return 2;

	rnd::Random<Engine8> first{uint8_t{42}};
	rnd::Random<Engine8> second{uint8_t{42}};
	if(first != second || first.next() != second.next()) return 3;

	first.discard(3);
	second.next();
	second.next();
	second.next();
	if(first != second) return 4;

	return 0;
}
