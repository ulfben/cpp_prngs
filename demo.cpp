#include "engines/pcg32.hpp" // PCG32 engine
#include "engines/romuduojr.hpp" // RomuDuoJr engine 
#include "random.hpp" // the Random interface
#include <print>
#include <vector>
#include <string_view>
#include <algorithm>

// Source: https://github.com/ulfben/cpp_prngs/
// Demo is available on Compiler Explorer: https://compiler-explorer.com/z/nzK9joeYE
// Benchmarks:
   // Quick Bench for generating raw random values: https://quick-bench.com/q/vWdKKNz7kEyf6kQSNnUEFOX_4DI
   // Quick Bench for generating normalized floats: https://quick-bench.com/q/GARc3WSfZu4sdVeCAMSWWPMQwSE
   // Quick Bench for generating bounded values: https://quick-bench.com/q/WHEcW9iSV7I8qB_4eb1KWOvNZU0

int main(){
   using namespace rnd;       
   const std::string_view str{"abcdefghijklmnopqrstuvwxyz"};
   std::vector<int> vec{1,2,3,4,5,6,7,8,9,10};

   Random<RomuDuoJr> random{};  //create a random number generator with default seed, using the PCG32 engine
   std::println("Random<RomuDuoJr>:");
   std::println("  next() [{},{}]: {}", random.min(), random.max(), random.next()); //same as random()
   std::println("  next(100) [0,100): {}", random.next(100)); //same as random(100)
   std::println("  between(10, 20): {}", random.between(10, 20));
   std::println("  between(5.0f, 10.0f): {}", random.between(5.0f, 10.0f));
   std::println("  normalized [0,1): {}", random.normalized<double>());
   std::println("  signed_norm [-1,1): {}", random.signed_norm<double>());
   std::println("  coin_flip(): {}", random.coin_flip()); //fair coin
   std::println("  weighted coin_flip(0.9f): {}", random.coin_flip(0.9f)); //90% probability of returning true
   std::println("  rgb8(): #{:06X}", random.rgb8()); // 24-bit RGB
   std::println("  rgba8(): #{:08X}", random.rgba8()); // 32-bit RGBA   
   std::println("  gaussian(0.0, 1.0) sample: {}", random.gaussian(0.0, 1.0));   
   std::println("  element(str): {}", random.element(str)); //pick a random element from a container
   std::println("  index(str): {}", random.index(str)); //pick a random index from a container   

   //compare discard() vs manual advance for PCG32 ===
   Random<PCG32> g1(42u), g2(42u);
   g1.discard(10); // fast skip 10 steps
   for(auto i = 0; i < 10; ++i){
      g2.next(); // manual advance
   }   
   assert(g1() == g2()); // same state after discard
   std::println("\n  Discard test passed: g1()==g2() == {}\n", g1() == g2());

   //Using with std algorithms, like std::shuffle  
   std::shuffle(vec.begin(), vec.end(), random);
   std::println("Shuffled vector:");
   for(auto v : vec) std::println("  {}", v);   

   return random.between(17, 71);
}