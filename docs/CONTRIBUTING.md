# Architectural & Coding Style Guide: Project "Lithic" / Advent of Code

Scope note:
- This file is contributor-focused policy and implementation style guidance.
- For core-library architecture rationale, see [ARCHITECTURE_DECISIONS.md](ARCHITECTURE_DECISIONS.md).
- For planned capability expansion (portability, tests, CLI, streaming, HTTP), see [FUTURE_ENHANCEMENTS.md](FUTURE_ENHANCEMENTS.md).

This document serves as the absolute reference for the architectural foundations, constraints, and ergonomic style decisions established for our high-performance systems development framework. It preserves the structural reasoning required to maintain an airtight, predictable, and bare-metal optimized development environment.

---

## 1. Core Philosophy & Language Paradigm

### The "Orthodox C++" (C+) Paradigm
We treat C++ strictly as a **"Better C Compiler."** We purposefully reject complex C++ language layers that hide control flow, inject implicit metadata overhead, or introduce non-deterministic execution timelines. 
* All code must compile cleanly with `-fno-exceptions` and `-fno-rtti`.
* Third-party libraries are banned to eliminate modern ecosystem clashes and maximize educational self-reliance.
* The explicit target is to understand how code maps to physical hardware registers and cache lines.

### Banned Language Features (The Absolute Sanctions)
1. **Object-Oriented Programming (OOP):** Poly-morphic class hierarchies, virtual tables (`vtable`), implementation inheritance, and hidden object runtime dispatch are strictly prohibited.
2. **Standard Template Library (STL):** No standard heap-allocated structures (`std::vector`, `std::unordered_map`), standard strings, or iostreams (`std::ifstream`). No code may introduce unexpected or opaque heap allocations.
3. **RAII & Implicit Lifecycle Hooks:** Standard Resource Acquisition Is Initialization (RAII) via complex automated type object wrappers is rejected. We prioritize explicit management pipelines. Move semantics are discarded.
4. **Dynamic Allocations (`new` / `delete`):** Direct usage of the language runtime operators or standard memory tracking functions is banned outside of foundational OS layer requests.

### Allowed Language Features
1. **C++20/23 Modules:** Used explicitly (`export module`, `import`) to isolate subsystem scopes cleanly and maintain fast compilation graphs without standard textual header repetition layout issues.
2. **C++ Templates:** Allowed for zero-cost generic utilities where they reduce duplication without hiding control flow (for example, parser helpers, cursor consume helpers, or type-generic containers). Deep compile-time recursive metaprogramming tricks are rejected.
3. **Struct-Scoped Methods:** Used solely for syntactic convenience and low-overhead modular ergonomics without object-oriented state dispatch.

  
### Error Accumulation Contract (Lexer/Parser Split)

For language-server scenarios, the project follows an explicit two-layer contract:

1. **Lexer / Cursor Layer (`core_lexer`)**
- Owns byte navigation, token boundaries, trivia skipping, and recovery-oriented movement.
- A top-level lex step must either emit a token or advance at least one byte.
- Consumed-window failures must be reportable to the coordinator (no silent consumed failures).

2. **Strict Parse Layer (`core_text_parse`)**
- Owns pure slice validation and conversion (`Str8 -> ParseResult<T>` with `ParseDiagCode` on error).
- Must not own cursor movement, rewind policy, or diagnostic list accumulation.

3. **Coordinator Layer (`core_lexer_coord` and parser integration)**
- Owns diagnostic aggregation, severity classification, and multi-error recovery policy.
- Converts byte offsets to user-facing locations and decides how to continue after failures.
4. **Diagnostic Record Contract (`ParseDiagnostic`)**
- Diagnostics must be representable as POD records with byte spans into the original source.
- The base layer keeps diagnostics allocation-free (`code`, `severity`, `start_offset`, `end_offset`).
- Coordinator owns storage policy (arena/list/vector-equivalent in project style), de-duplication, and presentation formatting.

`ParseDiagnostic` is intentionally a low-level transport shape, not a presentation object.

Field semantics:
- `code`: stable machine-readable failure identifier (`ParseDiagCode`).
- `severity`: classifier (`Note`, `Warning`, `Error`) chosen by base defaults or coordinator policy.
- `start_offset`: inclusive byte offset in the original source buffer.
- `end_offset`: exclusive byte offset in the original source buffer.

Message policy:
- Base parser/lexer diagnostics are code-first and do not carry message payloads.
- Human-readable strings are generated later by coordinator/reporting layers from `ParseDiagCode`.

Coordinator wrapper policy:
- Coordinator wrappers in `core_lexer_coord` are responsible for diagnostic emission.
- Core lexer primitives in `core_lexer` stay side-effect-free with respect to diagnostic storage.
- Diagnostics are emitted only when a candidate window was consumed and strict validation failed.
- Prefix-miss failures (no consumed bytes) return errors without diagnostic emission so callers can choose alternate token paths.
- For float consumption, an exponent marker (`e`/`E`) without exponent digits is treated as a token boundary (left for subsequent tokenization), not as a consumed failure.

Intended usage split:
- Use `core_lexer` + `core_text_parse` directly for lean parsing paths (for example, AoC puzzle solutions) where caller only needs typed parse success/error and cursor movement.
- Use `core_lexer_coord` when building recovery-aware flows (for example, language-server pipelines) that must accumulate `ParseDiagnostic` records with source spans.
- Keep domain policy in the coordinator: buffering strategy, severity mapping, de-duplication, and reporting format.

Current `ParseDiagCode` baseline:
- `None`
- `EmptyInput`
- `NoDigits`
- `InvalidNumericChar`
- `IntOverflow`
- `InvalidExponent`
- `ExponentOverflow`
- `TrailingChars`
- `UnterminatedString`
- `UnterminatedBlockComment`

Coordinator rule: convert offsets to line/column only at reporting boundaries. Keep base parsing and lexing logic offset-native for hot-path simplicity.

Line index cache policy:
- For repeated offset-to-line/column translation, build a `LineIndex` once per source buffer and reuse it.
- `LineIndex` storage is caller-owned; the base layer only builds and queries the index.
- Prefer indexed location lookup for coordinator/reporting paths that translate many diagnostics or token spans.
- The scalar `compute_location(Str8, offset)` path remains useful for one-off lookups and simple callers.

Phase boundary summary:
- `core_text_parse`: validates slices and converts bytes to typed values or `ParseDiagCode`; it does not move cursors, allocate diagnostic storage, or choose recovery.
- `core_lexer`: owns cursor movement, candidate window discovery, trivia skipping, and other byte-navigation primitives; it does not accumulate diagnostics or own reporting policy.
- `core_lexer_coord`: binds cursor spans to diagnostics, pushes `ParseDiagnostic` records into caller-owned buffers, and applies recovery-oriented "emit on consumed failure" rules.
- Higher parser / application layers: interpret token meaning, build AST or semantic structures, choose severity policy, render human-facing messages, and decide editor-facing workflows.

Boundary enforcement rules:
- If logic needs only a `Str8` slice, it belongs in `core_text_parse`.
- If logic needs cursor offset mutation but no diagnostic storage, it belongs in `core_lexer`.
- If logic needs both cursor spans and diagnostic accumulation, it belongs in `core_lexer_coord` or a higher coordinator.
- If logic needs presentation strings, semantic naming, fix-it generation, or editor integration, it belongs above the base layer.
---

## 2. Structural Memory Management (The Arena Ecosystem)

### The `Arena` Architecture
Memory management is built around explicit, contiguous **Linear Memory Arenas**. There is no fine-grained deallocation. Memory is reclaimed instantaneously at zero runtime cost by resetting tracking offsets back to standard baselines.

#### Layout Variables
* Members inside an `Arena` instance must remain raw, plain-old-data components: `u8* buffer`, `u64 capacity`, and `u64 offset`.
* Memory addresses passed by the OS are processed linearly via customized alignment constraints.

### The Self-Allocating `init()` Pattern
* **Rule:** Heavy, side-effect-prone operating system resource requests (like calling standard `malloc` or invoking low-level virtual memory `mmap` syscalls) **must never happen inside a constructor**.
* **Reasoning:** Since we compile with `-fno-exceptions`, a constructor cannot safely communicate a physical system resource or allocation failure. Allocating memory inside a constructor forces either immediate, uncontrolled application crashes (`std::abort`) or unverified, silent structural corruption. 
* **Execution:** Constructors are used strictly for zero-cost, safe default structure zeroing. Active resource generation must be handled via a dedicated, named initialization method (e.g., `inline bool init(u64 capacity)`) that returns an explicit success or failure state.

### Transient Processing & Scoped Rollbacks
Data pipelines (especially input stream text tokenization) rely heavily on short-lived "scratchpad" allocations. We employ two distinct methods to manage these snapshot lifecycles safely:

#### The Factory Lambda Pattern (`scoped_scratch`)
For compact text scanning loops, internal parsing branches, or highly volatile structural processing, we leverage an inline template method execution fence: