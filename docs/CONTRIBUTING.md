# Architectural & Coding Style Guide: Project "Lithic" / Advent of Code

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