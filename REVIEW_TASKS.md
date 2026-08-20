# Repository review follow-up

This checklist turns the August 2026 repository review into an implementation
backlog. It is ordered primarily by correctness and dependency, not by how easy
each change is. Each item should be small enough to discuss, implement, and
review independently.

## Working decisions

These are the proposed defaults for the work below. They can be revised when a
task uncovers contrary evidence, but recording them here prevents the same API
question from being reopened accidentally in several tasks.

- `next(bound)` is unbiased. A compile-time power-of-two bound still takes the
  genuine bit-extraction fast path.
- Do not add a biased API until benchmarks show a worthwhile use case. If one is
  added, name it `next_biased()`, not `next_fast()`: speed is an implementation
  property, while bias is part of the contract. High-level helpers remain
  unbiased.
- Keep `Random<E>::result_type` as the underlying engine's raw result type, as
  expected of a URBG. Do not use that type as the high-level API's maximum
  bound, collection size, or total weight; gather additional engine output when
  the requested integer type is wider.
- Standardize the built-in engines on a `uint64_t` user-facing `seed_type`.
  Narrow engines cannot map every 64-bit seed to a unique state, but they can
  mix the seed across substantially more of their state than an 8- or 16-bit
  seed permits.
- Rename the approximate distribution to `normal_approx(mean, stddev)`.
- Rename generic `Random<E>::split()` to `child()`. Preserve genuinely
  engine-specific stream/jump operations under their established terminology.
- Represent a replay snapshot with an engine-specific aggregate `state_type`,
  returned by value from `state()` and accepted by `from_state(state_type)`.
  Do not require every engine to expose its internal storage as an array or
  span.
- Reorder the README and add a statistical-quality section after the relevant
  API and packaging work has settled.

## P0: Correctness

### 1. Make bounded integer generation unbiased

- [x] Replace the current multiply-high-only `scale_to_bound()` path with
  Lemire reduction including its rejection step.
- [x] Preserve the current compile-time special cases: `next<1>()` consumes no
  engine output, and a power-of-two bound uses exact bit extraction rather than
  entering the rejection loop.
- [x] Make `next(bound)`, `between()`, `index()`, and weighted selection share
  the unbiased contract; none of the ordinary convenience functions may call a
  biased reduction internally.
- [x] Add tests that make the narrow-engine failure mode visible. In particular,
  exercise non-power-of-two 8-bit bounds such as 10 and 129, rejection near a
  type's maximum, power-of-two bounds, bound 1, deterministic consumption, and
  `constexpr` evaluation.
- [x] Update reference expectations affected by rejected draws, while retaining
  tests that distinguish implementation/reference-sequence validation from
  statistical validation.
- [ ] Re-run the bounded-integer benchmarks and record the new unbiased results.
  Use those measurements to decide whether a separate `next_biased()` task is
  justified; do not add it speculatively.

Implementation and test work is complete. The benchmark bullet remains open
because the repository's benchmark links target Quick Bench rather than a local
benchmark executable.

## P1: Settle the pre-1.0 API

### 2. Move public headers under `include/rnd` and namespace all public types

- [x] Move the public tree from `includes/` to conventional scoped paths such as
  `include/rnd/random.hpp`, `include/rnd/concepts.hpp`, and
  `include/rnd/engines/romuduojr.hpp`.
- [x] Put every bundled engine and `RandomBitEngine` in namespace `rnd`, so the
  canonical spelling is `rnd::Random<rnd::RomuDuoJr>`.
- [x] Update internal includes, tests, benchmarks, examples, CMake include
  directories, Arduino flags, README snippets, and links in one mechanical
  change.
- [x] Decide explicitly whether pre-1.0 compatibility forwarding headers are
  valuable. Prefer a clean break unless there are known consumers that need a
  migration window.
- [x] Add a consumer smoke test that includes only installed-style paths, so
  accidental dependence on the repository layout is caught.

Acceptance: no public engine or concept leaks into the global namespace, and a
consumer can compile using only `#include <rnd/...>` paths.

### 3. Remove engine width as a high-level API limit

Depends on task 1, because every width-specific path needs the same unbiased
reduction semantics.

- [ ] Add an unsigned typed bounded primitive along the lines of
  `template<class U> U next(U bound)`, supporting the library's fixed 8-, 16-,
  32-, and 64-bit unsigned types.
- [ ] Use a native-width path when suitable. When `U` is wider than the engine
  result, gather enough uniformly distributed bits (using the existing bit
  machinery) and perform reduction at `U`'s width.
- [ ] Preserve `next()` and `result_type` as raw engine-facing APIs. Clearly
  document the distinction between raw output width and high-level request
  width.
- [ ] Make `index(size_t)` work with narrow engines for collections larger than
  255 or 65,535.
- [ ] Make integer `between()` request the unsigned width needed by its argument
  type rather than asserting against `E::max()`.
- [ ] Redesign weighted accumulation so a narrow engine can select from wider
  weights and totals. Choose and document a checked accumulator limit (normally
  `uint64_t`), detect overflow before the draw, and keep zero-weight behavior.
- [ ] Relax projection/weight constraints that currently reject a weight merely
  because it is wider than the engine result.
- [ ] Add tests for `Random<SmallFast8>` selecting from 300+ elements, accepting
  totals above 255, and generating wider bounded integers without changing the
  cheap native-width cases.
- [ ] Benchmark native-width and gathered-width calls separately. Narrow-engine
  convenience is allowed to cost more only when the caller asks for a wider
  result.

Acceptance: swapping a 32- or 64-bit engine for an 8-bit engine does not make an
otherwise representable bound, collection size, or weight total invalid.

### 4. Make signed `between()` portable under C++17

Depends on task 3 so the range calculation and bounded draw are fixed once.

- [ ] Add a signed reconstruction helper that maps an unsigned offset back to
  the requested signed interval mathematically, without relying on an
  out-of-range unsigned-to-signed conversion.
- [ ] Cover ranges wholly below zero, crossing zero, near both signed limits,
  and the full `[min, max)` range for each supported signed width.
- [ ] Run these checks in `constexpr` C++17 tests as well as the normal suite.
- [ ] Tighten the README's cross-platform integer reproducibility claim to match
  the behavior actually guaranteed by C++17.

Acceptance: no integer result depends on implementation-defined
unsigned-to-signed conversion, including ranges that cross zero.

### 5. Standardize the built-in seeding contract

This deliberately changes some seed-to-sequence mappings and therefore belongs
before a stable release contract is declared.

- [ ] Give every bundled engine `using seed_type = uint64_t` and a corresponding
  constructor and `seed(uint64_t)` overload.
- [ ] For `SmallFast8`, `SmallFast16`, and `XorShift32Star8`, define a small,
  reproducible mixing/downfolding scheme. Populate as much valid internal state
  as possible instead of simply truncating and repeating the low seed bits.
- [ ] Keep algorithm-reference tests independent from public seed expansion by
  constructing known raw states where necessary.
- [ ] Test that high seed bits affect narrow-engine state/output, zero is valid as
  a user seed, default construction remains deterministic, and seeding is
  `constexpr` on supported targets.
- [ ] Document that 64-bit seeds are a common input interface, not a promise of
  2^64 distinct streams for engines with smaller state spaces.
- [ ] State when seed-to-sequence mappings become compatibility commitments.

Acceptance: generic code can seed any bundled engine with the same 64-bit value,
and changing only high seed bits is not silently ignored by narrow engines.

### 6. Add a complete state snapshot and replay API

- [ ] Define a public aggregate `state_type` for every engine that represents its
  complete logical state. Use named scalar fields where that is clearer than an
  array; avoid exposing references to private storage.
- [ ] Add `constexpr state_type state() const noexcept` and
  `static constexpr Engine from_state(state_type) noexcept` consistently.
- [ ] Refactor existing multi-argument `from_state(...)` factories to the common
  shape. If temporary compatibility overloads are retained, mark their intended
  removal point.
- [ ] Validate engine invariants at restoration boundaries, including all-zero
  forbidden states and PCG's odd increment.
- [ ] Add round-trip tests: advance, snapshot, restore, compare, and verify a
  subsequent output sequence. Cover every engine and `constexpr` use.
- [ ] Document the portable serialization contract as numeric state fields and
  engine identity/version—not the engine object's byte representation, padding,
  native endianness, or raw `memcpy` bytes.
- [ ] Correct `Random::engine()` documentation: it enables engine-specific
  access, while the new state API is what makes manual serialization practical.

Acceptance: every engine can be paused and restored without private-member
access, and the restored engine reproduces the same future sequence.

### 7. Rename misleading convenience APIs

- [ ] Rename `Random<E>::split()` to `child()`. Define it as consuming enough
  parent output to construct one common seed and returning a deterministically
  derived generator; do not promise independent streams or substreams.
- [ ] Keep true engine-specific stream, split, or jump facilities separate and
  document their stronger engine-specific guarantees individually.
- [ ] Rename `gaussian()` to `normal_approx()` and document its Irwin-Hall
  sum-of-12 construction, bounded support (roughly six standard deviations on
  either side), and intended non-cryptographic/game use.
- [ ] Decide whether source compatibility aliases are useful. Prefer direct
  pre-1.0 renames; if aliases are added, deprecate them and give them a removal
  version rather than maintaining two permanent names.
- [ ] Update tests, examples, benchmarks, API tables, and comments.

Acceptance: the generic child operation is not described with stream-splitting
terminology, and the distribution's approximate nature is visible at each call
site.

### 8. Define one invalid-input and projection policy

- [ ] Inventory all asserts, fallback returns, and unconditional `abort()` calls
  in the public API.
- [ ] Choose a consistent low-level precondition model. A suitable default is:
  document required preconditions, diagnose them with debug assertions, and do
  not promise recovery in release builds.
- [ ] Ensure reference-returning functions and internal "weights changed between
  passes" failures cannot fall through to an invalid reference.
- [ ] Document that weighted projections are `noexcept`, are evaluated in two
  passes, and must return stable non-negative integral values during one call.
- [ ] Add death/assert tests where practical and ordinary tests for all valid
  boundary inputs. Confirm the chosen mechanism is viable on AVR.

Acceptance: every public function follows one documented contract-violation
policy, and weighted selection's callable requirements are visible to users.

## P2: Repository and distribution cleanup

### 9. Replace obsolete and misleading project artifacts

- [ ] Remove the superseded `msvc/` solution, project, package metadata, and
  vendored GoogleTest 1.8.1 package after confirming CMake covers the same MSVC
  configurations.
- [ ] Replace root `demo.cpp` with a normal local C++17 example under `examples/`.
  Keep the Compiler Explorer URL in documentation rather than as source-file
  includes.
- [ ] Build and run (where applicable) the example in CI so public snippets
  cannot silently rot.
- [ ] Move the C++23-only `wide_multiply.hpp` out of the C++17 public header tree,
  or remove it if the portable implementation has superseded it. Retain any
  useful validation helper under `tests/`.
- [ ] Add a check that each public header compiles in the advertised C++17 mode.

Acceptance: the public tree contains only supported C++17 library headers, and
the repository has one obvious, locally compilable introductory example.

### 10. Add installable CMake packaging

Do this after the include layout is final. It is optional if the project
explicitly supports only copied headers and `add_subdirectory()`, but worthwhile
before package-manager distribution.

- [ ] Add separate `BUILD_INTERFACE` and `INSTALL_INTERFACE` include paths.
- [ ] Install the `include/rnd` tree and export the
  `cpp_prngs::cpp_prngs` target.
- [ ] Generate/install package config and version files with an explicitly chosen
  compatibility policy.
- [ ] Add a clean consumer test that configures against the installed package,
  not the source checkout.
- [ ] Document `add_subdirectory`, installation, and copied-header workflows
  without implying support for untested mechanisms.

Acceptance: a separate CMake project can `find_package(cpp_prngs)` from a staged
installation and compile an `#include <rnd/random.hpp>` consumer.

## P3: Evidence and documentation

### 11. Build a statistical-quality evidence inventory

- [ ] Create one record per engine containing its algorithm/source, upstream
  designer tests, independent published tests, known failures or limitations,
  tested version/parameters, and links to primary evidence.
- [ ] Separate three kinds of evidence explicitly: reference-sequence tests in
  this repository, statistical testing of generator output, and testing of this
  repository's seed-to-state mapping.
- [ ] Be especially explicit about the narrow engines: small output/state spaces,
  expected test limitations, and which upstream results apply to the exact
  constants used here.
- [ ] Record unknown or untested areas instead of converting absence of evidence
  into a quality claim.
- [ ] Decide whether to run an independent PractRand/TestU01-style campaign. If
  so, make its commands, engine adapters, versions, sample sizes, and raw results
  reproducible and keep large generated artifacts out of the public headers.

Acceptance: every statistical claim in the README can be traced to a precise
upstream or independently reproducible source, and implementation conformance is
never presented as a statistical test.

### 12. Reorder and update the README

Depends on the public API, layout, and evidence inventory being stable enough
that this is not immediately rewritten.

- [ ] Lead with a short description, a prominent non-cryptographic warning, and
  a minimal `RomuDuoJr` example.
- [ ] Follow with engine selection guidance and the engine table. Move the longer
  case against `rand()`, Mersenne Twister, standard distributions, and related
  facilities into a later "Why this library exists" section.
- [ ] Add the statistical-quality section before performance. Summarize upstream
  and independent evidence, known limitations, and untested areas from task 11.
- [ ] Update all include paths, namespaces, bounded-integer guarantees, width
  behavior, common seeding semantics, replay API, renamed functions, projection
  requirements, and invalid-input policy.
- [ ] Preserve the useful API tables, AVR instructions, portability explanation,
  benchmark caveats, attributions, and citations.
- [ ] Replace claims such as "proved it robust" with language matching the
  evidence, such as reporting that a named test found no short cycles up to a
  stated limit.
- [ ] Verify every README code block by compiling extracted snippets or by making
  them share source with tested examples.

Acceptance: the first screenful tells a new user what the library is, how to use
it, and that it is not cryptographically secure; quality and performance claims
are specific and supportable.

### 13. Resolve the external seeding helper's status

- [ ] Choose whether the external `seeding.hpp` gist is part of cpp_prngs. The
  preferred options are either (a) bring a supported subset into
  `extras/seeding.hpp` with tests, versioning, licensing, and clear entropy
  caveats, or (b) label it plainly as separate, unsupported example material.
- [ ] Do not present timestamps, thread IDs, addresses/ASLR, or compilation
  metadata as equivalent to operating-system entropy.
- [ ] Align every example with the common 64-bit seed contract from task 5.
- [ ] Test any helper shipped in the repository on its claimed platforms and
  distinguish reproducibility, run-to-run variation, and unpredictability.

Acceptance: users can tell which seeding code is maintained with the library and
what security/entropy property each source does—or does not—provide.

### 14. Run a final public-contract audit

- [ ] Build the normal test suite on Windows, Linux, and macOS in C++17 mode.
- [ ] Compile the Arduino ATmega32U4 validation in both floating-point modes.
- [ ] Run the installed-package consumer and the local example.
- [ ] Check that public headers contain no accidental post-C++17 constructs and
  introduce no global public names.
- [ ] Confirm deterministic reference sequences, seed mappings, and state
  serialization examples are deliberately versioned/documented wherever users
  may depend on them.
- [ ] Review benchmark and statistical text one final time for claims stronger
  than the recorded evidence.

Acceptance: the advertised API, portability, reproducibility, packaging, and
quality statements all have a corresponding automated check or a clearly
documented limitation.

## Explicitly deferred

- A biased `next_biased()` primitive, pending post-task-1 benchmark evidence.
- Biased variants of `between()`, `index()`, or weighted selection. These would
  multiply the high-level API surface and weaken their ordinary meaning.
- A single array/span state representation shared by every engine. Uniform
  `state()`/`from_state()` operations are useful; identical physical storage is
  not required.
- Splitting `random.hpp` or `compat.hpp` solely to reduce file size. Each is
  currently a coherent unit, and physical size alone is not a design problem.
