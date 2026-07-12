/**
 * @file main.cpp
 * @brief Temporary Advent of Code worker entry point and smoke-test harness.
 */


import core_types;
import core_string;
import core_file;
import core_result;
import core_memory;
import core_console;
import core_text_parse;
import puzzles;

using namespace base;
using namespace str;
using namespace mem;
using namespace puzzles;

/**
 * @brief Program entry point used for manual local runs.
 * @return Process exit code.
 */
int main(int argc, char** argv) {
  Arena arena = Arena();
  if (!arena.init(MB(1))) {
    console::print_err(Str8("failed to allocate arena memory."));
    return 1;
  }

  int exit_code = 0;

  auto specR = get_puz_spec(argc, argv);
  if (!specR.is_ok()) {
    display_spec_error(specR.error);
    return 1;
  }

  puzzles::runPuzzle(arena, specR.value);
    
    // Exercise Result-based formatting path and handle failures explicitly.
    // auto fmt_result = str::format_cap(arena, 128, "part2 enter basement on step: %" PRId64, answer);

  arena.free();
  return exit_code;
}