# AOC Documentation Guide

Welcome to the rendered documentation entry point for this project.

This page exists so the generated Doxygen site is not only an API dump. It provides a guided path through the authored design notes and the generated API reference.

## Recommended Reading Order

### 1. Architecture decisions

- [Architecture Decisions](ARCHITECTURE_DECISIONS.md)
- Durable project decisions for arena allocation, portability boundaries, and hash-container design.

### 2. HashMap algorithm guide

- [HashMap Algorithms](HASHMAP_ALGORITHMS.md)
- Tutorial-style walkthrough of Robin Hood hashing, cached hashes, growth by rehash, and the `HashMap<K, void>` set specialization.

### 2.5. Core modules guide

- [Core Modules Guide](CORE_MODULES_GUIDE.md)
- High-level map of the foundational modules and how memory, strings, results, and hash containers fit together.

### 3. Future roadmap

- [Future Enhancements](FUTURE_ENHANCEMENTS.md)
- Follow-up ideas for the hash container, platform work, tests, streaming, and other infrastructure.

### 4. Contributor guidance

- [Contributing](CONTRIBUTING.md)
- Project rules, coding expectations, and workflow guidance.

### 5. API reference

- Use the generated file, module, namespace, and type navigation in the Doxygen sidebar and index pages.
- The hash container implementation itself is documented in [core_hash_map.cppm API](core__hash__map_8cppm.html).
- The generated module page for the container is [core_hash_map module](module__core__hash__map.html).
- The generated namespace page is [base::hashmap namespace](namespacebase_1_1hashmap.html).
- The generated type pages are [HashMap<K,V>](structbase_1_1hashmap_1_1HashMap.html) and [HashMap<K,void>](structbase_1_1hashmap_1_1HashMap_3_01K_00_01void_01_4.html).

## Documentation Layers

This project intentionally uses two complementary documentation layers.

### Authored guides

These are Markdown documents under `docs/`.

They answer questions like:

- Why was this design chosen?
- What algorithmic tradeoffs matter here?
- What should future work improve?

### API reference

These are generated from Doxygen comments in source.

They answer questions like:

- What fields and functions exist?
- What arguments and return values do they use?
- What invariants or preconditions apply?

## Current Highlights

- Arena allocation is explicit and low-level.
- Portability and VM boundaries are separated into dedicated core modules.
- The hash container uses Robin Hood open addressing with cached hashes.
- Hash-set behavior is provided through `HashMap<K, void>`.

## Quick Links Into Rendered API

If you want to jump directly into generated reference material, these are the most useful entry points:

- [Arena API](core__memory_8cppm.html)
- [String API](core__string_8cppm.html)
- [Result API](core__result_8cppm.html)
- [Hash traits API](core__hash_8cppm.html)
- [HashMap file API](core__hash__map_8cppm.html)
- [HashMap module API](module__core__hash__map.html)
- [HashMap namespace API](namespacebase_1_1hashmap.html)

## Navigating HashMap Material

If your focus is the hash container specifically, the fastest path is:

1. [HashMap Algorithms](HASHMAP_ALGORITHMS.md)
2. [Architecture Decisions](ARCHITECTURE_DECISIONS.md)
3. [core_hash_map module API](module__core__hash__map.html)
4. [HashMap<K,V> type page](structbase_1_1hashmap_1_1HashMap.html)

That sequence gives you the algorithm, then the design constraints, then the exact implementation surface.