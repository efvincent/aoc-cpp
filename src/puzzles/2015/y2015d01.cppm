export module y2015d01;

import core_types;
import core_string;
import core_result;
import core_memory;
import aoc_value;

using namespace base;
using namespace str;
using namespace aoc;
export namespace y2015::d01 {

  aoc::Value part1(Str8 raw) {
    i64 floor = 0;

    for (u64 i = 0; i < raw.len; ++i) {
      floor += (raw.str[i] == '(') ? 1 : -1;
    }
    return Value { ValueTag::Signed, { .i64 = floor } };
  }

  aoc::Value part2(Str8 raw) {
    i64 floor = 0;
    u64 steps = 0;

    for (u64 i = 0; i < raw.len; ++i) {
      steps++;
      floor += (raw.str[i] == '(') ? 1 : -1;
      if (floor < 0) {
        return Value { ValueTag::Unsigned, { .u64 = steps } };
      }
    }
    return Value { ValueTag::Unsigned, { .u64 = steps } };
  }
}