#include <rnd/engines/romuduojr.hpp>
#include <rnd/random.hpp>

int main()
{
	rnd::Random<rnd::RomuDuoJr> random{1234};
	return random.next<16>() < 16 ? 0 : 1;
}
