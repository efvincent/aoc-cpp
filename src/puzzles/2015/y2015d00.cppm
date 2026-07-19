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
  
  aoc::Value part1(Arena& arena, Str8 raw) {
    BASE_ASSERT(raw.len > 0);
    arena.scoped_scratch([&](Arena& arena) {
      arena.alloc_array<u64>(20);
    });
    return Value { ValueTag::Unsigned, { .u64 = 0 } };
  }

  aoc::Value part2(Arena& arena, Str8 raw) {
    BASE_ASSERT(raw.len > 0);
    arena.scoped_scratch([&](Arena& arena) {
      arena.alloc_array<u64>(20);
    });

    return Value { ValueTag::Unsigned, { .u64 = 0 } };
  }    
  
}