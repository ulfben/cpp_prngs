#include <float.h>
#include <limits.h>
#include <stddef.h>
#include <stdint.h>
#if defined(__has_include)
#if __has_include(<iterator>) && __has_include(<limits>) && __has_include(<type_traits>) && __has_include(<utility>)
#define RND_DETAIL_HAS_STANDARD_COMPAT 1
#endif
#endif
#ifndef RND_DETAIL_HAS_STANDARD_COMPAT
#define RND_DETAIL_HAS_STANDARD_COMPAT 0
#endif
#if RND_DETAIL_HAS_STANDARD_COMPAT
#include <iterator>
#include <limits>
#include <type_traits>
#include <utility>
#if defined(__cpp_consteval) && defined(__has_include)
#if __has_include(<bit>)
#include <bit>
#endif
#endif
#endif
#if defined(__cpp_lib_bit_cast) && __cpp_lib_bit_cast >= 201806L
#define RND_DETAIL_HAS_CONSTEXPR_BIT_CAST 1
#else
#define RND_DETAIL_HAS_CONSTEXPR_BIT_CAST 0
#endif
#if defined(RND_FAST_FLOAT) && !RND_DETAIL_HAS_CONSTEXPR_BIT_CAST
#include <string.h>
#define RND_DETAIL_FLOAT_CONSTEXPR
#else
#define RND_DETAIL_FLOAT_CONSTEXPR constexpr
#endif
namespace rnd::detail {
#if RND_DETAIL_HAS_STANDARD_COMPAT
template <class T>
using remove_cv_t = typename std::remove_cv<T>::type;
template <class T>
using remove_cvref_t = typename std::remove_cv<typename std::remove_reference<T>::type>::type;
template <bool Condition, class T = void>
using enable_if_t = typename std::enable_if<Condition, T>::type;
template <class... T>
using void_t = std::void_t<T...>;
template <class T, class U>
static constexpr bool is_same = std::is_same<T, U>::value;
template <class T>
T&& declval() noexcept;
#else
template <class T, class U>
struct is_same_impl{
static constexpr bool value = false;
};
template <class T>
struct is_same_impl<T, T>{
static constexpr bool value = true;
};
template <class T, class U>
static constexpr bool is_same = is_same_impl<T, U>::value;
template <class T>
struct remove_cv{
using type = T;
};
template <class T>
struct remove_cv<const T>{
using type = T;
};
template <class T>
struct remove_cv<volatile T>{
using type = T;
};
template <class T>
struct remove_cv<const volatile T>{
using type = T;
};
template <class T>
using remove_cv_t = typename remove_cv<T>::type;
template <class T>
struct remove_reference{
using type = T;
};
template <class T>
struct remove_reference<T&>{
using type = T;
};
template <class T>
struct remove_reference<T&&>{
using type = T;
};
template <class T>
using remove_cvref_t = remove_cv_t<typename remove_reference<T>::type>;
template <bool Condition, class T = void>
struct enable_if{};
template <class T>
struct enable_if<true, T>{
using type = T;
};
template <bool Condition, class T = void>
using enable_if_t = typename enable_if<Condition, T>::type;
template <class...>
using void_t = void;
template <class T>
T&& declval() noexcept;
#endif
template <class T>
static constexpr bool supported_uint =
is_same<remove_cvref_t<T>, uint8_t> ||
is_same<remove_cvref_t<T>, uint16_t> ||
is_same<remove_cvref_t<T>, uint32_t> ||
is_same<remove_cvref_t<T>, uint64_t>;
template <class T>
static constexpr bool supported_integer =
supported_uint<T> ||
is_same<remove_cvref_t<T>, int8_t> ||
is_same<remove_cvref_t<T>, int16_t> ||
is_same<remove_cvref_t<T>, int32_t> ||
is_same<remove_cvref_t<T>, int64_t>;
template <size_t Size>
struct uint_of_size;
template <>
struct uint_of_size<1>{ using type = uint8_t; };
template <>
struct uint_of_size<2>{ using type = uint16_t; };
template <>
struct uint_of_size<4>{ using type = uint32_t; };
template <>
struct uint_of_size<8>{ using type = uint64_t; };
template <class T>
using unsigned_t = typename uint_of_size<sizeof(remove_cvref_t<T>)>::type;
template <class T>
constexpr unsigned bit_width() noexcept{
#if RND_DETAIL_HAS_STANDARD_COMPAT
return std::numeric_limits<remove_cvref_t<T>>::digits;
#else
return sizeof(remove_cvref_t<T>) * CHAR_BIT;
#endif
}
template <class I>
constexpr uint64_t integral_max() noexcept{
using value_type = remove_cvref_t<I>;
#if RND_DETAIL_HAS_STANDARD_COMPAT
return static_cast<uint64_t>((std::numeric_limits<value_type>::max)());
#else
using U = unsigned_t<value_type>;
if constexpr(value_type(-1) < value_type(0)){
return uint64_t{static_cast<U>(~U{0}) >> 1};
}else{
return uint64_t{static_cast<U>(~U{0})};
}
#endif
}
template <class UInt>
constexpr unsigned power_of_two_exponent(UInt value) noexcept{
static_assert(supported_uint<UInt>,
"power_of_two_exponent() requires a supported unsigned integer type");
#if defined(__cpp_lib_bitops) && __cpp_lib_bitops >= 201907L
return static_cast<unsigned>(std::countr_zero(value));
#else
unsigned exponent{};
while(value > 1){
value >>= 1;
++exponent;
}
return exponent;
#endif
}
template <class F>
struct float_traits{
static constexpr bool supported = false;
static constexpr unsigned mantissa_bits = 0;
};
template <>
struct float_traits<float>{
static constexpr bool supported =
sizeof(float) == 4 && FLT_RADIX == 2 &&
FLT_MANT_DIG == 24 && FLT_MAX_EXP == 128;
static constexpr unsigned mantissa_bits = FLT_MANT_DIG - 1;
};
template <>
struct float_traits<double>{
static constexpr bool supported =
FLT_RADIX == 2 &&
((sizeof(double) == 4 && DBL_MANT_DIG == 24 && DBL_MAX_EXP == 128) ||
(sizeof(double) == 8 && DBL_MANT_DIG == 53 && DBL_MAX_EXP == 1024));
static constexpr unsigned mantissa_bits = DBL_MANT_DIG - 1;
};
template <class F>
static constexpr bool supported_float = float_traits<remove_cvref_t<F>>::supported;
template <class F>
constexpr unsigned_t<F> floating_one_bits() noexcept{
if constexpr(sizeof(remove_cvref_t<F>) == 4){
return UINT32_C(0x3f800000);
}else{
return UINT64_C(0x3ff0000000000000);
}
}
template <class F>
RND_DETAIL_FLOAT_CONSTEXPR remove_cvref_t<F>
unit_float_from_mantissa(unsigned_t<F> mantissa) noexcept{
static_assert(supported_float<F>, "unit_float_from_mantissa() requires a supported floating-point type");
using real_type = remove_cvref_t<F>;
using UInt = unsigned_t<real_type>;
#if RND_DETAIL_HAS_CONSTEXPR_BIT_CAST
constexpr UInt base = std::bit_cast<UInt>(real_type{1});
const UInt representation = static_cast<UInt>(base | mantissa);
return std::bit_cast<real_type>(representation) - real_type{1};
#elif defined(RND_FAST_FLOAT)
const UInt representation =
static_cast<UInt>(floating_one_bits<real_type>() | mantissa);
real_type value;
memcpy(&value, &representation, sizeof(value));
return value - real_type{1};
#else
constexpr unsigned mantissa_bits = float_traits<real_type>::mantissa_bits;
constexpr real_type scale = real_type{1} /
static_cast<real_type>(UInt{1} << mantissa_bits);
return static_cast<real_type>(mantissa) * scale;
#endif
}
#if RND_DETAIL_HAS_STANDARD_COMPAT
template <class C>
constexpr auto collection_data(C& collection) noexcept(noexcept(std::data(collection)))
-> decltype(std::data(collection)){
return std::data(collection);
}
template <class C>
constexpr auto collection_size(const C& collection) noexcept(noexcept(std::size(collection)))
-> decltype(std::size(collection)){
return std::size(collection);
}
template <class C>
constexpr auto collection_begin(C& collection) noexcept(noexcept(std::begin(collection)))
-> decltype(std::begin(collection)){
return std::begin(collection);
}
#else
template <class C>
constexpr auto collection_data(C& collection) noexcept -> decltype(collection.data()){
return collection.data();
}
template <class T, size_t N>
constexpr T* collection_data(T (&collection)[N]) noexcept{
return collection;
}
template <class C>
constexpr auto collection_size(const C& collection) noexcept -> decltype(collection.size()){
return collection.size();
}
template <class T, size_t N>
constexpr size_t collection_size(const T (&)[N]) noexcept{
return N;
}
template <class C>
constexpr auto collection_begin(C& collection) noexcept -> decltype(collection.begin()){
return collection.begin();
}
template <class T, size_t N>
constexpr T* collection_begin(T (&collection)[N]) noexcept{
return collection;
}
#endif
struct priority_0{};
struct priority_1 : priority_0{};
struct priority_2 : priority_1{};
template <class Projection, class T>
constexpr auto invoke_impl(Projection& projection, T& value, priority_2)
noexcept(noexcept(projection(value))) -> decltype(projection(value)){
return projection(value);
}
template <class Projection, class T>
constexpr auto invoke_impl(Projection& projection, T& value, priority_1)
noexcept(noexcept((value.*projection)())) -> decltype((value.*projection)()){
return (value.*projection)();
}
template <class Projection, class T>
constexpr auto invoke_impl(Projection& projection, T& value, priority_0)
noexcept(noexcept(value.*projection)) -> decltype(value.*projection){
return value.*projection;
}
template <class Projection, class T>
constexpr auto invoke(Projection& projection, T& value)
noexcept(noexcept(invoke_impl(projection, value, priority_2{})))
-> decltype(invoke_impl(projection, value, priority_2{})){
return invoke_impl(projection, value, priority_2{});
}
template <class C, class = void>
struct contiguous_collection{
static constexpr bool value = false;
};
template <class C>
struct contiguous_collection<C, void_t<
decltype(collection_data(declval<C&>())),
decltype(collection_size(declval<const C&>())),
decltype(collection_begin(declval<C&>()))>>{
static constexpr bool value = true;
};
template <class T, class Projection, class Result, class = void>
struct valid_projection{
static constexpr bool value = false;
};
template <class T, class Projection, class Result>
struct valid_projection<T, Projection, Result, void_t<
decltype(invoke(declval<Projection&>(), declval<T&>()))>>{
using weight_type = remove_cvref_t<decltype(invoke(
declval<Projection&>(), declval<T&>()))>;
static constexpr bool value =
noexcept(invoke(declval<Projection&>(), declval<T&>())) &&
supported_uint<weight_type> &&
(sizeof(weight_type) <= sizeof(Result));
};
template <class C, class Result, class = void>
struct valid_weight_collection{
static constexpr bool value = false;
};
template <class C, class Result>
struct valid_weight_collection<C, Result, void_t<
decltype(*collection_data(declval<const C&>()))>>{
using weight_type = remove_cvref_t<decltype(*collection_data(declval<const C&>()))>;
static constexpr bool value =
contiguous_collection<const C>::value &&
supported_uint<weight_type> &&
(sizeof(weight_type) <= sizeof(Result));
};
template <class C, class Projection, class Result, class = void>
struct valid_projected_collection{
static constexpr bool value = false;
};
template <class C, class Projection, class Result>
struct valid_projected_collection<C, Projection, Result, void_t<
decltype(*collection_data(declval<C&>()))>>{
using element_type = decltype(*collection_data(declval<C&>()));
static constexpr bool value =
contiguous_collection<C>::value &&
valid_projection<element_type, Projection, Result>::value;
};
}
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
#include <assert.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#if defined(__cpp_concepts) && __cpp_concepts >= 201907L
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
#endif
namespace rnd {
template <class E>
class Random final{
using engine_result_type = typename E::result_type;
static constexpr unsigned value_bits = detail::bit_width<engine_result_type>();
static_assert(detail::supported_uint<engine_result_type>, "Random<E> requires an 8-, 16-, 32-, or 64-bit unsigned result_type");
static_assert((E::min)() == engine_result_type{0},
"Random<E> requires an engine whose minimum is zero");
static_assert((E::max)() == static_cast<engine_result_type>(~engine_result_type{0}),
"Random<E> requires an engine spanning its complete result_type");
template <class T>
static constexpr bool valid_weight_type =
detail::supported_uint<T> &&
(sizeof(detail::remove_cvref_t<T>) <= sizeof(engine_result_type));
public:
using engine_type = E;
using result_type = typename E::result_type;
using seed_type = typename E::seed_type;
constexpr Random() noexcept = default;
explicit constexpr Random(seed_type seed_value) noexcept : _engine(seed_value){}
explicit constexpr Random(engine_type engine) noexcept : _engine(engine){}
constexpr bool operator==(const Random& rhs) const noexcept{ return _engine == rhs._engine; }
constexpr bool operator!=(const Random& rhs) const noexcept{ return !(*this == rhs); }
constexpr const engine_type& engine() const noexcept{ return _engine; }
constexpr engine_type& engine() noexcept{ return _engine; }
constexpr void seed() noexcept{ _engine.seed(); }
constexpr void seed(seed_type value) noexcept{ _engine.seed(value); }
constexpr void discard(unsigned long long count) noexcept{ _engine.discard(count); }
static constexpr result_type (min)() noexcept{
return result_type{0};
}
static constexpr result_type (max)() noexcept{
return (E::max)();
}
constexpr result_type next() noexcept{ return _engine(); }
constexpr result_type operator()() noexcept{ return next(); }
template <class T = result_type>
constexpr T bits(unsigned n) noexcept{
static_assert(detail::supported_uint<T>, "Random::bits<T>() requires an 8-, 16-, 32-, or 64-bit unsigned type");
assert(n > 0 && n <= detail::bit_width<T>());
if(n <= value_bits){
return take_high_bits<T>(next(), n);
}
return gather_high_bits<T>(n);
}
template <unsigned N, class T = result_type>
constexpr T bits() noexcept{
static_assert(N > 0, "Random::bits<N>() needs at least one bit");
static_assert(detail::supported_uint<T>, "Random::bits<N, T>() requires an 8-, 16-, 32-, or 64-bit unsigned type");
static_assert(N <= detail::bit_width<T>(), "T cannot hold N bits");
if constexpr(N <= value_bits){
return take_high_bits<T>(next(), N);
}else{
return gather_high_bits<T>(N);
}
}
template <class T>
constexpr T bits_as() noexcept{
return bits<detail::bit_width<T>(), T>();
}
template <class T>
constexpr void fill_bits(T* buffer, size_t count) noexcept{
static_assert(detail::supported_uint<T>, "Random::fill_bits<T>() requires an 8-, 16-, 32-, or 64-bit unsigned type");
assert(buffer != nullptr || count == 0);
if(buffer == nullptr || count == 0){
return;
}
constexpr unsigned target_bits = detail::bit_width<T>();
if constexpr(value_bits == target_bits){
for(size_t i = 0; i < count; ++i){
buffer[i] = static_cast<T>(next());
}
}else if constexpr(value_bits > target_bits){
static_assert(value_bits % target_bits == 0, "Random::fill_bits<T>() requires evenly divisible bit widths");
constexpr unsigned values_per_draw = value_bits / target_bits;
size_t i{};
while(i < count){
const result_type value = next();
for(unsigned part = 0; part < values_per_draw && i < count; ++part){
const unsigned shift = value_bits - target_bits * (part + 1);
buffer[i++] = static_cast<T>(value >> shift);
}
}
}else{
static_assert(target_bits % value_bits == 0, "Random::fill_bits<T>() requires evenly divisible bit widths");
constexpr unsigned draws_per_value = target_bits / value_bits;
for(size_t i = 0; i < count; ++i){
T value{};
for(unsigned part = 0; part < draws_per_value; ++part){
value = static_cast<T>((value << value_bits) | static_cast<T>(next()));
}
buffer[i] = value;
}
}
}
[[nodiscard]] constexpr Random split() noexcept{
return Random{bits_as<seed_type>()};
}
constexpr result_type next(result_type bound) noexcept{
assert(bound > 0 && "Random::next(bound): bound must be positive.");
return scale_to_bound(next(), bound);
}
constexpr result_type operator()(result_type bound) noexcept{ return next(bound); }
template <result_type Bound, class T = result_type>
constexpr T next() noexcept{
static_assert(Bound > 0, "Random::next<Bound>(): bound must be positive");
static_assert(detail::supported_integer<T>, "Random::next<Bound, T>() requires a supported fixed-width integer type");
static_assert(uint64_t{Bound - 1} <= detail::integral_max<T>(), "Bound is too large for return type T");
if constexpr(Bound == 1){
return T{0};
}else if constexpr((Bound & (Bound - 1)) == 0){
using U = detail::unsigned_t<T>;
return static_cast<T>(bits<detail::power_of_two_exponent(Bound), U>());
}else{
return static_cast<T>(next(Bound));
}
}
template <class I, detail::enable_if_t<detail::supported_integer<I>, int> = 0>
constexpr I between(I lo, I hi) noexcept{
if(!(lo < hi)){
assert(false && "Random::between(lo, hi): inverted or empty range.");
return lo;
}
using U = detail::unsigned_t<I>;
const U bound = static_cast<U>(hi) - static_cast<U>(lo);
assert(uint64_t{bound} <= uint64_t{(max)()} && "Random::between(lo, hi): range is too large for this engine.");
return static_cast<I>(static_cast<U>(lo) +
static_cast<U>(next(static_cast<result_type>(bound))));
}
template <class F = float>
RND_DETAIL_FLOAT_CONSTEXPR F normalized() noexcept{
static_assert(detail::supported_float<F>,
"Random floating-point functions require IEEE-754 binary32 float or binary32/binary64 double");
using real_type = detail::remove_cvref_t<F>;
using UInt = detail::unsigned_t<real_type>;
constexpr unsigned mantissa_bits = detail::float_traits<real_type>::mantissa_bits;
const UInt mantissa = bits<mantissa_bits, UInt>();
return detail::unit_float_from_mantissa<real_type>(mantissa);
}
template <class F = float>
RND_DETAIL_FLOAT_CONSTEXPR F signed_norm() noexcept{
static_assert(detail::supported_float<F>,
"Random::signed_norm() requires a supported floating-point type");
return F{2} * normalized<F>() - F{1};
}
template <class F, detail::enable_if_t<detail::supported_float<F>, int> = 0>
RND_DETAIL_FLOAT_CONSTEXPR F between(F lo, F hi) noexcept{
assert(lo < hi && "Random::between(lo, hi): inverted or empty range.");
return lo + (hi - lo) * normalized<F>();
}
constexpr bool coin_flip() noexcept{
return bits<1, unsigned>() != 0;
}
template <class F = float>
RND_DETAIL_FLOAT_CONSTEXPR bool coin_flip(F probability) noexcept{
static_assert(detail::supported_float<F>, "Random::coin_flip(probability) requires a supported floating-point type");
assert(F{0} <= probability && probability <= F{1} && "Random::coin_flip(probability): probability must be in [0, 1].");
return normalized<F>() < probability;
}
template <class F = float>
RND_DETAIL_FLOAT_CONSTEXPR F gaussian(F mean, F stddev) noexcept{
static_assert(detail::supported_float<F>, "Random::gaussian() requires a supported floating-point type");
assert(stddev >= F{0} && "Random::gaussian(mean, stddev): standard deviation must be non-negative.");
if constexpr(value_bits >= 32){
F sum{};
for(unsigned i = 0; i < 12; ++i){
sum += normalized<F>();
}
return mean + (sum - F{6}) * stddev;
}else{
uint16_t lanes[12]{};
fill_bits<uint16_t>(lanes, 12);
uint32_t sum{};
for(const uint16_t lane : lanes){
sum += static_cast<uint32_t>(lane);
}
const F normalized_sum = (static_cast<F>(sum) + F{6}) / F{65536};
return mean + (normalized_sum - F{6}) * stddev;
}
}
[[nodiscard]] constexpr size_t index(size_t size) noexcept{
assert(size != 0 && "Random::index(): empty collection.");
assert(size <= static_cast<size_t>((max)()) && "Random::index(): collection is too large for this engine.");
return static_cast<size_t>(next(static_cast<result_type>(size)));
}
template <class C,
detail::enable_if_t<detail::contiguous_collection<const C>::value, int> = 0>
[[nodiscard]] constexpr size_t index(const C& collection) noexcept{
return index(static_cast<size_t>(detail::collection_size(collection)));
}
template <class T>
[[nodiscard]] constexpr T* iterator(T* collection, size_t size) noexcept{
assert(collection != nullptr && "Random::iterator(): null collection.");
return collection + index(size);
}
template <class C,
detail::enable_if_t<detail::contiguous_collection<C>::value, int> = 0>
[[nodiscard]] constexpr auto iterator(C& collection) noexcept{
return detail::collection_begin(collection) +
index(static_cast<size_t>(detail::collection_size(collection)));
}
template <class T>
[[nodiscard]] constexpr T& element(T* collection, size_t size) noexcept{
return *iterator(collection, size);
}
template <class C,
detail::enable_if_t<detail::contiguous_collection<C>::value, int> = 0>
[[nodiscard]] constexpr decltype(auto) element(C& collection) noexcept{
return *iterator(collection);
}
template <class W, detail::enable_if_t<valid_weight_type<W>, int> = 0>
[[nodiscard]] constexpr size_t weighted_index(const W* weights, size_t size) noexcept{
if(weights == nullptr){
assert(false && "Random::weighted_index(): null weight collection.");
abort();
}
auto weight_at = [weights](size_t i) constexpr -> decltype(auto){ return weights[i]; };
return weighted_offset(size, weight_at);
}
template <class C,
detail::enable_if_t<detail::valid_weight_collection<C, result_type>::value, int> = 0>
[[nodiscard]] constexpr size_t weighted_index(const C& weights) noexcept{
return weighted_index(
detail::collection_data(weights),
static_cast<size_t>(detail::collection_size(weights)));
}
template <class T, class Projection,
detail::enable_if_t<detail::valid_projection<T, Projection, result_type>::value, int> = 0>
[[nodiscard]] constexpr T* weighted_iterator(
T* collection, size_t size, Projection projection) noexcept{
return collection + projected_weighted_offset(collection, size, projection);
}
template <class C, class Projection,
detail::enable_if_t<detail::valid_projected_collection<C, Projection, result_type>::value, int> = 0>
[[nodiscard]] constexpr auto weighted_iterator(C& collection, Projection projection) noexcept{
const size_t offset = projected_weighted_offset(
detail::collection_data(collection),
static_cast<size_t>(detail::collection_size(collection)),
projection);
return detail::collection_begin(collection) + offset;
}
template <class T, class Projection,
detail::enable_if_t<detail::valid_projection<T, Projection, result_type>::value, int> = 0>
[[nodiscard]] constexpr T& weighted_element(
T* collection, size_t size, Projection projection) noexcept{
return *weighted_iterator(collection, size, projection);
}
template <class C, class Projection,
detail::enable_if_t<detail::valid_projected_collection<C, Projection, result_type>::value, int> = 0>
[[nodiscard]] constexpr decltype(auto) weighted_element(C& collection, Projection projection) noexcept{
return *weighted_iterator(collection, projection);
}
private:
engine_type _engine{};
template <class T>
static constexpr T low_bits_mask(unsigned n) noexcept{
return n >= detail::bit_width<T>()
? static_cast<T>(~T{0})
: static_cast<T>((T{1} << n) - T{1});
}
template <class T>
static constexpr T take_high_bits(result_type value, unsigned n) noexcept{
assert(n > 0 && n <= value_bits && n <= detail::bit_width<T>());
return static_cast<T>(value >> (value_bits - n)) & low_bits_mask<T>(n);
}
template <class T>
constexpr T gather_high_bits(unsigned n) noexcept{
assert(value_bits < n && n <= detail::bit_width<T>());
T result{};
unsigned filled{};
while(filled < n){
const unsigned remaining = n - filled;
const unsigned take = remaining < value_bits ? remaining : value_bits;
const T chunk = take_high_bits<T>(next(), take);
result = static_cast<T>(result | static_cast<T>(chunk << filled));
filled += take;
}
return static_cast<T>(result & low_bits_mask<T>(n));
}
template <class WeightAt>
constexpr result_type total_weight(size_t size, WeightAt& weight_at) noexcept{
using weight_type = detail::remove_cvref_t<decltype(weight_at(size_t{}))>;
static_assert(valid_weight_type<weight_type>, "Weights must be non-boolean unsigned integers no wider than result_type");
result_type total{};
for(size_t i = 0; i < size; ++i){
const result_type weight = static_cast<result_type>(weight_at(i));
if(weight > (max)() - total){
assert(false && "Random::weighted_index(): total weight is too large for this engine.");
abort();
}
total += weight;
}
return total;
}
template <class T, class Projection>
constexpr size_t projected_weighted_offset(T* collection, size_t size, Projection& projection) noexcept{
if(collection == nullptr){
assert(false && "Random::weighted_iterator(): null collection.");
abort();
}
static_assert(detail::valid_projection<T, Projection, result_type>::value, "Projection must be noexcept and return a valid unsigned weight type");
auto weight_at = [collection, &projection](size_t i) constexpr noexcept -> decltype(auto){
return detail::invoke(projection, collection[i]);
};
return weighted_offset(size, weight_at);
}
template <class WeightAt>
constexpr size_t weighted_offset(size_t size, WeightAt& weight_at) noexcept{
if(size == 0){
assert(false && "Random::weighted_index(): empty weight collection.");
abort();
}
result_type total = total_weight(size, weight_at);
if(total == 0){
assert(false && "Random::weighted_index(): at least one weight must be positive.");
abort();
}
result_type target = next(total);
for(size_t i = 0; i < size; ++i){
const result_type weight = static_cast<result_type>(weight_at(i));
if(target < weight){
return i;
}
target -= weight;
}
assert(false && "Random::weighted_index(): weights changed during selection.");
abort();
}
static constexpr result_type scale_to_bound(result_type value, result_type bound) noexcept{
if constexpr(sizeof(result_type) == 1){
return static_cast<result_type>((uint16_t{value} * uint16_t{bound}) >> 8);
}else if constexpr(sizeof(result_type) == 2){
return static_cast<result_type>((uint32_t{value} * uint32_t{bound}) >> 16);
}else if constexpr(sizeof(result_type) == 4){
return static_cast<result_type>((uint64_t{value} * uint64_t{bound}) >> 32);
}else{
return static_cast<result_type>(detail::mul64_to_128_parts(value, bound).hi);
}
}
};
}
#undef RND_DETAIL_FLOAT_CONSTEXPR
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
#include <assert.h>
#include <stdint.h>
class RomuDuoJr final{
using u64 = uint64_t;
using state_type = u64;
state_type x;
state_type y;
struct Direct{};
constexpr RomuDuoJr(state_type xstate, state_type ystate, Direct) noexcept
: x(xstate), y(ystate){
assert((x | y) != 0 && "RomuDuoJr all-zero state is invalid");
}
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
