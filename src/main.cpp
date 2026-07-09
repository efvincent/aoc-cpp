#include <stdio.h>

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

using namespace base;

/**
 * @brief Placeholder puzzle routine for early day-logic experimentation.
 * @param raw Input byte slice.
 * @return Accumulated result for the current placeholder implementation.
 */
u64 part1(Str8 raw) {
  u64 i = 0;
  while (i < raw.len) {
    i += raw.str[i] == '(' ? 1 : -1;
  }
  return i;
}

/**
 * @brief Program entry point used for manual local runs.
 * @return Process exit code.
 */
int main() {
  Arena arena = Arena();
  if (!arena.init(MB(1))) {
    fprintf(stderr, "Failed to allocate arena memory.\n");
    return 1;
  }

  auto file_result = file_read_all(arena, "./data/2015/day02.txt");
  if (file_result.is_ok()) {
    Str8 msg = Str8("File contents:\n");
    printl(msg);
    printl(file_result.value);
  } else {

  }
  arena.free();
  return 0;
}