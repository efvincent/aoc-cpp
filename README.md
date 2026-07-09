# aoc-cpp

Low-level Advent of Code workspace using C++23 modules and explicit systems-style design.

This README is the documentation index and recommended reading path.

## Documentation Index

### Start here (core library users)

1. [docs/ARCHITECTURE_DECISIONS.md](docs/ARCHITECTURE_DECISIONS.md)
	- Durable architecture decisions for memory model and low-level behavior.
	- Includes why arena allocation uses explicit lifetime start for typed views.

2. [docs/FUTURE_ENHANCEMENTS.md](docs/FUTURE_ENHANCEMENTS.md)
	- Forward-looking roadmap for portability, core tests, CLI utilities,
	  huge file streaming, and HTTP client support.

3. Doxygen API reference
	- Generated output: [docs/html/index.html](docs/html/index.html)
	- Configuration: [docs/Doxyfile](docs/Doxyfile)

### Contributor and project policy

1. [docs/CONTRIBUTING.md](docs/CONTRIBUTING.md)
	- Coding style, architectural constraints, and contribution rules.

## Build and Tooling

### Requirements

- clang++ with C++23 modules support
- make
- doxygen (for API docs)

### Common commands

```sh
make
make release
make lto
make instrument
make bear
make docs
make clean
make clean_all
```

## Repository Layout

- src/: executable entry point and C++ module units
- docs/: authored docs, Doxygen config, generated API docs
- data/: Advent of Code inputs
- build/: generated binaries, module caches, objects, and intermediate output

## Notes

- Default build configuration is debug.
- Compilation database is generated metadata and should be refreshed when compiler flags or source layout change.
