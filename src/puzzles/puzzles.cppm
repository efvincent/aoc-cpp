export module puzzles;

import core_types;
import core_memory;
import core_file;
import core_string;
import core_result;
import core_text_parse;
import core_console;
import aoc_value;

import y2015d01;
import y2015d02;
import y2015d03;

using namespace base;
using namespace mem;
using namespace str;
using namespace aoc;

constexpr u16 MIN_YEAR = 2015;
constexpr u16 MAX_YEAR = 2025;
constexpr u16 MIN_DAY  = 1;
constexpr u16 MAX_DAY  = 25;

export namespace puzzles {

  /** @brief Identifies a puzzle question */
  struct PuzSpec {
    u16 year;
    u16 day;
    u8 part;  // 0 = both, 1 or 2
  };

  /** @brief Possible problems with parsing the puzzle spec */
  enum struct PuzSpecErr : u8 {
    None = 0,
    InvalidYear,
    InvalidDay,
    InvalidPart,
    Usage
  };

  void display_spec_error(PuzSpecErr e) {
    switch(e) {
      case (PuzSpecErr::InvalidDay):
        console::printl("Invalid day.");
      break;

      case (PuzSpecErr::InvalidYear):
        console::printl("Invalid year.");
      break;

      case (PuzSpecErr::InvalidPart):
        console::printl("Invalid part.");
      break;

      case (PuzSpecErr::Usage):
        console::printl("Usage: aoc YYYY DD [P]\n\n"
          " YYYY = Year, 2015-2025\n"
          " DD   = Day, 1-25\n"
          " P    = optional part. 1 or 2, omit for both.");
      break;

      case (PuzSpecErr::None):
        console::printl("Spec is good.");
      break;
    }
  }
  
  using PuzSpecR = Result<PuzSpecErr, PuzSpec>;

  PuzSpecR get_puz_spec(int argc, char** argv) {
    if (argc < 3) return PuzSpecR::err(PuzSpecErr::Usage);
    PuzSpec spec = PuzSpec{0, 0, true};
    if (argc >= 2) {
      // have at least the year
      auto year_result = parse::parse_u16(argv[1]);
      if (year_result.is_ok() && year_result.value >= MIN_YEAR && year_result.value <= MAX_YEAR) {
        spec.year = year_result.value;
      } else {
        return PuzSpecR::err(PuzSpecErr::InvalidYear);
      }
    }
    if (argc >= 3) {
      // have at least year and day.
      auto dayR = parse::parse_u16(argv[2]);
      if (dayR.is_ok() && dayR.value >= MIN_DAY && dayR.value <= MAX_DAY) {
        spec.day = dayR.value;
      } else {
        return PuzSpecR::err(PuzSpecErr::InvalidDay);
      }
    }
    
    if (argc >= 4) {
      // we possibly have all 3 components
      auto partR = parse::parse_u8(argv[3]);
      if (partR.is_ok() && partR.value >= 0 && partR.value <= 2) {
        spec.part = partR.value; 
      } else {
        return PuzSpecR::err(PuzSpecErr::InvalidPart);
      }
    }
    return PuzSpecR::ok(spec);
  }

  Result<bool, Str8> get_data_filename(Arena& arena, const PuzSpec spec) {
    auto e = format_cap(arena, 100, "./data/%d/day%02d.txt", spec.year, spec.day);
    if (e.is_ok()) {
      return Result<bool, Str8>::ok(e.value);
    } else {
      console::write_stderr(Str8("failed to compute filename"));
      return Result<bool, Str8>::err(false); 
    }
  }

  void print_ans(Arena& arena, const PuzSpec& spec, const aoc::Value ans) {
    Str8 ans_str = aoc::value_to_str8(arena, ans);
    console::printfl_cap(arena, 100, "day %d of year %d part 1 ans: %.*s", 
      spec.day, spec.year, (int)ans_str.len, ans_str.str);
  }
  
  /** @brief runs the specified puzzle */
  void runPuzzle(Arena& arena, const PuzSpec spec) {
    auto fn = get_data_filename(arena, spec);
    if (!fn.is_ok()) return;
    auto rawR = fs::read_all(arena, fn.value.to_cstr(arena));
    if (!rawR.is_ok()) {
      console::write_stderr(Str8("failed to read data file"));
      return;
    }

    auto answers = arena.alloc_array<aoc::Value>(2);
    switch (spec.year) {
      case 2015:

        switch (spec.day) {

          case 1:
            if (spec.part == 1 || spec.part == 0) {
              answers[0] = y2015::d01::part1(rawR.value);
            }
            if (spec.part == 2 || spec.part == 0) {
              answers[1] = y2015::d01::part2(rawR.value);
            }
          break;

          case 2:
            if (spec.part == 1 || spec.part == 0) {
              answers[0] = y2015::d02::part1(rawR.value);
            }
            if (spec.part == 2 || spec.part == 0) {
              answers[1] = y2015::d02::part2(rawR.value);
            }
          break;

          case 3:
            if (spec.part == 1 || spec.part == 0) {
              answers[0] = y2015::d03::part1(arena, rawR.value);
            }
            if (spec.part == 2 || spec.part == 0) {
              answers[1] = y2015::d03::part2(arena, rawR.value);
            }
          break;

          default:
            console::print_err(Str8("Unknown spec or spec not yet implemented."));
            return;
        } /* switch day */

      break;  // case year == 2015

      default:
        auto msg = str::format_cap(arena, 50, "Year %lu not yet implemented.\n", spec.year);
        console::print_err(msg.value);
        return;
    } /* switch year */

    // success, print the answer
    if (spec.part == 1 || spec.part == 0) print_ans(arena, spec, answers[0]);
    if (spec.part == 2 || spec.part == 0) print_ans(arena, spec, answers[1]);
  } 
}