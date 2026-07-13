# Architecture Decisions

This document records durable architecture decisions for core library behavior.

## ADR-0001: Arena Uses Explicit Lifetime Start for Typed Views

- Status: Accepted
- Date: 2026-07-09
- Scope: Core memory arena allocation path

### Context

The arena allocates raw byte storage and then returns typed pointers (`T*`) to callers. In C++, raw storage is not automatically considered a live object of type `T` just because it has enough bytes and alignment. Optimizers can assume no `T` object exists there unless lifetime is started.

Without explicit lifetime start, typed reads and writes against arena storage can enter undefined-behavior territory, especially under aggressive optimization.

### Decision

Use an explicit lifetime-start helper for array allocations in the arena path (`base::portability::start_lifetime_as_array(...)`).

The helper:

1. Accepts raw storage pointer and element count.
2. Reinterprets storage as `T*`.
3. Returns early for zero-length arrays.
4. Uses a compiler barrier strategy on supported compilers so the optimizer treats the memory as a live `T` array region.
5. Restricts usage to trivially copyable `T` via static assertion.

### Why we need this

- The arena model separates storage allocation from object construction.
- We want fast typed access with low overhead for POD-style transient data.
- Explicit lifetime start makes this pattern valid under the language object model for the supported type subset.
- It reduces risk of optimization-time miscompiles and strict-aliasing surprises.

### Consequences

Positive:

- Keeps arena hot path fast and simple.
- Preserves low-level style without requiring full constructor loops for trivial types.
- Documents an otherwise subtle language-rule requirement.

Trade-offs:

- Current helper uses compiler-specific behavior for some implementations.
- Non-trivial object types are intentionally excluded from this fast path.

### Alternatives considered

1. Require full placement-new construction for all types.
- Pros: broad correctness for all object kinds.
- Cons: more overhead and more API complexity for transient workloads.

2. Use raw reinterpret cast only, with no explicit lifetime step.
- Pros: minimal code.
- Cons: undefined behavior risk under optimization.

3. Depend directly on standard library lifetime-start APIs from `<memory>`.
- Pros: standard interface.
- Cons: toolchain availability and portability variance may still require fallback shims.

### Follow-up actions

- Add portability layers so lifetime strategy can vary by compiler/toolchain.
- Provide a separate explicit construction path for non-trivial types if needed.
- Add tests that exercise arena behavior in optimized builds.

## ADR-0002: Arena Initialization Is Explicit, Not Constructor-Driven

- Status: Accepted
- Date: 2026-07-09
- Scope: Resource acquisition for mapped memory

### Decision

Arena memory mapping happens in `init(...)` and release in `free()`, rather than acquiring resources inside constructors/destructors.

### Rationale

- Keeps failures explicit via return values.
- Fits project settings that disable exceptions and RTTI.
- Avoids hidden control flow in critical low-level paths.

## ADR-0003: Compiler and Standard-Library Gaps Live in core_portability

- Status: Accepted
- Date: 2026-07-09
- Scope: Compiler intrinsics, standard-library replacements, and portability shims

### Context

Some low-level facilities used by the project are most naturally expressed through standard-library APIs such as lifetime-start helpers or utility functions such as `unreachable()`. In practice, toolchain support, header cost, or project style constraints can make direct use of those APIs undesirable.

Ad-hoc replacements inside feature modules make the codebase harder to reason about:

- Arena code starts carrying compiler-specific implementation details.
- Warning suppression and intrinsics become duplicated.
- Portability work spreads across unrelated modules.

### Decision

Compiler-specific intrinsics and project-owned replacements for selected standard-library facilities must live in `core_portability`.

Feature modules such as memory, parsing, or CLI code should:

1. import `core_portability` when they need these facilities;
2. call project-owned helpers from that module;
3. avoid defining local replacements for standard-library features.

### Consequences

Positive:

- Keeps business logic separate from compiler glue.
- Centralizes warning suppression and intrinsic selection.
- Makes future compiler and platform support changes local and reviewable.

Trade-offs:

- Adds one more foundational module dependency.
- Requires discipline so new shims are added centrally rather than opportunistically.

### Current examples

- `start_lifetime_as_array(...)`
- `unreachable()`
- compiler-backed trait wrappers such as trivially-copyable checks

## ADR-0004: OS Virtual Memory Boundaries Live in core_vm

- Status: Accepted
- Date: 2026-07-09
- Scope: Operating-system virtual memory APIs and arena backing storage

### Context

The arena allocator needs a contiguous writable region with explicit lifetime under project control. On Linux, that region is naturally provided by `mmap`, released by `munmap`, and optionally tuned with `madvise` huge-page hints. Those APIs are operating-system interfaces, not business logic.

When those calls live directly inside `core_memory`, several problems appear:

- arena code becomes tied to one OS header and one syscall vocabulary;
- future Windows or alternative Unix support requires reopening allocator logic rather than swapping a backend;
- policy questions such as huge-page hinting and unsupported-platform behavior become mixed into the arena implementation itself.

### Decision

All operating-system virtual memory boundaries must live in `core_vm`.

`core_memory` may depend on exported VM operations such as:

1. allocate a contiguous writable region;
2. free a previously allocated region;
3. apply optional VM tuning hints.

Feature modules must not include OS VM headers directly when a `core_vm` operation can express the intent.

### Selected policy

The current VM policy is intentionally strict and simple:

1. Huge-page hinting is best-effort.
	- Failure to apply a hint is not an allocation failure.
	- Lack of platform support for the hint is treated as a no-op.

2. Unsupported platforms hard-error at compile time.
	- The project should fail loudly rather than silently degrade to a different allocation model.
	- Any future non-Linux backend must be added intentionally in `core_vm`.

3. Arena logic remains policy-light.
	- It asks for memory and releases memory.
	- It does not reason about syscall flags or platform-specific fallback behavior.

### Consequences

Positive:

- OS-specific code is localized to one module.
- Arena code is easier to read and review.
- Adding a new platform means extending one backend surface instead of rewriting allocator logic.

Trade-offs:

- `core_vm` becomes a foundational dependency for arena-backed allocation.
- Hard compile errors on unsupported platforms trade convenience for explicitness.
- Platform-specific behavior still exists, but it is intentionally concentrated rather than removed.

### Boundary guidance

- `core_vm` owns syscalls, OS headers, and platform compile-time switches.
- `core_memory` owns arena policy, offsets, alignment, and typed allocation behavior.
- `core_portability` owns compiler and standard-library gaps, not OS allocation APIs.

This split is deliberate: compiler portability and operating-system portability are related concerns, but they are not the same layer and should not be collapsed into one module.

## ADR-0005: HashMap Uses Arena-Backed Robin Hood Open Addressing

- Status: Accepted
- Date: 2026-07-13
- Scope: Base hash container design for map and set workloads

### Context

The project needs a low-level hash container that matches the existing arena allocation model and stays efficient for Advent of Code style workloads.

The main constraints are:

- no per-element heap allocation;
- explicit failure reporting via `Result<E,T>` rather than exceptions;
- compatibility with both key-value maps and key-only sets;
- predictable probe behavior under moderately high load factors;
- acceptable performance for both integer keys and byte-string slice keys such as `Str8`.

Separate chaining would introduce extra pointer chasing and allocator pressure. A traditional open-addressing table without probe balancing risks long clusters and unstable lookup costs as load increases.

### Decision

The hash container design uses arena-backed open addressing with Robin Hood insertion.

The current shape is:

1. capacity is always a power of two;
2. indexing uses a bitmask rather than modulo;
3. keys, values, cached hashes, and occupancy control bytes are stored in separate arrays;
4. growth allocates new arena storage and reinserts live entries into the larger table;
5. `HashMap<K, void>` is the set specialization and does not allocate a values array;
6. operations report failures through a dedicated `HashMapE` error enum and `HashMapR<T>` aliases.

### Why we need this

- Robin Hood probing reduces variance in probe lengths and improves worst-case behavior at a given load factor.
- Separate key, value, hash, and control arrays preserve a simple data-oriented layout and match the existing arena model.
- Cached per-slot hashes avoid repeated re-hashing of resident keys during probe walks, which matters for `Str8` keys.
- Arena growth through allocate-and-rehash keeps ownership simple and avoids per-entry lifetime bookkeeping.
- A `void` specialization provides true set semantics without paying for unused value storage.

### Consequences

Positive:

- No general-purpose heap allocator dependency in the hot path.
- Predictable insertion and lookup behavior for the supported workload shape.
- String-key probe chains avoid unnecessary repeated hash computation.
- Map and set behavior share one conceptual implementation strategy.

Trade-offs:

- Growth is monotonic within an arena and does not reclaim prior table storage.
- The current design does not yet support erase.
- The current control-byte scheme is intentionally simple and does not yet use SIMD-friendly fingerprints.
- The fast path assumes trivially copyable key and value types are the intended primary workload.

### Alternatives considered

1. Separate chaining with linked nodes.
- Pros: simpler deletion semantics.
- Cons: worse locality, more pointer chasing, and allocator pressure.

2. Linear probing without Robin Hood balancing.
- Pros: simpler insert logic.
- Cons: longer-tail probe behavior under load.

3. Recompute hashes on every probe step.
- Pros: less storage overhead.
- Cons: avoidable cost for non-trivial key types, especially `Str8`.

### Follow-up actions

- Add focused container tests for insert, overwrite, contains, and rehash behavior.
- Decide whether erase belongs in this container or in a separate higher-level variant.
- Evaluate SIMD-friendly metadata or fingerprint control bytes if probe performance becomes a bottleneck.
- Document supported key/value type expectations more explicitly in API docs.
