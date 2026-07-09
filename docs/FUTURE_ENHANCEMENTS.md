# Future Enhancements Roadmap

This document tracks proposed improvements for portability, testing, and core platform capabilities.

## Planning Principles

- Keep the core library low-level, explicit, and allocation-aware.
- Preserve fast paths for Advent of Code workloads.
- Add features behind clear interfaces so Linux-first optimizations remain available.
- Prefer incremental, testable milestones over large one-shot rewrites.

## 1. Portability and Cross-Compiler Support

### Why this matters

The current memory arena implementation is optimized for Linux plus GCC or Clang. It relies on platform-specific and compiler-specific behavior in two places:

- Virtual memory calls (`mmap`, `munmap`, `madvise`, Linux flags).
- Lifetime start shim that currently uses GNU/Clang inline asm conventions.

If the project should run on Windows, broader Unix targets, or additional compilers, these areas need abstraction.

### Proposed milestones

1. Extend VM backends under core_vm:
- Reserve/commit/decommit/release (or map/unmap initially).
- Linux backend first (existing behavior).
- Windows backend next (`VirtualAlloc` and `VirtualFree`).

2. Make huge-page behavior optional and feature-gated:
- Use compile-time checks around Linux-specific flags.
- Keep huge-page hints best-effort and non-fatal.

Current direction:
- Huge-page hinting remains best-effort.
- Unsupported platforms should fail at compile time until an intentional backend is added.

3. Add compiler portability macros:
- Centralize always-inline and branch prediction attributes.
- Remove direct compiler attribute spelling from business logic.

4. Layer lifetime-start strategy:
- Use standard library lifetime-start API when available.
- Use compiler shim where required.
- Keep conservative fallback for trivially copyable types only.

### Success criteria

- Builds on Linux + GCC, Linux + Clang, and Windows + MSVC.
- Arena tests pass on all supported targets.
- No performance regression greater than 5% on benchmarked AoC hot paths.

## 2. Core Tests Expansion

### Goals

- Increase confidence in base modules used by puzzle solutions.
- Catch UB-adjacent memory and parser regressions early.
- Provide deterministic behavior under debug and release builds.

### Proposed test categories

1. Arena and memory tests:
- Alignment guarantees for varied `alignof(T)` values.
- Capacity boundary and out-of-memory paths.
- Reset and scoped scratch rollback correctness.
- Lifetime-start path for trivially copyable types.

2. String and parser primitive tests:
- Slice bounds and offset invariants.
- Numeric parsing edge cases and overflow handling.
- Lexer coordinator diagnostic accumulation semantics.

3. Integration tests for core pipelines:
- File to token stream to parsed records.
- Mixed valid and invalid input recovery behavior.

### Success criteria

- Core modules have deterministic test coverage for critical error paths.
- CI runs both debug and release test suites.
- Regression tests exist for each fixed production bug.

## 3. CLI Utilities for Argument Handling

### Goals

- Standardize command-line argument parsing for puzzle workers and tools.
- Avoid hand-rolled parsing spread across executables.

### Proposed capabilities

- Flag parsing (`--help`, `--day`, `--year`, `--input`).
- Positional argument support with typed conversion helpers.
- Validation and usage text generation.
- Error reporting with predictable exit codes.

### Suggested architecture

- Add a small `core_cli` module with:
- Argument view type (non-owning spans into argv).
- Option descriptors.
- Parse result object with diagnostics.

### Success criteria

- Main executable uses one shared CLI parser path.
- New tools can register options with minimal boilerplate.
- CLI behavior is covered by unit tests.

## 3.5 String Formatting API Strategy

### Goals

- Keep formatting allocation behavior explicit for arena-backed code.
- Provide a predictable correctness-first path and an optional performance path.
- Avoid sentinel return values by preserving `Result<E,T>` semantics.

### Planned API split

1. Exact formatting path (two-pass):
- `str8_pushf(...)` remains the exact-size formatter.
- Performs a sizing pass and then a write pass.
- Returns `Result<StringFormatError, Str8>`.
- Prioritizes correctness and precise allocation footprint.

2. Capped formatting path (single-pass):
- Add a capped formatter variant (for example `str8_pushf_cap(...)`).
- Caller provides maximum output capacity.
- Performs one formatting pass into `cap + 1` bytes.
- Returns explicit truncation/failure via `StringFormatError`.
- Prioritizes throughput in hot paths with known practical bounds.

### Success criteria

- Both formatting modes are available with clear caller intent.
- No formatting API relies on ambiguous empty-string sentinels for failure.
- Arena offsets are restored on formatting failure/truncation paths.

## 4. Huge File Streaming Support

### Goals

- Process very large inputs (for example, million-record class workloads) without loading entire files in memory.
- Keep parsing throughput high while controlling peak memory usage.

### Proposed capabilities

- Chunked buffered file reader abstraction.
- Delimiter-aware record framing across chunk boundaries.
- Backpressure-friendly processing pipeline.
- Optional memory-mapped fast path for suitable platforms.

### Suggested architecture

- Add `core_stream` module:
- `ByteStreamReader` for buffered reads.
- `RecordScanner` for newline or custom delimiter framing.
- Callback or iterator-based consume interface.

### Success criteria

- Able to process multi-gigabyte inputs with bounded memory.
- Record boundaries remain correct across chunk transitions.
- Throughput benchmark exists and is tracked over time.

## 5. HTTP Client Utilities

### Goals

- Enable optional programmatic input fetching and service-style integrations.
- Keep network concerns separate from puzzle logic and parsing core.

### Proposed capabilities

- Basic HTTP GET with timeout and retry policy.
- Header support and status-code handling.
- Response body streaming into file or parser pipeline.

### Suggested architecture

- Add a narrow `core_http` interface:
- Request options struct.
- Response metadata plus streaming body callback.
- Platform backend split (for example, libcurl adapter or native system APIs).

### Success criteria

- Input fetch command supports authenticated and unauthenticated endpoints.
- Failures return structured diagnostics (timeout, DNS, TLS, HTTP status).
- HTTP module remains optional and does not affect core parser builds.

## Implementation Order Recommendation

1. Core tests expansion (improves safety for all later work).
2. Portability abstraction for memory and compiler attributes.
3. CLI utilities (immediate developer ergonomics).
4. Huge file streaming support.
5. HTTP client utilities.

## Open Questions

- Should Windows support be first-class now or deferred until needed?
- Should HTTP support remain optional at build time by default?
- Should streaming APIs favor callback pipelines, iterators, or both?
