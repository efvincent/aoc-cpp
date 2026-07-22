# aoc-cpp

Low-level Advent of Code workspace using C++26 modules and explicit systems-style design.

This README is the documentation index and recommended reading path.

## Documentation Index

### Start here (core library users)

1. [docs/ARCHITECTURE_DECISIONS.md](docs/ARCHITECTURE_DECISIONS.md)
	- Durable architecture decisions for memory model and low-level behavior.
	- Includes why arena allocation uses explicit lifetime start for typed views.

2. [docs/FUTURE_ENHANCEMENTS.md](docs/FUTURE_ENHANCEMENTS.md)
	- Forward-looking roadmap for portability, core tests, CLI utilities,
	  huge file streaming, and HTTP client support.

3. [docs/HASHMAP_ALGORITHMS.md](docs/HASHMAP_ALGORITHMS.md)
	- Standalone walkthrough of the algorithms used in the arena-backed hash map.
	- Covers Robin Hood probing, cached hashes, growth via rehash, and set specialization.

4. [docs/CORE_MODULES_GUIDE.md](docs/CORE_MODULES_GUIDE.md)
	- Guided overview of the foundational modules and how they connect.
	- Includes a direct-import DAG for the base layer.
	- Useful when moving from architecture notes into concrete APIs.

5. Doxygen API reference
	- Rendered landing page: [docs/html/index.html](docs/html/index.html)
	- Guided authored-doc entry page: [docs/DOXYGEN_MAINPAGE.md](docs/DOXYGEN_MAINPAGE.md)
	- Generated output: [docs/html/index.html](docs/html/index.html)
	- Configuration: [docs/Doxyfile](docs/Doxyfile)

### Rendered Docs Structure

The rendered Doxygen site now uses a dedicated Markdown landing page so authored design notes and API reference live in one navigation flow.

- Landing page source: `docs/DOXYGEN_MAINPAGE.md`
- Rendered main page: `docs/html/index.html`
- Additional authored guides under `docs/` are included automatically by Doxygen.

### Contributor and project policy

1. [docs/CONTRIBUTING.md](docs/CONTRIBUTING.md)
	- Coding style, architectural constraints, and contribution rules.

## Build and Tooling

### Requirements

- clang++ with C++26 modules support
- make
- doxygen (for API docs)

### Clone and submodule setup

The repository consumes core modules through a pinned git submodule.

```sh
git clone https://github.com/efvincent/aoc-cpp.git
cd aoc-cpp
git submodule update --init --recursive
```

To verify the pin contract locally:

```sh
make verify-submodule-pin
```

### Common commands

```sh
make
make release
make lto
make instrument
make bear
make docs
make docs_pdf
make verify-submodule-pin
make clean
make clean_all

# benchmark entry points (nanosecond wall-clock)
make bench-debug N=2000 ARGS='2015 2 1'
make bench-release N=2000 ARGS='2015 2 1'
make bench-lto N=2000 ARGS='2015 2 1'
make bench-instrument N=200 ARGS='2015 2 1'
make MODE=release bench N=2000 ARGS='2015 2'
```

### Benchmarking

PDF docs note:

- `make docs_pdf` requires a TeX installation with `listofitems` and `ulem` available.
- On TeX Live user setups, one working option is:
	- `tlmgr --usermode install listofitems ulem`

The project includes a root benchmark script and matching make targets:

- Script: `./bench_nanos.sh`
- Log file (default): `bench_results.csv`
- Binary selector: build mode (`debug`, `release`, `lto`, `instrument`) or explicit binary path.

#### Script usage

```sh
./bench_nanos.sh N BUILD year day [part]
```

- `N`: number of benchmark iterations (must be a positive integer).
- `BUILD`: one of `debug`, `release`, `lto`, `instrument`, or a binary path.
- Remaining arguments are passed directly to the AoC program.
- `part` is optional; if omitted, the log records an empty field for `part`.

Examples:

```sh
./bench_nanos.sh 5000 debug 2015 2 1
./bench_nanos.sh 5000 release 2015 2
./bench_nanos.sh 1000 build/aoc_worker_release 2015 2 2
```

#### Make target usage

Benchmark targets automatically build the requested mode before running:

```sh
make bench-debug N=5000 ARGS='2015 2 1'
make bench-release N=5000 ARGS='2015 2 1'
make bench-lto N=5000 ARGS='2015 2 1'
make bench-instrument N=200 ARGS='2015 2 1'
```

Generic target (uses `MODE`):

```sh
make MODE=release bench N=5000 ARGS='2015 2'
```

Defaults if omitted:

- `N=1000`
- `ARGS='2015 2'`

#### Output and log format

Each run prints:

- `total_ns`: total wall-clock nanoseconds across `N` runs.
- `avg_ns`: integer average nanoseconds per run.

CSV rows are appended to `bench_results.csv` with header:

```text
timestamp,iterations,build,year,day,part,total_ns,avg_ns
```

Example row with omitted part:

```text
2026-07-12T20:28:04Z,2,debug,2015,2,,11453630,5726815
```

## Repository Layout

- src/: executable entry point and AoC-specific module units
- third_party/cpp-core/: pinned core module source submodule
- docs/: authored docs, Doxygen config, generated API docs
- data/: Advent of Code inputs
- build/: generated binaries, module caches, objects, and intermediate output

## Notes

- Default build configuration is debug.
- Compilation database is generated metadata and should be refreshed when compiler flags or source layout change.
