module;

#include "../../base/core_debug.h"

export module y2015d00;

import core_types;
import core_string;
import core_result;
import core_memory;
import aoc_value;

using namespace base;
using namespace str;
using namespace aoc;

export namespace y2015::d00 {

  aoc::Value part1(Str8 raw) {
    i64 ans = 0;

    BASE_ASSERT(raw.len > 0);

    return Value { ValueTag::Signed, { .i64 = ans } };
  }

  aoc::Value part2(Str8 raw) {
    i64 ans = 0;

    BASE_ASSERT(raw.len > 0);

    return Value { ValueTag::Signed, { .i64 = ans } };
  }
}