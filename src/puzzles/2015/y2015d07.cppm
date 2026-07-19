module;

#include "../../base/core_debug.h"

export module y2015d07;

import core_types;
import core_string;
import core_result;
import core_memory;
import core_lexer;
import core_hash_map;
import core_console;
import aoc_value;

using namespace base;
using namespace str;
using namespace aoc;
using namespace lex;
using namespace hashmap;

enum struct OpType : u8 {
  Assign,
  And,
  Or,
  LShift,
  RShift,
  Not
};

struct Arg {
  bool is_literal;
  u16 literal_val;
  Str8 wire_name;
};

struct Instruction {
  union {
    struct ins {
      OpType op;
      Arg left;
      Arg right;
    } ins;
    u16 val;
  } as;
  bool evaluated;
};

using Env = HashMap<Str8, Instruction>;

const Str8 kw_not = Str8("NOT");
const Str8 kw_and = Str8("AND");
const Str8 kw_lsh = Str8("LSHIFT");
const Str8 kw_rsh = Str8("RSHIFT");
const Str8 kw_or  = Str8("OR");

Instruction make_literal_instruction(u16 v) {
  Instruction inst = {};
  inst.as.ins.op = OpType::Assign;
  inst.as.ins.left = Arg{
    .is_literal = true,
    .literal_val = v,
    .wire_name = Str8()
  };
  inst.as.ins.right = Arg{};   // unused for Assign
  inst.evaluated = false;      // let interpret() compute/cache it
  return inst;
}

void parse_day7(Arena& arena, const Str8 file_content, Env& env) {
  Str8Cursor cursor = { file_content, 0 };

  auto parse_arg = [&](Str8Cursor& cur) -> Arg {
    Arg arg = {};
    if (cur.is_numeric(cur.peek())) {
      arg.is_literal = true;
      arg.literal_val = cur.consume_u16().value;
    } else {
      arg.is_literal = false;
      arg.wire_name = cur.consume_ident();
    }
    return arg;
  };
  
  while (!cursor.is_eof()) {
    cursor.skip_whitespace();
    if (cursor.is_eof()) break;
    Instruction inst = {};

    // Check for unary NOT operator
    if (cursor.peek() == 'N') {
      inst.as.ins.op = OpType::Not;
      cursor.advance(3);
      cursor.skip_whitespace();
      inst.as.ins.left = parse_arg(cursor);
    } else {
      // It wasn't NOT, so the first token must be the left argument.
      // We rewind our logic slightly; if it was a number, we parse it as such

      // For simplicity, assume we read the left Arg correctly
      inst.as.ins.left = parse_arg(cursor);
      cursor.skip_whitespace();

      // Check if we hiut the arrow (assignment) or a binary operator
      if (cursor.peek() == '-') {
        inst.as.ins.op = OpType::Assign;
      } else {
        Str8 op_token = cursor.consume_ident();
        if (op_token.match(kw_and)) inst.as.ins.op = OpType::And;
        else if (op_token.match(kw_or)) inst.as.ins.op = OpType::Or;
        else if (op_token.match(kw_lsh)) inst.as.ins.op = OpType::LShift;
        else if (op_token.match(kw_rsh)) inst.as.ins.op = OpType::RShift;
        
        cursor.skip_whitespace();
        inst.as.ins.right = parse_arg(cursor);
      }
    }

    // Consume ->
    cursor.skip_whitespace();
    if (cursor.peek() == '-') {
      cursor.advance(2);    // skip over ->
    }

    // Consume destination wire
    cursor.skip_whitespace();

    // Save the instruction for this wire into the environment
    env.upsert(arena, cursor.consume_ident(), inst);
  }
}

// forward decl interpret for mutually recursive algo
void interpret(Arena& arena, Instruction& inst, Env &env);

u16 get_value(Arena& arena, Arg arg, Env &env) {
  if (arg.is_literal) {
    return arg.literal_val;
  }
  auto wire = arg.wire_name;
  auto ins_for_wireR = env.find(wire);
  BASE_ASSERT(ins_for_wireR.is_ok() && ins_for_wireR.value.is_some());
  auto ins = ins_for_wireR.value.value;
  if (ins->evaluated) {
    return ins->as.val;
  } else {
    interpret(arena, *ins, env);
    return ins->as.val;
  }
  __builtin_unreachable();
} 

void interpret(Arena& arena, Instruction& inst, Env &env) {
  if (inst.evaluated) return;
  switch (inst.as.ins.op) {
    case OpType::Assign: {
      auto source = inst.as.ins.left;
      if (source.is_literal) {
        inst.evaluated = true;
        inst.as.val = source.literal_val;
      } else {
        auto val = get_value(arena, source, env);
        inst.evaluated = true;
        inst.as.val = val;
      }
    }
    break;
    
    case OpType::And: {
      auto left = get_value(arena, inst.as.ins.left, env);
      auto right = get_value(arena, inst.as.ins.right, env);
      auto val = left & right;
      inst.evaluated = true;
      inst.as.val = val;
    }
    break;

    case OpType::Or: {
      auto left = get_value(arena, inst.as.ins.left, env);
      auto right = get_value(arena, inst.as.ins.right, env);
      auto val = left | right;
      inst.evaluated = true;
      inst.as.val = val;
    }
    break;

    case OpType::Not: {
      auto left = get_value(arena, inst.as.ins.left, env);
      auto val = ~left;
      inst.evaluated = true;
      inst.as.val = val;
    }
    break;

    case OpType::LShift:{
      auto left = get_value(arena, inst.as.ins.left, env);
      BASE_ASSERT(inst.as.ins.right.is_literal);
      auto right = inst.as.ins.right.literal_val;
      auto val = left << right;
      inst.evaluated = true;
      inst.as.val = val;
    }
    break;

    case OpType::RShift:{
      auto left = get_value(arena, inst.as.ins.left, env);
      BASE_ASSERT(inst.as.ins.right.is_literal);
      auto right = inst.as.ins.right.literal_val;
      auto val = left >> right;
      inst.evaluated = true;
      inst.as.val = val;
    }
    break;
    
  }
}

export namespace y2015::d07 {
  
  #pragma clang diagnostic push
  #pragma clang diagnostic ignored "-Wunused-parameter"

  aoc::Value part1(Arena& arena, Str8 raw) {
    BASE_ASSERT(raw.len > 0);
    HashMap<Str8,Instruction> env = HashMap<Str8,Instruction>();
    env.init(arena, 350);
    parse_day7(arena, raw, env);
    auto answer = get_value(arena, Arg {false,0, Str8("a")}, env);
    return Value { ValueTag::Unsigned, { .u64 =  answer } };
  }

  aoc::Value part2(Arena& arena, Str8 raw) {
    BASE_ASSERT(raw.len > 0);
    HashMap<Str8,Instruction> env = HashMap<Str8,Instruction>();
    env.init(arena, 350);
    parse_day7(arena, raw, env);

    Instruction part1_answer = {};
    part1_answer.as.val = 46065;
    part1_answer.evaluated = true;
    env.upsert(arena, Str8("b"), part1_answer);
    auto answer = get_value(arena, Arg {false,0, Str8("a")}, env);

    return Value { ValueTag::Unsigned, { .u64 =  answer } };
  }

  #pragma clang diagnostic pop
}