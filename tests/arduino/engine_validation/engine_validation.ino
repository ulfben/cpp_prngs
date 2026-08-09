#include "engine_reference_validation.hpp"

volatile uint64_t output;

void setup(){
	PCG32 pcg;
	Konadare192 konadare;
	QuarkBurst64 quarkburst;
	RomuDuoJr romu;
	SmallFast32 small_fast_32;
	SmallFast64 small_fast_64;
	Xoshiro256SS xoshiro;

	output = pcg() ^ konadare() ^ quarkburst() ^ romu() ^
		small_fast_32() ^ small_fast_64() ^ xoshiro();
}

void loop(){}
