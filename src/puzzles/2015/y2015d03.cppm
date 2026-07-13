module;

#include "../../base/core_debug.h"

export module y2015d03;

import core_types;
import core_string;
import core_result;
import core_memory;
import aoc_value;

using namespace base;
using namespace str;
using namespace aoc;

export namespace y2015::d03 {

  struct Point {
    u32 x;
    u32 y;
  };

  void modPoint(Point& p, u8 c) {
    switch(c) {
      case '^':
        p.y -= 1;
      break;

      case 'v':
        p.y += 1;
      break;
      
      case '<':
        p.x -= 1;
      break;
      
      case '>':
        p.x += 1;
      break;

      default:
        __builtin_unreachable();
    }
  }

  u64 key_of(const Point p) {
    return ((u64)(u32)p.x << 32) | (u32)p.y;
  }

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