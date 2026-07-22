module;

#include "../../base/core_debug.h"

export module y2015d09;

import core_types;
import core_string;
import core_result;
import core_memory;
import core_lexer;
import core_hash_map;
import core_text_parse;
import core_console;
import aoc_value;

using namespace base;
using namespace str;
using namespace aoc;
using namespace console;

export namespace y2015::d09 {

  constexpr const u8 LOCATION_COUNT = 8;

  u32 min_cost = max_u32;
  u32 max_cost = 0;

  using CityMap = HashMap<Str8, u8>;
  using Matrix = u16[LOCATION_COUNT][LOCATION_COUNT];

  Matrix mat = {};
  CityMap idx_map = {};

  void parse_line(Arena& arena, Str8 line) {
    
    static u8 next_city_idx = 0;
    u8 word_idx = 0;
    Str8 cursor = line;
    Str8 word = {};
    u8 city1_idx = {};
    u8 city2_idx = {};

    while(iter_next(cursor, word, ' ')) {
      switch (word_idx) {
        case 0: {
          // first city
          auto c1R = idx_map.find(word);
          if (c1R.is_ok() && c1R.value.is_some()) {
            city1_idx = *c1R.value.unwrap();
          } else {
            idx_map.upsert(arena, word, next_city_idx);
            city1_idx = next_city_idx++;
          }
        }
        break;

        case 2: {
          //second city
          auto c2R = idx_map.find(word);
          if (c2R.is_ok() && c2R.value.is_some()) {
            city2_idx = *c2R.value.unwrap();
          } else {
            idx_map.upsert(arena, word, next_city_idx);
            city2_idx = next_city_idx++;
          }
        }
        break;

        case 4: {
          // cost
          auto costR = parse::parse_u8(word);
          BASE_ASSERT(costR.is_ok());
          mat[city1_idx][city2_idx] = costR.value;          
          mat[city2_idx][city1_idx] = costR.value;          
        }
        break;
      }
      word_idx++;
    }
  }
  
  void parse(Arena &arena, Str8 raw) {
      if (!idx_map.is_initialized()) {
        idx_map.init(arena, 8);
      }
      Str8 cursor = raw;
      Str8 line = {};
      while (iter_next(cursor, line, '\n')) {
        parse_line(arena, line);
      }
  }

  // Pure function signature operating on registers
  void dfs_search(
      Matrix &matrix,
      u8  node_count,
      u8  current_node,
      u8  mask,
      u8  visited,
      u32 current_cost,
      u32& out_min,
      u32& out_max
  ) {
    if (visited == node_count) {
      out_min = Min(out_min, current_cost);
      out_max = Max(out_max, current_cost);
      return;
    }
    for (u64 next_node = 0; next_node < node_count; next_node++) {
      bool is_visited = (mask & (1 << next_node)) != 0;
      if (!is_visited) {
        auto edge = matrix[current_node][next_node];
        auto next_mask = mask | (1 << next_node);
        dfs_search(matrix, node_count, next_node, next_mask, visited + 1, current_cost + edge, out_min, out_max);
      }
    }
  }

  void print_matrix(Arena& arena, u16 (&m)[8][8] ) {
    printl("\n     0   1   2   3   4   5   6   7"
           "\n  ---------------------------------");
    for (u16 i = 0; i < 8; i++) {
      auto msg = format_cap(arena, 100, "%d | %03d %03d %03d %03d %03d %03d %03d %03d",
        i, m[i][0], m[i][1], m[i][2], m[i][3], m[i][4], m[i][5], m[i][6], m[i][7]);
      printl(msg.value); 
    }
    printl("  ---------------------------------");
  }

  aoc::Value part1(Arena& arena, Str8 raw) {
    BASE_ASSERT(raw.len > 0);
    arena.scoped_scratch([&](Arena& scratch) {
      parse(scratch, raw);
      for (u8 start_node = 0; start_node < LOCATION_COUNT; ++start_node) {
        u8 initial_mask = (1 << start_node);
        dfs_search(mat, LOCATION_COUNT, start_node, initial_mask, 1, 0, min_cost, max_cost);
      }

    });
    return Value { ValueTag::Unsigned, { .u64 = min_cost } };
  }

  aoc::Value part2(Arena& arena, Str8 raw) {
    BASE_ASSERT(raw.len > 0);
    arena.scoped_scratch([&](Arena& scratch) {
      scratch.alloc_array<u64>(20);
    });

    if (max_cost != 0) {
      return Value { ValueTag::Unsigned, { .u64 = max_cost }};
    }

    arena.scoped_scratch([&](Arena& scratch) {
      parse(scratch, raw);
      for (u8 start_node = 0; start_node < LOCATION_COUNT; ++start_node) {
        u8 initial_mask = (1 << start_node);
        dfs_search(mat, LOCATION_COUNT, start_node, initial_mask, 1, 0, min_cost, max_cost);
      }
    });

    return Value { ValueTag::Unsigned, { .u64 = max_cost } };
  }    
  
}