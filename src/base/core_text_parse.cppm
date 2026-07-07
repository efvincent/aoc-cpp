module;
// Global module fragment for intrinsics if needed
#include <limits>
#include <type_traits>

export module core_text_parse;

import core_types;
import core_result_s;
import core_string;

export namespace base {

  /**
   * @defgroup StringResolution Zero-Allocation String Parsing
   * @brief How to handle escaped string literals in downstream modules.
   *
   * In this Data-Oriented architecture, the core lexer and parser utilities
   * are strictly zero-allocation. When the lexer extracts a string literal
   * (for example, "Hello\\nWorld"), it returns a raw Str8 slice directly
   * pointing to the source buffer. This means escape sequences (for example,
   * "\\n", "\\t", and "\\\"") remain physical, multi-byte characters in
   * the slice.
   *
   * Because resolving escapes requires a string to shrink in size and physically
   * decouple from the source buffer, it requires memory allocation. Therefore,
   * resolving strings is the responsibility of the Coordinator Layer
   * (for example, your AST Builder or String Interner), not the core utility
   * layer.
   *
   * This separation preserves deterministic behavior in low-level parsing code
   * and keeps ownership boundaries explicit: parser utilities only classify and
   * slice, while downstream systems decide allocation policy and lifetime.
   *
   * @par Downstream Implementation Guide
   * When your AST module requires the clean, resolved bytes of a string, you
   * should implement an allocation bridge using your Arena. Feel free to copy
   * and paste the following implementation into your AST builder:
   *
   * @code
   * // Inside my_language_ast.cppm (or similar)
   * import core_memory;
   * import core_text_parse;
   *
   * namespace my_language {
   *
   *    Str8 resolve_string_literal(Arena* arena, Str8 raw_slice) {
   *        // Pre-allocate worst-case size (the original string length)
   *        u8* dest = arena_push_array(arena, u8, raw_slice.len);
   *        u64 write_idx = 0;
   *
   *        for (u64 i = 0; i < raw_slice.len; ++i) {
   *            if (raw_slice.str[i] == '\\' && i + 1 < raw_slice.len) {
   *                i++; // Skip the backslash
   *                switch (raw_slice.str[i]) {
   *                    case 'n':  dest[write_idx++] = '\n'; break;
   *                    case 'r':  dest[write_idx++] = '\r'; break;
   *                    case 't':  dest[write_idx++] = '\t'; break;
   *                    case '\\': dest[write_idx++] = '\\'; break;
   *                    case '"':  dest[write_idx++] = '"';  break;
   *                    // Keep unknown escapes as raw bytes
   *                    default:   dest[write_idx++] = raw_slice.str[i]; break;
   *               }
   *            } else {
   *                dest[write_idx++] = raw_slice.str[i];
   *            }
   *        }
   *
   *        // Return the bounded slice representing the exact resolved length
   *        return Str8{dest, write_idx};
   *    }
   *
   * } // namespace my_language
   * @endcode
   */

  //-------------------------------------------------------------
  // Location and meta utilities 
  //------------------------------------------------------------ 

  /**
   * @brief 1-based source location measured in bytes.
   */
  struct SourceLocation {
    /** @brief 1-based line index. */
    u32 line;
    /** @brief 1-based column index. */
    u32 column;
  };

  /**
   * @brief Compute line/column location for a byte offset.
   * @param buffer Full source buffer.
   * @param offset Byte offset into `buffer`.
   * @return 1-based line/column location clamped at end-of-buffer.
   */
  constexpr SourceLocation compute_location(Str8 buffer, u64 offset) {
    u32 line = 1;
    u32 col = 1;
    u64 limit = (offset < buffer.len) ? offset : buffer.len;

    for (u64 i = 0; i < limit; ++i) {
      if (buffer.str[i] == '\n') {
        line++;
        col = 1;
      } else {
        col++;
      }
    }
    return {line, col};
  }

  /**
   * @brief High-performance constexpr FNV-1a hash for string lookups/keywords.
   * @param str Input byte slice.
   * @param seed Optional basis value for variant hash domains.
   * @return 64-bit FNV-1a hash value.
   */
  constexpr u64 hash_fnv1a(Str8 str, u64 seed = 0xcbf29ce484222325ull) {
      u64 hash = seed;
      for (u64 i = 0; i < str.len; ++i) {
          hash ^= str.str[i];
          hash *= 0x100000001b3ull;
      }
      return hash;
  }  

  //-------------------------------------------------------------
  // Integer Parsers (template engine)
  //-------------------------------------------------------------

  /**
   * @brief Generic integer parser for ASCII decimal slices.
   *
   * Parses an optional sign for signed integer targets and then validates each
   * remaining byte as `[0-9]`.
   *
   * @note Strict parser contract: the full input slice must be a valid integer.
   * Any non-digit payload after an optional sign fails immediately.
   *
   * @par Data-oriented separation
   * This routine is intentionally pure "math" logic and does not touch cursor
   * state. Cursor movement belongs in lexer `consume_*` style functions.
   *
   * @par Signed safety strategy
   * Digits are accumulated in the unsigned counterpart of `T` so intermediate
   * multiply/add steps cannot trigger signed overflow UB. A final range gate
   * then applies sign and validates representability for the target type.
   *
   * @tparam T Integer destination type.
   * @param text Input byte slice.
   * @return Parsed integer or error when invalid/empty/overflowing.
   */
  template <typename T>
  constexpr ResultS<T> parse_int_impl(Str8 text) {
    if (text.len == 0) return ResultS<T>::err("Empty string");

    u64 i = 0;
    bool is_negative = false;

    // compile time branch: check sign handling only if type is signed
    if constexpr(static_cast<T>(-1) < 0) {
      if (text.str[0] == '-') {
        is_negative = true;
        i++;
      } else if (text.str[0] == '+') {
        i++;
      }
    }

    if (i == text.len) return ResultS<T>::err("No digits found");

    // Accumulate in unsigned space first; signed range checks happen once.
    using UnsignedT = std::make_unsigned_t<T>;
    UnsignedT result = 0;

    for (; i < text.len; ++i) {
      u8 c = text.str[i];
      if (c < '0' || c > '9') {
        return ResultS<T>::err("Invalid numeric character");
      }
      UnsignedT digit = c - '0';

      // Pre-multiply overflow check: result * 10 + digit must stay representable.
      if (result > (std::numeric_limits<UnsignedT>::max() - digit) / 10) {
        return ResultS<T>::err("Integer overflow");
      }
      result = result * 10 + digit;
    }

    // Final sign application and target-range validation
    // Portability safe path: never cast an out-of-range unsigned magnitude to signed.
    if constexpr (static_cast<T>(-1) < 0) {
      const UnsignedT max_pos = static_cast<UnsignedT>(std::numeric_limits<T>::max());
      const UnsignedT max_neg_mag = max_pos + 1;  // |T::min()|

      if (is_negative) {
        if (result > max_neg_mag) {
          return ResultS<T>::err("Integer overflow");
        }
        if (result == max_neg_mag) {
          return ResultS<T>::ok(std::numeric_limits<T>::min());
        }

        // result <= max_pos, so cast to T is representable
        T mag = static_cast<T>(result);
        return ResultS<T>::ok(static_cast<T>(-mag));
      }

      if (result > max_pos) {
        return ResultS<T>::err("Integer overflow");
      }
      return ResultS<T>::ok(static_cast<T>(result));
    }
    return ResultS<T>::ok(static_cast<T>(result));
  }

  /** @brief Parse `Str8` into `u8`. */
  constexpr ResultS<u8>  parse_u8(Str8 t)  { return parse_int_impl<u8>(t); }
  /** @brief Parse `Str8` into `i8`. */
  constexpr ResultS<i8>  parse_i8(Str8 t)  { return parse_int_impl<i8>(t); }
  /** @brief Parse `Str8` into `u16`. */
  constexpr ResultS<u16> parse_u16(Str8 t) { return parse_int_impl<u16>(t); }
  /** @brief Parse `Str8` into `i16`. */
  constexpr ResultS<i16> parse_i16(Str8 t) { return parse_int_impl<i16>(t); }
  /** @brief Parse `Str8` into `u32`. */
  constexpr ResultS<u32> parse_u32(Str8 t) { return parse_int_impl<u32>(t); }
  /** @brief Parse `Str8` into `i32`. */
  constexpr ResultS<i32> parse_i32(Str8 t) { return parse_int_impl<i32>(t); }
  /** @brief Parse `Str8` into `u64`. */
  constexpr ResultS<u64> parse_u64(Str8 t) { return parse_int_impl<u64>(t); }
  /** @brief Parse `Str8` into `i64`. */
  constexpr ResultS<i64> parse_i64(Str8 t) { return parse_int_impl<i64>(t); }  


  //-------------------------------------------------------------
  // Floating point parsers
  //-------------------------------------------------------------

  /**
   * @brief Parse decimal/scientific ASCII into `f64`.
   *
   * Supports optional sign, fractional part, and exponent marker (`e`/`E`).
   *
   * @param text Input byte slice.
   * @return Parsed `f64` or error if no digits are present.
   */
  constexpr ResultS<f64> parse_f64(Str8 text) {
    if (text.len == 0) {
      return ResultS<f64>::err("Empty string");
    }

    u64 i = 0;
    bool is_negative = false;

    if (text.str[0] == '-') { 
      is_negative = true; 
      i++; 
    } else if (text.str[0] == '+') {
      i++;
    }

    f64 result = 0.0;
    bool found_digits = false;

    // Mantissa: integer part
    while (i < text.len && text.str[i] >= '0' && text.str[i] <= '9') {
      result = result * 10.0 + (text.str[i] - '0');
      i++;
      found_digits = true;
    }

    // Mantissa: fractional part
    if (i < text.len && text.str[i] == '.') {
      i++;
      f64 fraction = 1.0;
      while (i < text.len && text.str[i] >= '0' && text.str[i] <= '9') {
        fraction *= 0.1;
        result += (text.str[i] - '0') * fraction;
        i++;
        found_digits = true;
      }
    }

    if (!found_digits) {
      return ResultS<f64>::err("No valid digits");
    }

    // Exponent part (for example, e-4)
    // Strict rule: at least one digit is required after e, e+, or e-.
    // Performance rule: 10^exp is computed with exponentiation by squaring
    // to keep multiplication cost logarithmic in the exponent size.
    if (i < text.len && (text.str[i] == 'e' || text.str[i] == 'E')) {
      i++;
      bool exp_negative = false;
      if (i < text.len && text.str[i] == '-') {
        exp_negative = true;
        i++;
      } else if (i < text.len && text.str[i] == '+') {
        i++;
      }

      u64 exp_digits_start = i;
      u32 exp_val = 0;
      while (i < text.len && text.str[i] >= '0' && text.str[i] <= '9') {
        u32 digit = static_cast<u32>(text.str[i] - '0');
        if (exp_val > (std::numeric_limits<u32>::max() - digit) / 10) {
          return ResultS<f64>::err("Exponent overflow");
        }
        exp_val = exp_val * 10 + (text.str[i] - '0');
        i++;
      }

      if (i == exp_digits_start) {
        return ResultS<f64>::err("Invalid exponent");
      }

      // Compute 10^e using binary exponentiation.
      // Invariant during loop: out * base^(remaining e) == 10^(original e).
      // Complexity: O(log e) multiplications instead of O(e).
      auto pow10 = [](u32 e) constexpr -> f64 {
        f64 base = 10.0;
        f64 out = 1.0;
        while (e > 0) {
          if (e & 1u) {
            out *= base;
          }
          base *= base;
          e >>= 1u;
        }
        return out;
      };

      f64 p = pow10(exp_val);
      if (exp_negative) {
        result /= p;
      } else {
        result *= p;
      }
    }

    if (i != text.len) {
      return ResultS<f64>::err("Trailing characters");
    }

    return ResultS<f64>::ok(is_negative ? -result : result);
  }

  /**
   * @brief Parse decimal/scientific ASCII into `f32` via `parse_f64`.
   * @param text Input byte slice.
   * @return Parsed `f32` or forwarded parse error.
   */
  constexpr ResultS<f32> parse_f32(Str8 text) {
    auto res = parse_f64(text);
    if (!res.is_ok()) {
      return ResultS<f32>::err(res.error);
    }
    return ResultS<f32>::ok(static_cast<f32>(res.value));
  }

}

