#include "xoshiro256.h" //for RNG::splitmix()
#include <chrono>
#include <ctime>
#include <thread>
#include <random> //only needed for createSeedsB, for std::random_device

/*This file demonstrates a few ideas for sourcing entropy in C++. 
    std::random_device is typically hardware-based, high-quality entropy and works fine on most platforms and configurations. 
    However, there are times when you might need other sources of entropy — perhaps for speed reasons, to generate seeds at 
compile time, or when targeting portable devices without hardware / kernel entropy evailable. 

Use these functions as inspiration and adjust for your own platform and needs.*/

// createSeeds uses time-since-the-epoch, CPU time, thread ID, and a stack memory address as sources of entropy.
// each value is hashed and then further mixed using splitmix64. 
// hashing *and* splitmixing is very likely overkill. One or the other ought to be fine.
static typename RNG::State createSeeds() noexcept{
    const auto hash = std::hash<RNG::u64>{};
    const auto date = hash(std::chrono::system_clock::now().time_since_epoch().count());    
    const auto cpu_time = hash(std::clock());    
    const auto uptime = hash(std::chrono::high_resolution_clock::now().time_since_epoch().count());
    const auto mixed_time = hash((uptime << 1) ^ date);
    const auto thread_id = std::hash<std::thread::id>{}(std::this_thread::get_id());    
    const auto local_addr = hash(reinterpret_cast<std::uintptr_t>(&hash));
    return {
        RNG::splitmix64(mixed_time),
        RNG::splitmix64(thread_id),
        RNG::splitmix64(cpu_time),
        RNG::splitmix64(local_addr),
    };
}

//createSeedsB *additionally* mix those sources with entropy from std::random_device
static typename RNG::State createSeedsB() {
    std::random_device rd;    
    auto seeds = createSeeds();
    seeds[0] = RNG::splitmix64(rd() ^ seeds[0]);
    seeds[1] = RNG::splitmix64(rd() ^ (seeds[0] << 1));
    seeds[2] = RNG::splitmix64(rd() ^ (seeds[1] << 1));
    seeds[3] = RNG::splitmix64(rd() ^ (seeds[2] << 1));
    return seeds;
}

// compile_time_seed() generates a 64-bit value at compile time based on the date, time, and a secret string.
// If your compiler supports the __COUNTER__ preprocessor macro, this function can produce unique seeds 
// for each invocation within the same compilation unit. 
// Note that __DATE__ and __TIME__ are expanded only once during the compilation, so they remain constant 
// throughout the entire compilation process.
constexpr uint64_t compile_time_seed() noexcept{
    uint64_t shifted = 0;       
    const char* source = __DATE__ " " __TIME__ " secret ";
    for (const char* c = source; *c != '\0'; ++c) {
        shifted <<= 8;
        shifted |= static_cast<uint64_t>(*c);
    }
#ifdef __COUNTER__
    shifted ^= static_cast<uint64_t>(__COUNTER__);
#endif
    return shifted;
}
