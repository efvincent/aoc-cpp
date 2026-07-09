export module core_text_parse;

import core_types;
import core_result;
import core_string;
import core_portability;

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
   * @par Data-oriented separation
   * This routine is intentionally pure "math" logic and does not touch cursor
   * state. Cursor movement belongs in lexer `consume_*` style functions.
   *
   * @par Coordinator contract
   * These strict parsers validate only the provided slice and return success or
   * failure for that slice. They do not perform recovery, cursor rewinds, or
   * diagnostic list mutation. In language-server style workflows, a coordinator
   * layer is responsible for aggregating diagnostics and selecting recovery steps.
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
   * @brief Precomputed line-start offsets for O(log N) location lookup.
   *
   * `line_starts[i]` stores the byte offset for 1-based line `i+1`.
   */
  struct LineIndex {
    /** @brief Pointer to caller-owned line-start offset storage. */
    const u64* line_starts;
    /** @brief Number of entries in @ref line_starts. */
    u32 line_count;
  };


  /**
   * @brief Compute required entry count for a line-start index.
   *
   * Always returns at least 1 (line 1 at offset 0).
   */
  constexpr u32 line_index_required_capacity(Str8 buffer) {
    u32 lines = 1;
    for(u64 i = 0; i < buffer.len; ++i) {
      if (buffer.str[i] == '\n') {
        lines++;
      }
    }
    return lines;
  }

  /**
   * @brief Build a line-start index into caller-provided storage.
   *
   * @param buffer Source buffer.
   * @param out_line_starts Destination storage for line starts.
   * @param capacity Number of writable entries in @p out_line_starts.
   * @param out_index Receives the built index view.
   * @return True on success, false for invalid args or insufficient capacity.
   */
  constexpr bool line_index_build(Str8 buffer, u64* out_line_starts, u32 capacity, LineIndex* out_index) {
    if (out_line_starts == nullptr || out_index == nullptr || capacity == 0) {
      return false;
    }

    u32 needed = line_index_required_capacity(buffer);
    if (capacity < needed) {
      return false;
    }

    u32 write = 0;
    out_line_starts[write++] = 0;   // line 1 always starts at offset 0

    for (u64 i = 0; i < buffer.len; ++i) {
      if (buffer.str[i] == '\n') {
        out_line_starts[write++] = i + 1;
      }
    }

    out_index->line_starts = out_line_starts;
    out_index->line_count = write;
    return true;
  }

  /**
   * @brief Diagnostic severity for parser/lexer recovery workflows.
   * 
   */
  enum struct DiagnosticSeverity : u8 {
    Note = 0,
    Warning,
    Error
  };

  /**
   * @brief Stable diagnostic code identifiers for text parse failures.
   *
   * Keep this list compact and allocator-free for code-first diagnostics.
   * Human-readable text is a coordinator/reporting concern.
   */
  enum struct ParseDiagCode : u16 {
    None = 0,
    EmptyInput,
    NoDigits,
    InvalidNumericChar,
    IntOverflow,
    InvalidExponent,
    ExponentOverflow,
    TrailingChars,
    UnterminatedString,
    UnterminatedBlockComment
  };

  struct ParseDiagnostic {
    ParseDiagCode code;
    DiagnosticSeverity severity;
    u64 start_offset;
    u64 end_offset;
  };

  template <typename T> 
  using ParseResult = Result<ParseDiagCode, T>;

  static_assert(portability::is_standard_layout_v<ParseDiagnostic>, 
                "ParseDiagnostic must remain standard layout.");
  static_assert(portability::is_trivially_copyable_v<ParseDiagnostic>, 
                "ParseDiagnostic must remain trivially copyable.");

  constexpr SourceLocation compute_location(Str8 buffer, u64 offset) {
    u32 line = 1;
    u32 col = 1;
    u64 limit = base::Min(offset, buffer.len);
    
    for (u64 i = 0; i < limit; ++i) {
      if (buffer.str[i] == '\n') {
        line++;
        col = 1;
      } else {
        col++;
      }
    }
    return {line,col};
  }

  /**
   * @brief Compute line/column location from a precomputed line index.
   * @param index Precomputed line-start index.
   * @param buffer_len Source buffer length used for clamping.
   * @param offset Byte offset into source data.
   * @return 1-based line/column location clamped at end-of-buffer.
   */
  constexpr SourceLocation compute_location(LineIndex index, u64 buffer_len, u64 offset) {
    if (index.line_starts == nullptr || index.line_count == 0) {
      return {1, 1};
    }

    u64 clamped = (offset < buffer_len) ? offset : buffer_len;

    // upper_bound(line_starts, clamped)
    u32 lo = 0;
    u32 hi = index.line_count;
    while (lo < hi) {
      u32 mid = lo + ((hi - lo) / 2);
      if (index.line_starts[mid] <= clamped) {
        lo = mid + 1;
      } else {
        hi = mid;
      }
    }

    u32 line_idx = (lo == 0) ? 0 : (lo - 1);
    u64 line_start = index.line_starts[line_idx];
    u64 col64 = (clamped - line_start) + 1;
    u32 column = (col64 > static_cast<u64>(portability::numeric_limits<u32>::max()))
                    ? portability::numeric_limits<u32>::max()
                    : static_cast<u32>(col64);
    return {line_idx + 1, column};
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
  constexpr ParseResult<T> parse_int_impl(Str8 text) {
    if (text.len == 0) return ParseResult<T>::err(ParseDiagCode::EmptyInput);

    u64 i = 0;
    bool is_negative = false;

    // compile time branch: check sign handling only if type is signed
    if (base::portability::is_signed_v<T>) {
      if (text.str[0] == '-') {
        is_negative = true;
        i++;
      } else if (text.str[0] == '+') {
        i++;
      }
    }

    if (i == text.len) return ParseResult<T>::err(ParseDiagCode::NoDigits);

    // Accumulate in unsigned space first; signed range checks happen once.
    using UnsignedT = portability::make_unsigned_t<T>;
    UnsignedT result = 0;

    for (; i < text.len; ++i) {
      u8 c = text.str[i];
      if (c < '0' || c > '9') {
        return ParseResult<T>::err(ParseDiagCode::InvalidNumericChar);
      }
      UnsignedT digit = c - '0';

      // Pre-multiply overflow check: result * 10 + digit must stay representable.
      if (result > (portability::numeric_limits<UnsignedT>::max() - digit) / 10) {
        return ParseResult<T>::err(ParseDiagCode::IntOverflow);
      }
      result = result * 10 + digit;
    }

    // Final sign application and target-range validation
    // Portability safe path: never cast an out-of-range unsigned magnitude to signed.
    if constexpr (portability::is_signed_v<T>) {
      const UnsignedT max_pos = static_cast<UnsignedT>(portability::numeric_limits<T>::max());
      const UnsignedT max_neg_mag = max_pos + 1;  // |T::min()|

      if (is_negative) {
        if (result > max_neg_mag) {
          return ParseResult<T>::err(ParseDiagCode::IntOverflow);
        }
        if (result == max_neg_mag) {
          return ParseResult<T>::ok(portability::numeric_limits<T>::min());
        }

        // result <= max_pos, so cast to T is representable
        T mag = static_cast<T>(result);
        return ParseResult<T>::ok(static_cast<T>(-mag));
      }

      if (result > max_pos) {
        return ParseResult<T>::err(ParseDiagCode::IntOverflow);
      }
      return ParseResult<T>::ok(static_cast<T>(result));
    }
    return ParseResult<T>::ok(static_cast<T>(result));
  }

  /** @brief Parse `Str8` into `u8`. */
  constexpr ParseResult<u8>  parse_u8(Str8 t)  { return parse_int_impl<u8>(t); }
  /** @brief Parse `Str8` into `i8`. */
  constexpr ParseResult<i8>  parse_i8(Str8 t)  { return parse_int_impl<i8>(t); }
  /** @brief Parse `Str8` into `u16`. */
  constexpr ParseResult<u16> parse_u16(Str8 t) { return parse_int_impl<u16>(t); }
  /** @brief Parse `Str8` into `i16`. */
  constexpr ParseResult<i16> parse_i16(Str8 t) { return parse_int_impl<i16>(t); }
  /** @brief Parse `Str8` into `u32`. */
  constexpr ParseResult<u32> parse_u32(Str8 t) { return parse_int_impl<u32>(t); }
  /** @brief Parse `Str8` into `i32`. */
  constexpr ParseResult<i32> parse_i32(Str8 t) { return parse_int_impl<i32>(t); }
  /** @brief Parse `Str8` into `u64`. */
  constexpr ParseResult<u64> parse_u64(Str8 t) { return parse_int_impl<u64>(t); }
  /** @brief Parse `Str8` into `i64`. */
  constexpr ParseResult<i64> parse_i64(Str8 t) { return parse_int_impl<i64>(t); }  


  //-------------------------------------------------------------
  // Floating point parsers
  //-------------------------------------------------------------

  /**
   * @brief Parse decimal/scientific ASCII into `f64`.
   *
   * Supports optional sign, fractional part, and exponent marker (`e`/`E`).
   *
   * @param text Input byte slice.
   * @return Parsed `f64` on full-slice success; otherwise an error for empty
   *         input, invalid exponent, exponent overflow, trailing characters,
   *         or missing digits.
   * @note This function is strict over the passed slice only. Error accumulation
   *       and continued parsing across a full buffer are coordinator-layer tasks.
   */
  constexpr ParseResult<f64> parse_f64(Str8 text) {
    if (text.len == 0) {
      return ParseResult<f64>::err(ParseDiagCode::EmptyInput);
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
      return ParseResult<f64>::err(ParseDiagCode::NoDigits);
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
        if (exp_val > (portability::numeric_limits<u32>::max() - digit) / 10) {
          return ParseResult<f64>::err(ParseDiagCode::ExponentOverflow);
        }
        exp_val = exp_val * 10 + (text.str[i] - '0');
        i++;
      }

      if (i == exp_digits_start) {
        return ParseResult<f64>::err(ParseDiagCode::InvalidExponent);
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
      return ParseResult<f64>::err(ParseDiagCode::TrailingChars);
    }

    return ParseResult<f64>::ok(is_negative ? -result : result);
  }

  /**
   * @brief Parse decimal/scientific ASCII into `f32` via `parse_f64`.
   * @param text Input byte slice.
   * @return Parsed `f32` or forwarded parse error.
   */
  constexpr ParseResult<f32> parse_f32(Str8 text) {
    auto res = parse_f64(text);
    if (!res.is_ok()) {
      return ParseResult<f32>::err(res.error);
    }
    return ParseResult<f32>::ok(static_cast<f32>(res.value));
  }

}

