# Benchmarks

The canonical benchmark sources are:

- `engine_comparison.cpp`: compares the raw engines available to the library.
- `random_bounded_integer.cpp`: compares bounded integer generation.
- `random_bounded_float.cpp`: compares bounded floating-point generation.

The public API comparisons currently include `QuarkBurst64`, `RomuDuoJr`, and
`Konadare192`. Add or remove engine headers and their `BENCHMARK_TEMPLATE`
registrations in the two `random_bounded_*.cpp` sources to change that set.

Run the generator from the repository root with Windows PowerShell:

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass `
    -File .\benchmarks\generate-quickbench.ps1
```

This creates three standalone files under `benchmarks/generated/`. They are
intended to be pasted separately into Quick Bench. Do not edit generated files
directly.

The C bounded-integer baseline uses `% bound` and can be biased. It is included
as a familiar performance baseline, not as a quality-equivalent implementation.
