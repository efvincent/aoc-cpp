module;

#include "../../base/core_debug.h"

export module y2015d06;

import core_types;
import core_string;
import core_result;
import core_memory;
import aoc_value;
import core_lexer;
import core_lexer_coord;
import core_text_parse;
import core_console;

using namespace base;
using namespace str;
using namespace aoc;
using namespace lex;

export namespace y2015::d06 {

  enum struct Operation : u8 {
    TURN_ON,
    TURN_OFF,
    TOGGLE
  };

  struct Instruction {
    Operation op;
    u16 x1, y1;
    u16 x2, y2;
  };

  u32 get_next_num(Str8Cursor& c) {
    c.skip_whitespace();
    return c.consume_u32().value;
  }

  Instruction parse_line(Str8 line) {
    Instruction inst = {};
    Str8Cursor cursor{line, 0};
    
    // lex the command
    cursor.skip_whitespace();
    Str8 action = cursor.consume_ident();
  
    if (action.match(Str8("toggle"))) {
      inst.op = Operation::TOGGLE;
    } else {
      // if it wasn't toggle, it must be turn
      cursor.skip_whitespace();
      auto which_turn_op = cursor.consume_ident();
      if (which_turn_op.match(Str8("on"))) {
        inst.op = Operation::TURN_ON;
      } else {
        inst.op = Operation::TURN_OFF;
      }
    }

    // starting coordinate
    inst.x1 = get_next_num(cursor);

    // skip comma
    cursor.advance();

    inst.y1 = get_next_num(cursor);

    cursor.skip_whitespace();
    cursor.consume_ident();     // consumes "through"

    // ending coordinate    
    inst.x2 = get_next_num(cursor);
    
    // skip comma
    cursor.advance();
    inst.y2 = get_next_num(cursor);

    return inst;
  }
  
  #pragma clang diagnostic push
  #pragma clang diagnostic ignored "-Wunused-parameter"

  Str8 op_str(Operation op) {
    switch (op) {
      case Operation::TOGGLE:
        return Str8("toggle");
      break;
      case Operation::TURN_ON:
        return Str8("turn on");
      break;
      default:
        return Str8("turn off");
    }
  }

  aoc::Value part1(Arena& arena, Str8 raw) {
    BASE_ASSERT(raw.len > 0);
    u64 count = 0;
    arena.scoped_scratch([&raw, &count](Arena& scratch){
      Str8 cursor = raw;
      Str8 current = {};
      auto grid = scratch.alloc_array<bool>(1000 * 1000, true);

      while(iter_next(cursor, current, '\n')) {
        auto inst = parse_line(current);

        switch (inst.op) {
          case Operation::TURN_ON:
            for (u64 x = inst.x1; x <= inst.x2; x++) {
              auto idx_base = x * 1000;
              for (u64 y = inst.y1; y <= inst.y2; y++) {
                auto idx   = idx_base + y;
                count     += (!grid[idx]);
                grid[idx] = true;
              }
            }
          break;

          case Operation::TURN_OFF:
            for (u64 x = inst.x1; x <= inst.x2; x++) {
              auto idx_base = x * 1000;
              for (u64 y = inst.y1; y <= inst.y2; y++) {
                auto idx   = idx_base + y;
                count     -= (grid[idx]);
                grid[idx] = false;
              }
            }
          break;

          default:
            for (u64 x = inst.x1; x <= inst.x2; x++) {
              auto idx_base = x * 1000;
              for (u64 y = inst.y1; y <= inst.y2; y++) {
                auto idx   = idx_base + y;
                count     += grid[idx] ? -1 : 1;
                grid[idx]  = !grid[idx];
              }
            }
          break;
        } /* switch */
          
      }
    });

    return Value { ValueTag::Unsigned, { .u64 = count } };
  }

  aoc::Value part2(Arena& arena, Str8 raw) {
    BASE_ASSERT(raw.len > 0);
    u64 count = 0;
    arena.scoped_scratch([&raw, &count](Arena& scratch){
      Str8 cursor = raw;
      Str8 current = {};
      auto grid = scratch.alloc_array<u32>(1000 * 1000, true);

      while(iter_next(cursor, current, '\n')) {
        auto inst = parse_line(current);

        switch (inst.op) {
          case Operation::TURN_ON:
            for (u64 x = inst.x1; x <= inst.x2; x++) {
              auto idx_base = 1000 * x;
              for (u64 y = inst.y1; y <= inst.y2; y++) {
                auto idx   = idx_base + y;
                count     += 1;
                grid[idx] += 1;
              }
            }
          break;

          case Operation::TURN_OFF:
            for (u64 x = inst.x1; x <= inst.x2; x++) {
              auto idx_base = 1000 * x;
              for (u64 y = inst.y1; y <= inst.y2; y++) {
                auto idx   = idx_base + y;
                u32 dec    = (grid[idx]) > 0;
                grid[idx] -= dec;
                count     -= dec;
              }
            }
          break;

          default:
            for (u64 x = inst.x1; x <= inst.x2; x++) {
              auto idx_base = 1000 * x;
              for (u64 y = inst.y1; y <= inst.y2; y++) {
                auto idx   = idx_base + y;
                count     += 2;
                grid[idx] += 2;
              }
            }
        } /* switch */
      }
    });

    return Value { ValueTag::Unsigned, { .u64 = count } };
  }

  #pragma clang diagnostic pop
}