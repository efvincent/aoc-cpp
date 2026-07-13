module;

#include "../../base/core_debug.h"
#include "openssl/md5.h"
#include <string.h>


export module y2015d04;

import core_types;
import core_string;
import core_result;
import core_memory;
import aoc_value;

using namespace base;
using namespace str;
using namespace aoc;

export namespace y2015::d04 {

  void compute_md5(Str8 input, Str8 digest) {
    #if defined(__clang__) || defined(__GNUC__)
    #pragma GCC diagnostic push
    #pragma GCC diagnostic ignored "-Wdeprecated-declarations"
    #endif

    MD5((const unsigned char*)input.str, input.len, digest.str);

    #if defined(__clang__) || defined(__GNUC__)
    #pragma GCC diagnostic pop
    #endif
  }

  bool has_five_zero_prefix(Str8 candidate) {
    // bytes zero and one should be exactly 0x00
    if (candidate.str[0] != 0x00 || candidate.str[1] != 0x00) {
      return false;
    }
    // byte 2 upper nibble must be 0x0_
    // apply bitwise AND with 0xF0 (11110000) to mask the lower nibble
    if ((candidate.str[2] & 0xF0) != 0x00) {
      return false;
    }
    return true;
  }

  bool has_six_zero_prefix(Str8 candidate) {
    // bytes zero, one, and two should be exactly 0x00
    return !( candidate.str[0] != 0x00
              || candidate.str[1] != 0x00
              || candidate.str[2] != 0x00);
  }

  u64 count_digits_u64(u64 n) {
    u64 digits = 1;
    while (n >= 10) {
      n /= 10;
      digits++;
    }
    return digits;
  }

  Str8 append_counter(Str8 key_prefix, u64 counter) {
    Str8 out = key_prefix;
    u64 digits = count_digits_u64(counter);
    out.len = key_prefix.len + digits;

    // Fill only the suffix region [key_prefix.len, out.len) from right to left.
    u8 *suffix_start = out.str + key_prefix.len;
    u8 *cur = suffix_start + digits;
    do {
      *--cur = (u8)('0' + (counter % 10));
      counter /= 10;
    } while (counter > 0);

    return out;
  }

  u64 find_lowest_suffix(Arena& arena, Str8 key, bool (*predicate)(Str8)) {
    u64 count = 0;
    u64 start_pos = arena.offset;
    Str8 temp = Str8(arena, key.len + 10);
    memcpy(temp.str, key.str, key.len);
    temp.len = key.len;
    while(count < GB(1)) {
      u64 digest_start_pos = arena.offset;
      Str8 digest = Str8(arena, 16);

      // concatenate the secret key with the current counter
      Str8 candidate = append_counter(temp, count);
      compute_md5(candidate, digest);
      if (predicate(digest)) {
        break;
      }
      arena.offset = digest_start_pos;
      count++;
    }
    arena.offset = start_pos;
    return count;
  }

  aoc::Value part1(Arena& arena, Str8 raw) {
    BASE_ASSERT(raw.len > 0);
    u64 ans = find_lowest_suffix(arena, raw, has_five_zero_prefix);
    return Value { ValueTag::Unsigned, { .u64 = ans } };
  }

  aoc::Value part2(Arena& arena, Str8 raw) {
    BASE_ASSERT(raw.len > 0);
    u64 ans = find_lowest_suffix(arena, raw, has_six_zero_prefix);
    return Value { ValueTag::Unsigned, { .u64 = ans } };
  }
}