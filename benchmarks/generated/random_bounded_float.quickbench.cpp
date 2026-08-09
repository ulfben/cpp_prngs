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
#include <stdint.h>
namespace rnd{
namespace detail{
struct u128_parts final{
uint64_t lo;
uint64_t hi;
};
[[nodiscard]] constexpr u128_parts mul64_to_128_parts(uint64_t a, uint64_t b) noexcept{
const uint64_t a0 = static_cast<uint32_t>(a);
const uint64_t a1 = a >> 32;
const uint64_t b0 = static_cast<uint32_t>(b);
const uint64_t b1 = b >> 32;
const uint64_t p00 = a0 * b0;
const uint64_t p01 = a0 * b1;
const uint64_t p10 = a1 * b0;
const uint64_t p11 = a1 * b1;
const uint64_t mid = p01 + p10;
const uint64_t mid_carry = mid < p01 ? (uint64_t{1} << 32) : uint64_t{0};
const uint64_t mid_low = (mid & UINT32_MAX) << 32;
const uint64_t low = p00 + mid_low;
const uint64_t high = p11 + (mid >> 32) + mid_carry + (low < p00 ? 1u : 0u);
return {low, high};
}
}
}
#include <cstdint>
#ifdef _MSC_VER
#include <intrin.h>
#endif
namespace rnd{
namespace detail {
template <unsigned digits>
[[nodiscard]] constexpr std::uint64_t shr128_to_u64(std::uint64_t hi, std::uint64_t lo) noexcept{
static_assert(digits > 0 && digits <= 64);
if constexpr(digits == 64){
return hi;
} else{
return (lo >> digits) | (hi << (64u - digits));
}
}
template <unsigned digits>
[[nodiscard]] constexpr std::uint64_t mul_shift_u64(std::uint64_t x, std::uint64_t bound) noexcept{
static_assert(digits >= 1 && digits <= 64, "digits must be in [1, 64]");
#if defined(__SIZEOF_INT128__)
return static_cast<std::uint64_t>(
(static_cast<__uint128_t>(x) * static_cast<__uint128_t>(bound)) >> digits
);
#elif defined(_MSC_VER)
std::uint64_t hi = 0;
std::uint64_t lo = 0;
if consteval{
const auto p = mul64_to_128_parts(x, bound);
lo = p.lo;
hi = p.hi;
} else{
lo = _umul128(x, bound, &hi);
}
return shr128_to_u64<digits>(hi, lo);
#else
static_assert(false, "mul_shift_high64 requires either __uint128_t or MSVC _umul128");
#endif
}
}
}
#include <algorithm>
#include <bit>
#include <cassert>
#include <concepts>
#include <cstdint>
#include <iterator>
#include <limits>
#include <ranges>
#include <type_traits>
#include <utility>
namespace rnd {
template <class F>
concept supported_float =
(std::same_as<F, float> || std::same_as<F, double>) &&
(sizeof(F) == sizeof(std::uint32_t) || sizeof(F) == sizeof(std::uint64_t)) &&
std::numeric_limits<F>::is_iec559 &&
std::numeric_limits<F>::radix == 2;
template <RandomBitEngine E>
class Random final{
static constexpr unsigned value_bits = std::numeric_limits<typename E::result_type>::digits;
template <class T>
static constexpr bool valid_weight_type =
std::unsigned_integral<std::remove_cv_t<T>> &&
!std::same_as<std::remove_cv_t<T>, bool> &&
(std::numeric_limits<std::remove_cv_t<T>>::digits <= value_bits);
template <class R, class Projection>
using projected_weight_t = std::remove_cvref_t<std::invoke_result_t<Projection&, std::ranges::range_reference_t<R>>>;
public:
using engine_type = E;
using result_type = typename E::result_type;
using seed_type = typename E::seed_type;
constexpr Random() noexcept = default;
explicit constexpr Random(seed_type seed_val) noexcept : _e(seed_val){}
explicit constexpr Random(engine_type engine) noexcept : _e(engine){}
constexpr bool operator==(const Random& rhs) const noexcept = default;
constexpr const E& engine() const noexcept{
return _e;
}
constexpr E& engine() noexcept{
return _e;
}
constexpr void seed() noexcept{
_e.seed();
}
constexpr void seed(seed_type v) noexcept{
_e.seed(v);
}
constexpr void discard(unsigned long long n) noexcept{
_e.discard(n);
}
[[nodiscard]] constexpr Random split() noexcept{
return Random{bits_as<seed_type>()};
}
static constexpr result_type  min() noexcept{
return 0;
}
static constexpr result_type  max() noexcept{
return E::max();
}
constexpr result_type next() noexcept{
return _e();
}
constexpr result_type operator()() noexcept{
return next();
}
template <class T = result_type>
constexpr T bits(unsigned n) noexcept{
static_assert(std::is_unsigned_v<T>, "bits<T>(n) requires an unsigned T");
assert(n > 0);
assert(n <= std::numeric_limits<T>::digits);
if(n <= value_bits){
return take_high_bits<T>(next(), n);
}
return gather_bits_runtime<T>(n);
}
template <unsigned N, class T = result_type>
constexpr T bits() noexcept{
static_assert(N > 0, "Need at least 1 bit");
static_assert(std::is_unsigned_v<T>, "bits<N,T> requires an unsigned T");
static_assert(N <= std::numeric_limits<T>::digits, "T cannot hold N bits");
if constexpr(N <= value_bits){
return take_high_bits<T>(next(), N);
} else{
return gather_bits_runtime<T>(N);
}
}
template <class T>
constexpr T bits_as() noexcept{
static_assert(std::is_unsigned_v<T>, "bits_as<T>() requires an unsigned T");
return bits<std::numeric_limits<T>::digits, T>();
}
constexpr result_type next(result_type bound) noexcept{
assert(bound > 0 && "bound must be non-zero and positive");
result_type raw_value = next();
if constexpr(value_bits <= 32){
auto product = std::uint64_t(raw_value) * std::uint64_t(bound);
auto result = result_type(product >> value_bits);
return result;
} else if constexpr(value_bits <= 64){
return detail::mul_shift_u64<value_bits>(raw_value, bound);
} else{
return bound > 0 ? raw_value % bound : bound;
}
}
constexpr result_type operator()(result_type bound) noexcept{
return next(bound);
}
template <result_type Bound, std::integral T = result_type>
constexpr T next() noexcept{
static_assert(Bound > 0, "Bound must be positive");
static_assert(Bound - 1 <= static_cast<result_type>(std::numeric_limits<T>::max()),
"Bound is too large for return type T");
if constexpr(Bound == 1){
return T{0};
}else if constexpr((Bound & (Bound - 1)) == 0){
constexpr unsigned bits_needed = std::countr_zero(Bound);
return bits<bits_needed, T>();
} else{
return static_cast<T>(next(Bound));
}
}
template <std::integral I>
constexpr I between(I lo, I hi) noexcept{
if(!(lo < hi)){
assert(false && "between(lo, hi): inverted or empty range");
return lo;
}
using U = std::make_unsigned_t<I>;
U bound = U(hi) - U(lo);
assert(bound <= E::max() &&
"between(lo, hi): range too large for this engine. Consider a 64-bit engine "
"(xoshiro256ss, SmallFast64) or ensure hi–lo <= max()");
auto safe_bound = static_cast<result_type>(bound);
return static_cast<I>(U(lo) + static_cast<U>(next(safe_bound)));
}
template <supported_float F = float>
constexpr F normalized() noexcept{
using UInt = std::conditional_t<sizeof(F) == sizeof(std::uint32_t), std::uint32_t, std::uint64_t>;
constexpr int mantissa_bits = std::numeric_limits<F>::digits - 1;
constexpr UInt base = std::bit_cast<UInt>(F{1});
const UInt mantissa = this->template bits<mantissa_bits, UInt>();
const UInt as_int = base | mantissa;
return std::bit_cast<F>(as_int) - F{1};
}
template <supported_float F = float>
constexpr F signed_norm() noexcept{
return F{2} * normalized<F>() - F{1};
}
template <supported_float F = float>
constexpr F between(F lo, F hi) noexcept{
assert(lo < hi && "between(lo, hi): inverted or empty range");
return lo + (hi - lo) * normalized<F>();
}
constexpr bool coin_flip() noexcept{
return bits<1, unsigned>() != 0;
}
template <supported_float F = float>
constexpr bool coin_flip(F probability) noexcept{
assert(F{0} <= probability && probability <= F{1} && "coin_flip(probability): probability must be in [0, 1]");
return normalized<F>() < probability;
}
template <supported_float F = float>
constexpr F gaussian(F mean, F stddev) noexcept{
assert(stddev >= F{0} && "gaussian(mean, stddev): standard deviation must be non-negative");
F sum{};
for(auto i = 0; i < 12; ++i){
sum += normalized<F>();
}
return mean + (sum - F{6}) * stddev;
}
template <std::ranges::sized_range R>
[[nodiscard]] constexpr auto index(R&& collection) noexcept{
using size_type = std::ranges::range_size_t<R>;
const size_type size = std::ranges::size(collection);
assert(size != 0 && "Random::index(): empty collection.");
assert(std::in_range<result_type>(size) &&
"Random::index(): collection is too large for this engine.");
return static_cast<size_type>(next(static_cast<result_type>(size)));
}
template <std::ranges::forward_range R>
requires std::ranges::sized_range<R> &&
std::ranges::borrowed_range<R>
[[nodiscard]] constexpr auto iterator(R&& collection) noexcept{
const auto offset = static_cast<std::ranges::range_difference_t<R>>(index(collection));
return std::ranges::next(std::ranges::begin(collection), offset);
}
template <std::ranges::forward_range R>
requires std::ranges::sized_range<R> &&
std::ranges::borrowed_range<R>
[[nodiscard]] constexpr decltype(auto) element(R&& collection) noexcept{
return *iterator(std::forward<R>(collection));
}
template <std::ranges::forward_range R>
requires std::ranges::sized_range<R> &&
valid_weight_type<std::ranges::range_value_t<R>>
[[nodiscard]] constexpr auto weighted_index(R&& weights) noexcept{
using size_type = std::ranges::range_size_t<R>;
assert(!std::ranges::empty(weights) && "Random::weighted_index(): empty weight range.");
result_type total{};
for(const auto weight : weights){
assert(weight <= max() - total && "Random::weighted_index(): total weight is too large for this engine.");
total += weight;
}
assert(total != 0 && "Random::weighted_index(): at least one weight must be positive.");
result_type target = next(total);
size_type selected{};
for(const auto weight : weights){
if(target < weight){
return selected;
}
target -= weight;
++selected;
}
std::unreachable();
}
template <std::ranges::forward_range R, class Projection>
requires std::ranges::sized_range<R>&&
std::ranges::borrowed_range<R>&&
std::invocable<Projection&, std::ranges::range_reference_t<R>>&&
valid_weight_type<projected_weight_t<R, Projection>>
[[nodiscard]] constexpr auto weighted_iterator(R&& collection, Projection projection) noexcept{
auto weights = collection | std::views::transform(std::move(projection));
const auto offset = static_cast<std::ranges::range_difference_t<R>>(weighted_index(weights));
return std::ranges::next(std::ranges::begin(collection), offset);
}
template <std::ranges::forward_range R, class Projection>
requires std::ranges::sized_range<R>&&
std::ranges::borrowed_range<R>&&
std::invocable<Projection&, std::ranges::range_reference_t<R>>&&
valid_weight_type<projected_weight_t<R, Projection>>
[[nodiscard]] constexpr decltype(auto) weighted_element(R&& collection, Projection projection) noexcept{
return *weighted_iterator(std::forward<R>(collection), std::move(projection));
}
private:
E _e{};
template <class T>
static constexpr T low_bits_mask(unsigned n) noexcept{
assert(n <= std::numeric_limits<T>::digits);
constexpr unsigned W = std::numeric_limits<T>::digits;
if(n == 0) return T{0};
if(n >= W) return std::numeric_limits<T>::max();
return static_cast<T>((T{1} << n) - T{1});
}
template <class T>
constexpr T take_high_bits(E::result_type x, unsigned n) noexcept{
assert(1 <= n && n <= value_bits && n <= std::numeric_limits<T>::digits);
const unsigned shift = value_bits - n;
return static_cast<T>(x >> shift) & low_bits_mask<T>(n);
}
template <class T>
constexpr T gather_bits_runtime(unsigned n) noexcept{
assert(1 <= n && n <= std::numeric_limits<T>::digits);
T acc = 0;
unsigned filled = 0;
while(filled < n){
const unsigned take = std::min<unsigned>(value_bits, n - filled);
const T chunk = take_high_bits<T>(next(), take);
acc |= (chunk << filled);
filled += take;
}
return acc & low_bits_mask<T>(n);
}
};
}
#include <limits.h>
#if __cplusplus >= 202002L || (defined(_MSVC_LANG) && _MSVC_LANG >= 202002L)
#include <bit>
#endif
namespace rnd::detail {
template <class T>
[[nodiscard]] constexpr T rotl(T value, unsigned shift) noexcept{
#if defined(__cpp_lib_bitops) && __cpp_lib_bitops >= 201907L
return std::rotl(value, static_cast<int>(shift));
#else
constexpr unsigned digits = sizeof(T) * CHAR_BIT;
const unsigned amount = shift % digits;
return amount == 0 ? value : static_cast<T>((value << amount) | (value >> (digits - amount)));
#endif
}
template <class T>
[[nodiscard]] constexpr T rotr(T value, unsigned shift) noexcept{
#if defined(__cpp_lib_bitops) && __cpp_lib_bitops >= 201907L
return std::rotr(value, static_cast<int>(shift));
#else
constexpr unsigned digits = sizeof(T) * CHAR_BIT;
const unsigned amount = shift % digits;
return amount == 0 ? value : static_cast<T>((value >> amount) | (value << (digits - amount)));
#endif
}
}
#include <stdint.h>
class Konadare192 final{
using u64 = uint64_t;
static constexpr u64 INC = 0xBB67AE8584CAA73BULL;
static constexpr u64 DEFAULT_SEED = 1;
u64 a_{};
u64 b_{};
u64 c_{};
static constexpr u64 mix(u64 a, u64 b) noexcept{
u64 c = b;
u64 x = a;
for(u64 i = 0; i < 5; ++i){
x ^= rnd::detail::rotr(x, 25) ^ rnd::detail::rotr(x, 49);
c += INC + (c << 15) + (c << 7) + i;
c ^= (c >> 47) ^ (c >> 23);
x += c;
x ^= (x >> 11) ^ (x >> 3);
}
return x;
}
public:
using result_type = uint64_t;
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
b_ = rnd::detail::rotr(b_ + a0, 11);
c_ = rnd::detail::rotl(c_ + b_, 8);
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
static constexpr result_type (min)() noexcept{
return result_type{0};
}
static constexpr result_type (max)() noexcept{
return static_cast<result_type>(~result_type{0});
}
constexpr bool operator==(const Konadare192& rhs) const noexcept{
return a_ == rhs.a_ && b_ == rhs.b_ && c_ == rhs.c_;
}
constexpr bool operator!=(const Konadare192& rhs) const noexcept{
return !(*this == rhs);
}
};
#include <stdint.h>
class QuarkBurst64 final{
using u64 = uint64_t;
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
a_ = rnd::detail::rotl(a_, 29) ^ b_;
b_ += INCREMENT;
c_ = rnd::detail::rotl(c_, 41) + a_;
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
static constexpr result_type (min)() noexcept{
return result_type{0};
}
static constexpr result_type (max)() noexcept{
return static_cast<result_type>(~result_type{0});
}
constexpr bool operator==(const QuarkBurst64& rhs) const noexcept{
return a_ == rhs.a_ && b_ == rhs.b_ && c_ == rhs.c_;
}
constexpr bool operator!=(const QuarkBurst64& rhs) const noexcept{
return !(*this == rhs);
}
};
#include <stdint.h>
class RomuDuoJr final{
using u64 = uint64_t;
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
x *= rnd::detail::rotl(y, 27);
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
y = rnd::detail::rotl(y - old_x, 27);
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
static constexpr result_type (min)() noexcept{
return result_type{0};
}
static constexpr result_type (max)() noexcept{
return static_cast<result_type>(~result_type{0});
}
constexpr bool operator==(const RomuDuoJr& rhs) const noexcept{
return x == rhs.x && y == rhs.y;
}
constexpr bool operator!=(const RomuDuoJr& rhs) const noexcept{
return !(*this == rhs);
}
};
#include <benchmark/benchmark.h>
#include <cstdint>
#include <cstdlib>
#include <random>
namespace {
constexpr std::int64_t values_per_iteration = 1024;
constexpr std::uint64_t seed = 1234567890ULL;
constexpr float lower_bound = -10.0f;
constexpr float upper_bound = 10.0f;
template<class Engine>
void BM_RandomBoundedFloat(benchmark::State& state){
using seed_type = typename Engine::seed_type;
rnd::Random<Engine> rng{static_cast<seed_type>(seed)};
double sum = 0.0;
for([[maybe_unused]] auto _ : state){
for(std::int64_t i = 0; i < values_per_iteration; ++i){
sum += rng.between(lower_bound, upper_bound);
}
}
benchmark::DoNotOptimize(sum);
state.SetItemsProcessed(state.iterations() * values_per_iteration);
}
template<class Engine>
void BM_UniformRealDistribution(benchmark::State& state){
Engine rng{static_cast<typename Engine::result_type>(seed)};
std::uniform_real_distribution<float> distribution{
lower_bound,
upper_bound
};
double sum = 0.0;
for([[maybe_unused]] auto _ : state){
for(std::int64_t i = 0; i < values_per_iteration; ++i){
sum += distribution(rng);
}
}
benchmark::DoNotOptimize(sum);
state.SetItemsProcessed(state.iterations() * values_per_iteration);
}
void BM_CstdlibRandScaled(benchmark::State& state){
std::srand(static_cast<unsigned>(seed));
double sum = 0.0;
constexpr double scale =
1.0 / (static_cast<double>(RAND_MAX) + 1.0);
for([[maybe_unused]] auto _ : state){
for(std::int64_t i = 0; i < values_per_iteration; ++i){
const double normalized =
static_cast<double>(std::rand()) * scale;
sum += lower_bound +
(upper_bound - lower_bound) * normalized;
}
}
benchmark::DoNotOptimize(sum);
state.SetItemsProcessed(state.iterations() * values_per_iteration);
}
BENCHMARK_TEMPLATE(BM_RandomBoundedFloat, QuarkBurst64);
BENCHMARK_TEMPLATE(BM_RandomBoundedFloat, RomuDuoJr);
BENCHMARK_TEMPLATE(BM_RandomBoundedFloat, Konadare192);
BENCHMARK_TEMPLATE(BM_UniformRealDistribution, std::mt19937_64);
BENCHMARK_TEMPLATE(BM_UniformRealDistribution, std::mt19937);
BENCHMARK_TEMPLATE(BM_UniformRealDistribution, std::minstd_rand);
BENCHMARK(BM_CstdlibRandScaled);
}
