module;

#include <inttypes.h>

export module aoc_value;

import core_types;
import core_string;
import core_memory;
import core_result;
import core_console;

using namespace base;

export namespace aoc {

  enum struct ValueTag : u8 {
    Signed,
    Unsigned,
    String
  };

  struct Value {
    ValueTag tag;
    union {
      i64 i64;
      u64 u64;
      Str8 str;
    } as;
  };

  Str8 value_to_str8(Arena& arena, const Value val) {
    Result<str::StringFormatError, Str8> res = {};
    switch (val.tag) {

      case ValueTag::Signed:
        res = str::format_cap(arena, 100, "%" PRId64, val.as.i64);
      break;

      case ValueTag::Unsigned:
        res = str::format_cap(arena, 100, "%" PRIu64, val.as.u64);
      break;

      case ValueTag::String:
        return val.as.str;
      break;

      default:
        __builtin_unreachable();
    }
    if (res.is_ok()) {
      return res.value;
    } else {
      console::print_err(Str8("Failed to format result."));
      return Str8();
    }
  }
}