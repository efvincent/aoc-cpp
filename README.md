# aoc-cpp

`aoc-cpp` is a small Advent of Code workspace built around Clang, C++23 modules, and a deliberately low-level "orthodox C++" style.

## Requirements

- `clang++` with C++23 modules support
- `make`
- `doxygen` for API docs

## Layout

- `src/` contains the executable entry point and C++ module units.
- `docs/` contains project documentation and the checked-in Doxygen config.
- `build/` contains generated binaries, module caches, objects, and other build output.

## Build

The default configuration is `debug`.

```sh
make
```

Other supported configurations:

```sh
make CONFIG=release
make CONFIG=instrument
```

Clean generated output:

```sh
make clean
make clean_all
```

## Tooling

Regenerate the compilation database used by `clangd`:

```sh
make bear
```

This target rebuilds the project with Clang's native `-MJ` compilation database
fragments so C++23 module interface units are included correctly.

No external `bear` binary is required for this workflow.

Generate Doxygen output with the checked-in config:

```sh
make docs
```

That target reads `docs/Doxyfile` and writes generated output to `docs/html/` and `docs/latex/`.

## Notes

- The build currently uses `clang++` directly from the `makefile`.
- `compile_commands.json` is generated metadata and should be refreshed when source layout or compiler flags change.
- The repository includes VS Code settings for `clangd` and `uncrustify`.
