module;

#include "../../base/core_debug.h"

export module y2015d03;

import core_types;
import core_string;
import core_result;
import core_memory;
import aoc_value;
import core_hash_map;
import core_hash;

using namespace base;
using namespace str;
using namespace aoc;

namespace y2015::d03 {
  struct Point {
    u32 x;
    u32 y;
  };
}

namespace base::hash {
  template <>
  struct HashTraits<y2015::d03::Point> {
    static constexpr u64 hash(const y2015::d03::Point& p) {
      u64 packed = (static_cast<u64>(p.x) << 32) | static_cast<u64>(p.y);
      return detail::mix64(packed);
    }

    static constexpr bool eq(const y2015::d03::Point& a,
                             const y2015::d03::Point& b) {
      return a.x == b.x && a.y == b.y;
    }
  };
}

export namespace y2015::d03 {

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

  aoc::Value part1(Arena& arena, Str8 raw) {
    i64 ans = 0;
    BASE_ASSERT(raw.len > 0);

    Point p = {};
    HashSet<Point> set = hashmap::HashSet<Point>();
  
    BASE_ASSERT(set.init(arena, 1000 * 1000).is_ok());

    return Value { ValueTag::Signed, { .i64 = ans } };
  }

  aoc::Value part2(Arena& arena, Str8 raw) {
    i64 ans = 0;

    BASE_ASSERT(raw.len > 0);

    return Value { ValueTag::Signed, { .i64 = ans } };
  }
}