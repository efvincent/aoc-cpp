module;

#include "core_debug.h"

export module y2015d02;

import core_types;
import core_string;
import core_result;
import core_memory;
import core_lexer;
import aoc_value;

using namespace base;
using namespace str;
using namespace aoc;
using namespace base::lex;

struct BoxDims {
  u64 l;
  u64 w;
  u64 h;
};

export namespace y2015::d02 {

  bool parse_dims(Str8 line, BoxDims& out) {
    Str8Cursor cursor{line, 0};

    auto l = cursor.consume_u64();
    if (!l.is_ok()) return false;

    if (cursor.peek() != 'x') return false;
    cursor.advance();

    auto w = cursor.consume_u64();
    if (!w.is_ok()) return false;

    if (cursor.peek() != 'x') return false;
    cursor.advance();

    auto h = cursor.consume_u64();
    if (!h.is_ok()) return false;

    if (!cursor.is_eof()) return false;

    out = BoxDims{
      .l = l.value,
      .w = w.value,
      .h = h.value,
    };
    return true;
  }

  aoc::Value part1(Str8 raw) {
    BASE_ASSERT(raw.len > 0);
    u64 total_paper = 0;
    Str8 remaining  = raw;
    Str8 line       = {};
    BoxDims box     = {};

    while(str::iter_next(remaining, line, '\n')) {
      if (parse_dims(line, box)) {
        auto paper_needed = 
            (box.l * box.w * 2) 
          + (box.w * box.h * 2) 
          + (box.h * box.l * 2) 
          + Min(box.l * box.w, Min(box.w * box.h, box.h * box.l));
        total_paper += paper_needed;
      }
    }
    return Value { ValueTag::Unsigned, { .u64 = total_paper } };
  }
  

  aoc::Value part2(Str8 raw) {
    BASE_ASSERT(raw.len > 0);
    u64 total_paper = 0;
    Str8 remaining = raw;
    Str8 line = {};
    BoxDims box = {};

    while(str::iter_next(remaining, line, '\n')) {
      if (parse_dims(line, box)) {
        auto s1 = 2 * box.l + 2 * box.w;
        auto s2 = 2 * box.h + 2 * box.w;
        auto s3 = 2 * box.l + 2 * box.h;
        auto ribbon = Min(s1, Min(s2, s3));
        auto bow = box.l * box.w * box.h;
        auto paper_needed = ribbon + bow;
        total_paper += paper_needed;
      }
    }
    return Value { ValueTag::Unsigned, { .u64 = total_paper } };
  }
}