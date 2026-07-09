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

using base::MB;
using base::Str8;
using base::i64;
using base::u64;

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

i64 part2(Str8 raw) {
  i64 floor = 0;

  for (u64 i = 0; i < raw.len; ++i) {
    floor += (raw.str[i] == '(') ? 1 : -1;
    if (floor == -1) {
      return i + 1;
    }
  }
  return floor;
}

/**
 * @brief Program entry point used for manual local runs.
 * @return Process exit code.
 */
int main() {
  base::mem::Arena arena = base::mem::Arena();
  if (!arena.init(MB(1))) {
    base::console::print_err(Str8("failed to allocate arena memory."));
    return 1;
  }

  int exit_code = 0;

  auto file_result = base::fs::read_all(arena, "./data/2015/day01.txt");
  if (file_result.is_ok()) {
    i64 answer = part2(file_result.value);

    // Exercise Result-based formatting path and handle failures explicitly.
    auto fmt_result = base::str::push_cap(arena, 128, "part2 enter basement on step: %" PRId64, answer);

    if (!fmt_result.is_ok()) {
      base::console::print_err(Str8("failed to format output"));
      exit_code = 1;
    } else {
      base::console::printl(fmt_result.value);
    }
  } else {
    base::console::print_err(Str8("could not read input file"));
    exit_code = 1;
  }

  arena.free();
  return exit_code;
}