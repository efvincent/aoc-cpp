/**
 * @file main.cpp
 * @brief Temporary Advent of Code worker entry point and smoke-test harness.
 */

#include <inttypes.h>

import core_types;
import core_string;
import core_file;
import core_result;
import core_memory;
import core_console;

using namespace base;

/**
 * @brief Placeholder puzzle routine for early day-logic experimentation.
 * @param raw Input byte slice.
 * @return Accumulated result for the current placeholder implementation.
 */
i64 part1(Str8 raw) {
  i64 floor = 0;

  for (u64 i = 0; i < raw.len; ++i) {
    floor += (raw.str[i] == '(') ? 1 : -1;
  }
  return floor;
}

/**
 * @brief Program entry point used for manual local runs.
 * @return Process exit code.
 */
int main() {
  Arena arena = Arena();
  if (!arena.init(MB(1))) {
    print_err(Str8("failed to allocate arena memory."));
    return 1;
  }

  int exit_code = 0;

  auto file_result = file_read_all(arena, "./data/2015/day01.txt");
  if (file_result.is_ok()) {
    i64 answer = part1(file_result.value);

    // Exercise Result-based formatting path and handle failures explicitly.
    auto fmt_result = str8_push_cap(arena, 128, "part1 floor: %" PRId64, answer);

    if (!fmt_result.is_ok()) {
      print_err(Str8("failed to format output"));
      exit_code = 1;
    } else {
      printl(fmt_result.value);
    }
  } else {
    print_err(Str8("could not read input file"));
    exit_code = 1;
  }

  arena.free();
  return exit_code;
}