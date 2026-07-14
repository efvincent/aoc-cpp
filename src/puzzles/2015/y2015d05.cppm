module;

#include "../../base/core_debug.h"

export module y2015d05;

import core_types;
import core_string;
import core_result;
import core_memory;
import aoc_value;

using namespace base;
using namespace str;
using namespace aoc;

export namespace y2015::d05 {
  
  #pragma clang diagnostic push
  #pragma clang diagnostic ignored "-Wunused-parameter"

  constexpr u8 isVowel(u8 c) {
    return (c == 'a' || c == 'e' || c == 'i' || c == 'o' || c == 'u' );
  }

  constexpr bool at_least_3_vowels(const Str8& s) {
    u8 count = 0;
    for (u32 i = 0; i < s.len; i++) {
      if (isVowel(s.str[i])) {
        if (++count >= 3) return true;
      }
    }
    return false;
  }

  constexpr bool one_letter_twice(Str8 s) {
    u8 last = 0;
    for (u32 i = 0; i < s.len; i++) {
      if (s.str[i] == last) {
        return true;
      }
      last = s.str[i];
    }    
    return false;
  }

  constexpr bool no_bad_pairs(Str8 s) {
    for (u32 i = 0; i < s.len - 1; i++) {
      u8 c1 = s.str[i];
      u8 c2 = s.str[i + 1];
      //  ab, cd, pq, or xy,
      if ( (c1 == 'a' && c2 == 'b')
          || (c1 == 'c' && c2 == 'd')
          || (c1 == 'p' && c2 == 'q')
          || (c1 == 'x' && c2 == 'y')) {
        return false;
      }
    }
    return true;
  }

  aoc::Value part1(Arena& arena, Str8 raw) {
    BASE_ASSERT(raw.len > 0);
    Str8 cursor = raw;
    Str8 line = {};
    u64 count = 0;
    while (iter_next(cursor, line, '\n')) {
      if (
        at_least_3_vowels(line) &&
        one_letter_twice(line) &&
        no_bad_pairs(line)
      ) {
        count++;
      }
    }
    return Value { ValueTag::Unsigned, { .u64 = count } };
  }

  bool one_between(Str8 s) {
    // contains one letter that repeats with exactly one letter between
    for(u32 i = 0; i < s.len - 2; i++) {
      if (s.str[i] == s.str[i+2]) {
        return true;
      }
    }
    return false;
  }

  bool pair_repeats(Str8 s, u8 cfg[3]) {
    for (u32 i = cfg[0]; i < s.len - 1; i++) {
      if (s.str[i] == cfg[1] && s.str[i+1] == cfg[2]) {
        return true;
      }
    }
    return false;
  }

  bool two_pair(Str8 s) {
    u8 cfg[3];
    for(u32 i = 0; i < s.len; i++) {
      cfg[0] = i+2;
      cfg[1] = s.str[i];
      cfg[2] = s.str[i+1];
      if(pair_repeats(s, cfg)) {
        return true;
      }
    }
    return false;
  }

  aoc::Value part2(Arena& arena, Str8 raw) {
    BASE_ASSERT(raw.len > 0);
    Str8 cursor = raw;
    Str8 line = {};
    u64 count = 0;
    while(iter_next(cursor, line, '\n')) {
      if (one_between(line) && two_pair(line)) {
        count++;
      }
    }
    return Value { ValueTag::Unsigned, { .u64 = count } };
  }

  #pragma clang diagnostic pop
}