# Core Modules Guide

This document connects the core modules at a higher level than the generated API pages.

The goal is to answer a simple question:

- if you are trying to understand or extend the base layer, what should you read, and in what order?

## The Core Story

The base layer is built around a small set of modules that each own a distinct concern.

At a high level:

1. `core_types` defines the project's scalar vocabulary.
2. `core_vm` owns operating-system virtual memory boundaries.
3. `core_portability` owns compiler and standard-library portability shims.
4. `core_memory` builds the arena allocator on top of those lower layers.
5. `core_result` and `core_result_s` define explicit success/failure return types.
6. `core_string` provides `Str8`, formatting, and string-centric primitives.
7. `core_hash` defines hashing traits over supported key types.
8. `core_hash_map` builds the arena-backed hash containers.

## Module Dependency DAG

The direct module dependencies form a DAG. A lower layer is imported by the modules above it, and higher layers never feed back into the foundation.

\dot
digraph module_dependency_dag {
	rankdir=TB;
	node [shape=box, style=rounded];

	core_portability -> core_types;
	core_vm -> core_types;
	core_result -> core_types;
	core_option -> core_types;
	core_option -> core_portability;
	core_memory -> core_types;
	core_memory -> core_portability;
	core_memory -> core_vm;
	core_string -> core_types;
	core_string -> core_memory;
	core_string -> core_result;
	core_hash -> core_types;
	core_hash -> core_string;
	core_result_s -> core_types;
	core_result_s -> core_result;
	core_result_s -> core_string;
	core_text_parse -> core_types;
	core_text_parse -> core_result;
	core_text_parse -> core_string;
	core_text_parse -> core_portability;
	core_lexer -> core_types;
	core_lexer -> core_string;
	core_lexer -> core_result;
	core_lexer -> core_text_parse;
	core_lexer -> core_portability;
	core_lexer_coord -> core_types;
	core_lexer_coord -> core_string;
	core_lexer_coord -> core_lexer;
	core_lexer_coord -> core_text_parse;
	core_lexer_coord -> core_result;
	core_console -> core_types;
	core_console -> core_string;
	core_console -> core_memory;
	core_console -> core_result;
	core_file -> core_types;
	core_file -> core_string;
	core_file -> core_memory;
	core_file -> core_result;
	core_hash_map -> core_types;
	core_hash_map -> core_memory;
	core_hash_map -> core_hash;
	core_hash_map -> core_result;
	core_hash_map -> core_option;
	aoc_value -> core_types;
	aoc_value -> core_string;
	aoc_value -> core_memory;
	aoc_value -> core_result;
	aoc_value -> core_console;
}
\enddot

The puzzle modules sit above this graph. They import `aoc_value` and whichever core modules their solution needs, but they do not participate in the base-layer dependency chain.

## Suggested Reading Order

### 1. Scalar and utility base

- [core_types module API](module__core__types.html)
- [core_types file API](core__types_8cppm.html)

Read this first if you want the project's common integer aliases, arithmetic helpers, and shared low-level vocabulary.

### 2. Portability and OS boundaries

- [core_vm module API](module__core__vm.html)
- [core_vm file API](core__vm_8cppm.html)
- [core_portability module API](module__core__portability.html)
- [core_portability file API](core__portability_8cppm.html)

These modules separate two kinds of platform concerns:

- OS-level VM behavior belongs in `core_vm`.
- Compiler and language-model gaps belong in `core_portability`.

That split matters for understanding how the arena remains simple while still being correct.

### 3. Arena allocation

- [core_memory module API](module__core__memory.html)
- [core_memory file API](core__memory_8cppm.html)
- [Arena type page](structbase_1_1mem_1_1Arena.html)

This is the allocation foundation used by most higher-level transient data structures in the project.

If you are reading the hash table, you should understand the arena first.

### 4. Result types

- [core_result module API](module__core__result.html)
- [core_result file API](core__result_8cppm.html)
- [core_result_s module API](module__core__result__s.html)
- [core_result_s file API](core__result__s_8cppm.html)

These modules define the project's explicit error-reporting style.

The hash map uses this pattern directly through `HashMapE` and `HashMapR<T>`.

### 5. Strings and slices

- [core_string module API](module__core__string.html)
- [core_string file API](core__string_8cppm.html)
- [Str8 type page](structbase_1_1str_1_1Str8.html)

`Str8` is one of the most important data types in the project. It is both a common runtime string representation and one of the supported hash-key types.

### 6. Hash traits and containers

- [core_hash module API](module__core__hash.html)
- [core_hash file API](core__hash_8cppm.html)
- [core_hash_map module API](module__core__hash__map.html)
- [core_hash_map file API](core__hash__map_8cppm.html)
- [base::hashmap namespace](namespacebase_1_1hashmap.html)

This is the natural point where the earlier modules come together:

- the arena provides storage;
- result types provide failures;
- strings provide one important key family;
- hash traits bridge key types to the container;
- `core_hash_map` provides map and set behavior.

## How the Pieces Fit Together

### Arena and HashMap

The hash container does not own heap allocations directly.

Instead:

- `core_memory` supplies arena-backed arrays;
- `core_hash_map` stores keys, values, hashes, and control bytes inside those arrays;
- growth means allocate-and-rehash, not per-entry relocation with independent ownership.

### Result and HashMap

The container follows the same explicit-error style as the rest of the codebase.

- low-level helpers return `HashMapR<T>`;
- failures are categorized by `HashMapE`;
- callers are expected to branch on success rather than rely on exceptions.

### Str8 and HashMap

The `Str8` type is a natural fit for this design because it is a non-owning slice.

That means the table can store the slice cheaply, provided the backing bytes outlive the table entry.

## If You Are Focused on HashMap

The fastest path through the core layer is:

1. [HashMap Algorithms](HASHMAP_ALGORITHMS.md)
2. [Architecture Decisions](ARCHITECTURE_DECISIONS.md)
3. [core_memory file API](core__memory_8cppm.html)
4. [core_hash file API](core__hash_8cppm.html)
5. [core_hash_map file API](core__hash__map_8cppm.html)

That gives you:

- algorithmic intuition,
- design rationale,
- allocator model,
- trait model,
- and final implementation details.

## If You Are Extending the Base Layer

Use this rule of thumb:

- add OS-facing behavior to `core_vm`;
- add compiler or standard-library shims to `core_portability`;
- add allocation policy to `core_memory`;
- add shared data representation to `core_string` or similar domain modules;
- add container-specific logic to the container module itself.

Keeping those boundaries sharp is what makes the rest of the documentation readable.