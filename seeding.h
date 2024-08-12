#include "xoshiro256.h" //for RNG::splitmix()
#include <chrono>
#include <ctime>
#include <thread>
#include <random> //only needed for createSeedsB, for std::random_device

//seeding.h is a collection of different strategies for generating seeds. Probably not production code worthy, but good enough for a bit of inspiration. :)

// createSeeds uses time-since-the-epoch, CPU time, thread ID, and a memory address as sources of entropy.
// each value is hashed and then further mixed using splitmix64. 
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
