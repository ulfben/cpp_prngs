#include <concepts>
#include <limits>
#include <random>       
#include <type_traits>  
template<typename E>
concept RandomBitEngine =
requires {
typename E::result_type;
typename E::seed_type;
} &&
std::uniform_random_bit_generator<E> &&
std::same_as<typename E::result_type, std::invoke_result_t<E&>> &&
std::default_initializable<E> &&
std::copy_constructible<E> &&
std::constructible_from<E, typename E::seed_type> &&
std::equality_comparable<E> &&
std::is_nothrow_default_constructible_v<E> &&
std::is_nothrow_copy_constructible_v<E> &&
std::is_nothrow_constructible_v<E, typename E::seed_type> &&
std::is_unsigned_v<typename E::result_type> &&
std::numeric_limits<typename E::result_type>::is_integer &&
std::is_unsigned_v<typename E::seed_type> &&
std::numeric_limits<typename E::seed_type>::is_integer &&
(E::min() == typename E::result_type{0}) &&
(E::max() == std::numeric_limits<typename E::result_type>::max()) &&
requires(E& e, const E& ce, typename E::seed_type seed, unsigned long long n){
{ e() } noexcept -> std::same_as<typename E::result_type>;
{ E::min() } noexcept -> std::same_as<typename E::result_type>;
{ E::max() } noexcept -> std::same_as<typename E::result_type>;
{ ce == ce } noexcept -> std::convertible_to<bool>;
{ e.seed() } noexcept -> std::same_as<void>;
{ e.seed(seed) } noexcept -> std::same_as<void>;
{ e.discard(n) } noexcept -> std::same_as<void>;
};
#include <limits>
#include <cstdint>
#include <bit>  
class Konadare192 final{
using u64 = std::uint64_t;
static constexpr u64 INC = 0xBB67AE8584CAA73BULL;
static constexpr u64 DEFAULT_SEED = 1;
u64 a_{};
u64 b_{};
u64 c_{};
static constexpr u64 mix(u64 a, u64 b) noexcept{
u64 c = b;
u64 x = a;
for(u64 i = 0; i < 5; ++i){
x ^= std::rotr(x, 25) ^ std::rotr(x, 49);
c += INC + (c << 15) + (c << 7) + i;
c ^= (c >> 47) ^ (c >> 23);
x += c;
x ^= (x >> 11) ^ (x >> 3);
}
return x;
}
public:
using result_type = std::uint64_t;
using seed_type = u64;
constexpr Konadare192() noexcept : Konadare192(DEFAULT_SEED){}
constexpr explicit Konadare192(seed_type seed_val) noexcept : a_(seed_val), b_(seed_val + 1), c_(seed_val + 2){
for(int m = 0; m < 2; ++m){  
result_type t0 = mix(a_, c_);
result_type t1 = mix(b_, a_);
result_type t2 = mix(c_, b_);
a_ = t0; b_ = t1; c_ = t2;
}
if((a_ | b_ | c_) == 0){  
a_ = 0x3C6EF372FE94F82BULL;  
}
}
constexpr result_type next() noexcept{
result_type out = b_ ^ c_;
result_type a0 = a_ ^ (a_ >> 32);
a_ += INC;
b_ = std::rotr(b_ + a0, 11);
c_ = std::rotl(c_ + b_, 8);
return out;
}
constexpr result_type operator()() noexcept{ return next(); }
constexpr void discard(result_type n) noexcept{
while(n--){
next();
}
}
constexpr void seed(result_type value) noexcept{
*this = Konadare192{value};
}
constexpr void seed() noexcept{
*this = Konadare192{};
}
static constexpr result_type min() noexcept{
return result_type{0};
}
static constexpr result_type max() noexcept{
return std::numeric_limits<result_type>::max();
}
constexpr bool operator==(const Konadare192&) const noexcept = default;
};
static_assert(RandomBitEngine<Konadare192>);
#include <bit>
#include <cstdint>
#include <limits>
class PCG32 final{
struct Direct{};  
using u64 = std::uint64_t;
using u32 = std::uint32_t;  
static constexpr u64 DEFAULT_SEED = 0x853c49e6748fea9bULL;
static constexpr u64 DEFAULT_STREAM = 0xda3e39cb94b95bdbULL;
static constexpr u64 MULT = 6364136223846793005ULL;
u64 state{0};  
u64 inc{1};  
constexpr PCG32(u64 state_val, u64 inc_val, Direct) noexcept
: state(state_val), inc(inc_val){}
public:
using result_type = u32;
using seed_type = u64;
using state_type = u64;
constexpr PCG32() noexcept
: PCG32(DEFAULT_SEED, DEFAULT_STREAM){}
explicit constexpr PCG32(seed_type seed_val) noexcept
: PCG32(seed_val, DEFAULT_STREAM){}
constexpr PCG32(seed_type seed_val, seed_type sequence) noexcept{
seed(seed_val, sequence);
}
static constexpr PCG32 from_state(state_type state_val, state_type inc_val) noexcept{
return PCG32{state_val, inc_val, Direct{}};
}
constexpr result_type next() noexcept{
const auto oldstate = state;
state = oldstate * MULT + inc;
const u32 xorshifted = static_cast<u32>(((oldstate >> 18u) ^ oldstate) >> 27u);
const u32 rot = static_cast<u32>(oldstate >> 59u);
return std::rotr(xorshifted, static_cast<int>(rot));
}
constexpr result_type operator()() noexcept{
return next();
}
constexpr void seed() noexcept{
seed(DEFAULT_SEED, DEFAULT_STREAM);
}
constexpr void seed(seed_type seed_val, seed_type sequence = DEFAULT_STREAM) noexcept{
state = 0U;
inc = (sequence << 1u) | 1u;  
(void) next();  
state += seed_val;
(void) next();
}
constexpr void discard(unsigned long long delta) noexcept{
u64 cur_mult = MULT;
u64 cur_plus = inc;
u64 acc_mult = 1u;
u64 acc_plus = 0u;
while(delta > 0){
if(delta & 1){
acc_mult *= cur_mult;
acc_plus = acc_plus * cur_mult + cur_plus;
}
cur_plus = (cur_mult + 1) * cur_plus;
cur_mult *= cur_mult;
delta /= 2;
}
state = acc_mult * state + acc_plus;
}
constexpr PCG32 split() noexcept{
return PCG32{next(), (next() << 1u) | 1u};
}
static constexpr result_type min() noexcept{
return result_type{0};
}
static constexpr result_type max() noexcept{
return std::numeric_limits<result_type>::max();
}
constexpr bool operator==(const PCG32& rhs) const noexcept = default;
};
static_assert(RandomBitEngine<PCG32>);
#include <bit>
#include <cstdint>
#include <limits>
class QuarkBurst64 final{
using u64 = std::uint64_t;
static constexpr u64 DEFAULT_SEED = 0xFEEDFACECAFEBEEFULL;
static constexpr u64 INCREMENT = 1'111'111'111'111'111ULL;
u64 a_{};
u64 b_{};
u64 c_{};
struct Direct{};
constexpr QuarkBurst64(u64 a, u64 b, u64 c, Direct) noexcept
: a_(a), b_(b), c_(c){}
static constexpr u64 splitmix64(u64& state) noexcept{
state += 0x9E3779B97F4A7C15ULL;
u64 value = state;
value = (value ^ (value >> 30)) * 0xBF58476D1CE4E5B9ULL;
value = (value ^ (value >> 27)) * 0x94D049BB133111EBULL;
return value ^ (value >> 31);
}
public:
using result_type = u64;
using seed_type = u64;
using state_type = u64;
constexpr QuarkBurst64() noexcept
: QuarkBurst64(DEFAULT_SEED){}
explicit constexpr QuarkBurst64(seed_type seed_value) noexcept{
a_ = splitmix64(seed_value);
b_ = splitmix64(seed_value);
c_ = splitmix64(seed_value);
discard(3);
}
static constexpr QuarkBurst64 from_state(
state_type a,
state_type b,
state_type c
) noexcept{
return QuarkBurst64{a, b, c, Direct{}};
}
constexpr void seed() noexcept{
*this = QuarkBurst64{};
}
constexpr void seed(seed_type seed_value) noexcept{
*this = QuarkBurst64{seed_value};
}
constexpr result_type next() noexcept{
a_ = std::rotl(a_, 29) ^ b_;
b_ += INCREMENT;
c_ = std::rotl(c_, 41) + a_;
return c_;
}
constexpr result_type operator()() noexcept{
return next();
}
constexpr void discard(unsigned long long n) noexcept{
while(n--){
next();
}
}
static constexpr result_type min() noexcept{
return result_type{0};
}
static constexpr result_type max() noexcept{
return std::numeric_limits<result_type>::max();
}
constexpr bool operator==(const QuarkBurst64&) const noexcept = default;
};
static_assert(RandomBitEngine<QuarkBurst64>);
#include <cstdint>
#include <limits>
#include <bit>  
class RomuDuoJr final{
using u64 = std::uint64_t;
using state_type = u64;
state_type x;
state_type y;
struct Direct{};  
constexpr RomuDuoJr(state_type xstate, state_type ystate, Direct) noexcept
: x(xstate), y(ystate){}
public:
using result_type = u64;
using seed_type = u64;   
constexpr RomuDuoJr() noexcept : RomuDuoJr(0xFEEDFACEFEEDFACEULL){}
explicit constexpr RomuDuoJr(seed_type seed) noexcept
: x(0x9E6C63D0676A9A99ULL), y(~seed - seed){
y *= x;
y = y ^ (y >> 23) ^ (y >> 51);
y *= x;
x *= std::rotl(y, 27);
y = y ^ (y >> 23) ^ (y >> 51);
}
constexpr void seed() noexcept{
*this = RomuDuoJr{};
}
constexpr void seed(seed_type seed) noexcept{
*this = RomuDuoJr{seed};
}
static constexpr RomuDuoJr from_state(state_type xstate, state_type ystate) noexcept{
return RomuDuoJr{xstate, ystate, Direct{}};
}
constexpr result_type next() noexcept{
const state_type old_x = x;
x = y * 15241094284759029579ULL;
y = std::rotl(y - old_x, 27);
return old_x;
}
constexpr result_type operator()() noexcept{
return next();
}
constexpr void discard(result_type n) noexcept{
while(n--){
next();
}
}
static constexpr result_type min() noexcept{
return result_type{0};
}
static constexpr result_type max() noexcept{
return std::numeric_limits<result_type>::max();
}
constexpr bool operator==(const RomuDuoJr& rhs) const noexcept = default;
};
static_assert(RandomBitEngine<RomuDuoJr>);
#include <limits>
#include <cstdint>
#include <bit>
class SmallFast32 final{
using u32 = std::uint32_t;
using u64 = std::uint64_t;
u32 a;
u32 b;
u32 c;
u32 d;
public:
using result_type = u32;
using seed_type = u64;
constexpr SmallFast32() noexcept
: SmallFast32(0xBADC0FFEu){}
explicit constexpr SmallFast32(seed_type seed) noexcept
: a(0xf1ea5eedu)
, b(static_cast<u32>(seed))
, c(static_cast<u32>(seed >> 32))
, d(static_cast<u32>(seed ^ (seed >> 32))){
discard(20);  
}
explicit constexpr SmallFast32(u32 seed) noexcept
: a(0xf1ea5eedu)
, b(seed)
, c(seed)
, d(seed){
discard(20);  
}
constexpr void seed() noexcept{
*this = SmallFast32{};
}
constexpr void seed(seed_type seed) noexcept{
*this = SmallFast32{seed};
}
static constexpr result_type min() noexcept{
return result_type{0};
}
static constexpr result_type max() noexcept{
return std::numeric_limits<result_type>::max();
}
constexpr result_type operator()() noexcept{
return next();
}
constexpr result_type next() noexcept{
const u32 e = a - std::rotl(b, 27);
a = b ^ std::rotl(c, 17);
b = c + d;
c = d + e;
d = e + a;
return d;
}
constexpr void discard(unsigned long long n) noexcept{
while(n--){
next();
}
}
constexpr bool operator==(const SmallFast32& rhs) const noexcept = default;
};
static_assert(RandomBitEngine<SmallFast32>);
#include <limits>
#include <cstdint>
#include <bit>  
class SmallFast64{
using u64 = std::uint64_t;
u64 a;
u64 b;
u64 c;
u64 d;
public:
using result_type = u64;
using seed_type = u64;
constexpr SmallFast64() noexcept
: SmallFast64(0xBADC0FFEE0DDF00DuLL){}
explicit constexpr SmallFast64(seed_type seed) noexcept : a(0xf1ea5eeduLL), b(seed), c(seed), d(seed){
discard(20); 
}
constexpr void seed() noexcept{
*this = SmallFast64{};
}
constexpr void seed(seed_type seed) noexcept{
*this = SmallFast64{seed};
}
static constexpr result_type max() noexcept{
return std::numeric_limits<u64>::max();
}
static constexpr result_type min() noexcept{
return result_type{0};
}
constexpr result_type next() noexcept{
const u64 e = a - std::rotl(b, 7);
a = b ^ std::rotl(c, 13);
b = c + std::rotl(d, 37);
c = d + e;
d = e + a;
return d;
}
constexpr result_type operator()() noexcept{
return next();
}
constexpr void discard(result_type n) noexcept{
while(n--){
next();
}
}
constexpr bool operator==(const SmallFast64& rhs) const noexcept = default;
};
static_assert(RandomBitEngine<SmallFast64>);
#include <limits>
#include <cstdint>
#include <span>
#include <bit>  
#include <cassert>
class Xoshiro256SS{
using u64 = std::uint64_t;
u64 s[4]{};
static constexpr u64 splitmix64(u64& x) noexcept{
x += 0x9E3779B97F4A7C15ULL;  
u64 z = x;
z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ULL;
z = (z ^ (z >> 27)) * 0x94D049BB133111EBULL;
return z ^ (z >> 31);
}
constexpr Xoshiro256SS(std::span<const u64, 4> state) noexcept{		
s[0] = state[0];
s[1] = state[1];
s[2] = state[2];
s[3] = state[3];	
assert((s[0] | s[1] | s[2] | s[3]) != 0 && "xoshiro256** all-zero state is invalid");
}
public:
using result_type = u64;
using seed_type = u64;
using state_type = u64;
constexpr Xoshiro256SS() noexcept
: Xoshiro256SS(0xFEEDFACECAFEBEEFuLL){}
explicit constexpr Xoshiro256SS(seed_type seed) noexcept{		
s[0] = splitmix64(seed);
s[1] = splitmix64(seed);
s[2] = splitmix64(seed);
s[3] = splitmix64(seed);
if((s[0] | s[1] | s[2] | s[3]) == 0){
s[0] = 0xFEEDFACECAFEBEEFuLL;  
}
}
static constexpr Xoshiro256SS from_state(std::span<const state_type, 4> state) noexcept{
return Xoshiro256SS{state};
}
constexpr void seed() noexcept{
*this = Xoshiro256SS{};
}
constexpr void seed(seed_type seed) noexcept{
*this = Xoshiro256SS{seed};
}
static constexpr result_type min() noexcept{
return result_type{0};
}
static constexpr result_type max() noexcept{
return std::numeric_limits<result_type>::max();
}
constexpr result_type next() noexcept{
const auto result = std::rotl(s[1] * 5, 7) * 9;
const auto t = s[1] << 17;
s[2] ^= s[0];
s[3] ^= s[1];
s[1] ^= s[2];
s[0] ^= s[3];
s[2] ^= t;
s[3] = std::rotl(s[3], 45);
return result;
}
constexpr result_type operator()() noexcept{
return next();
}
constexpr void discard(unsigned long long n) noexcept{
while(n--){
next();
}
}
constexpr bool operator==(const Xoshiro256SS& rhs) const noexcept = default;  
};
static_assert(RandomBitEngine<Xoshiro256SS>);
#include <benchmark/benchmark.h>
#include <cstdint>
#include <cstdlib>
#include <random>
namespace {
constexpr std::int64_t values_per_iteration = 1024;
template<class Engine>
void BM_EngineNext(benchmark::State& state){
using seed_type = typename Engine::seed_type;
Engine rng{static_cast<seed_type>(1234567890ULL)};
std::uint64_t sum = 0;
for([[maybe_unused]] auto _ : state){
for(std::int64_t i = 0; i < values_per_iteration; ++i){
sum += static_cast<std::uint64_t>(rng.next());
}
}
benchmark::DoNotOptimize(sum);
state.SetItemsProcessed(state.iterations() * values_per_iteration);
}
BENCHMARK_TEMPLATE(BM_EngineNext, PCG32);
BENCHMARK_TEMPLATE(BM_EngineNext, SmallFast32);
BENCHMARK_TEMPLATE(BM_EngineNext, SmallFast64);
BENCHMARK_TEMPLATE(BM_EngineNext, Xoshiro256SS);
BENCHMARK_TEMPLATE(BM_EngineNext, RomuDuoJr);
BENCHMARK_TEMPLATE(BM_EngineNext, Konadare192);
BENCHMARK_TEMPLATE(BM_EngineNext, QuarkBurst64);
template<class Engine>
void BM_StandardEngine(benchmark::State& state){
Engine rng{0};
std::uint64_t sum = 0;
for([[maybe_unused]] auto _ : state){
for(std::int64_t i = 0; i < values_per_iteration; ++i){
sum += static_cast<std::uint64_t>(rng());
}
}
benchmark::DoNotOptimize(sum);
state.SetItemsProcessed(state.iterations() * values_per_iteration);
}
BENCHMARK_TEMPLATE(BM_StandardEngine, std::mt19937);
BENCHMARK_TEMPLATE(BM_StandardEngine, std::default_random_engine);
BENCHMARK_TEMPLATE(BM_StandardEngine, std::minstd_rand);
void BM_CstdlibRand(benchmark::State& state){
std::srand(0);
std::uint64_t sum = 0;
for([[maybe_unused]] auto _ : state){
for(std::int64_t i = 0; i < values_per_iteration; ++i){
sum += static_cast<std::uint64_t>(std::rand());
}
}
benchmark::DoNotOptimize(sum);
state.SetItemsProcessed(state.iterations() * values_per_iteration);
}
BENCHMARK(BM_CstdlibRand);
}  
