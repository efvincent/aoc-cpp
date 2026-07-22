module;

#include "core_debug.h"

export module y2015d08;

import core_types;
import core_string;
import core_result;
import core_memory;
import core_console;
import aoc_value;

using namespace base;
using namespace console;
using namespace str;
using namespace aoc;

export namespace y2015::d08 {

  struct LineStats {
    u16 code;
    u16 chars;
  };

  LineStats count_chars(Str8& line) {
    LineStats stats = {2, 0};
    for(u64 i = 1; i < line.len - 1;) {
      // start at the second char, and loop to penultimate char, knowing
      // that each line starts and ends with a double quote
      if (line.str[i] == '\\') {
        // we're starting an escape sequence
        if (line.str[i + 1] == '\\' || line.str[i + 1] == '"') {
          // we have a simple escape clause
          i += 2;
          stats.chars += 1;
          stats.code += 2;
        } else {
          // we have a 3 character hex encoding
          i += 4;
          stats.chars += 1;
          stats.code += 4;
        }
      } else {
        // regular char
        i++;
        stats.chars += 1;
        stats.code += 1;
      }
    }
    return stats;
  }

  u16 reencoded_len(Str8 s) {
    BASE_ASSERT(s.str != nullptr && s.len > 0);
    u16 count = 2;
    for (u64 i = 0; i < s.len; i++) {
      count += s.str[i] == '"' || s.str[i] == '\\' ? 2 : 1;
    }
    return count;
  }

  aoc::Value part1(Str8 raw) {
    BASE_ASSERT(raw.len > 0);
    u64 total_chars = 0;
    u64 total_code = 0;
    Str8 cursor = raw;
    Str8 current = {};
    while (iter_next(cursor, current, '\n')) {
      auto current_stats = count_chars(current);
      total_chars += current_stats.chars;
      total_code += current_stats.code;
    }
    auto ans = total_code - total_chars;
    return Value { ValueTag::Unsigned, { .u64 = ans } };
  }

  aoc::Value part2(Str8 raw) {
    BASE_ASSERT(raw.len > 0);
    u64 total_old = 0;
    u64 total_new = 0;
    Str8 cursor = raw;
    Str8 current = {};
    while (iter_next(cursor, current, '\n')) {
      auto reencoded = reencoded_len(current);
      total_old += current.len;
      total_new += reencoded;
    }
    auto ans = total_new - total_old;
    return Value { ValueTag::Unsigned, { .u64 = ans } };
  }

}