/**
 * @file core_lexer_coord.cppm
 * @brief Coordinator-layer wrappers that bind lexer cursor operations to
 *        diagnostic accumulation.
 */

/**
 * @module core_lexer_coord
 * @brief Explicit recovery/diagnostic coordination layer above pure lexer/parse primitives.
 */

export module core_lexer_coord;

import core_types;
import core_string;
import core_lexer;
import core_text_parse;
import core_result;

export namespace base::lex {
  /**
   * @brief Failure categories for diagnostic buffer append operations.
   */
  enum struct DiagBufferError : u8 {
    None = 0,
    NullStorage,
    BufferFull
  };

  /** @brief Result alias for diagnostic-buffer append operations. */
  using DiagBufferPushResult = Result<DiagBufferError, u32>;

  /**
   * @brief Caller-owned fixed-capacity diagnostic accumulation buffer.
   *
   * Coordinator wrappers append diagnostics to this buffer when recovery-aware
   * cursor operations encounter consumed failures.
   */
  struct DiagBuffer {
    /** @brief Destination storage for diagnostics. */
    base::parse::ParseDiagnostic* data;
    /** @brief Number of populated entries in @ref data. */
    u32 count;
    /** @brief Maximum writable entries in @ref data. */
    u32 capacity;
  };

  /**
   * @brief Append one diagnostic to a caller-owned diagnostic buffer.
   * @param buffer Diagnostic buffer.
   * @param diag Diagnostic record to append.
   * @return Ok(new_count) when the diagnostic is appended, or Err(DiagBufferError)
   *         when storage is null or capacity is exhausted.
   */
  constexpr DiagBufferPushResult diag_buffer_push(DiagBuffer& buffer, base::parse::ParseDiagnostic diag) {
    if (buffer.data == nullptr) {
      return DiagBufferPushResult::err(DiagBufferError::NullStorage);
    }
    if (buffer.count >= buffer.capacity) {
      return DiagBufferPushResult::err(DiagBufferError::BufferFull);
    }
    
    buffer.data[buffer.count] = diag;
    buffer.count++;
    return DiagBufferPushResult::ok(buffer.count);
  }

  /**
   * @brief Coordinator wrapper for strict integer consume + diagnostic emission.
   *
   * Emits a diagnostic only when bytes were consumed and strict parsing failed.
   * Prefix-miss failures are returned without diagnostic emission.
   *
   * @tparam T Integer result type.
   * @param cursor Input cursor.
   * @param parser Strict parse callback.
   * @param diags Destination diagnostic buffer.
   * @return Parsed integer result or error code from strict parser.
   */
  template <typename T>
  constexpr ParseResult<T> coord_consume_int(Str8Cursor& cursor,
                                             ParseResult<T> (*parser)(base::str::Str8),
                                             DiagBuffer& diags) {
    u64 start = cursor.offset;
    ParseResult<T> res = cursor.consume_int_impl<T>(parser);
    if (!res.is_ok() && cursor.offset > start) {
      base::parse::ParseDiagnostic diag = make_error_diag(res.error, start, cursor.offset);
      (void)diag_buffer_push(diags, diag);
    }
    return res;
  }

  /** @brief Coordinator wrapper for consuming `u8` with diagnostic emission. */
  constexpr ParseResult<u8> coord_consume_u8(Str8Cursor& cursor, DiagBuffer& diags) {
    return coord_consume_int<u8>(cursor, base::parse::parse_u8, diags);
  }

  /** @brief Coordinator wrapper for consuming `i8` with diagnostic emission. */
  constexpr ParseResult<i8> coord_consume_i8(Str8Cursor& cursor, DiagBuffer& diags) {
    return coord_consume_int<i8>(cursor, base::parse::parse_i8, diags);
  }

  /** @brief Coordinator wrapper for consuming `u16` with diagnostic emission. */
  constexpr ParseResult<u16> coord_consume_u16(Str8Cursor& cursor, DiagBuffer& diags) {
    return coord_consume_int<u16>(cursor, base::parse::parse_u16, diags);
  }

  /** @brief Coordinator wrapper for consuming `i16` with diagnostic emission. */
  constexpr ParseResult<i16> coord_consume_i16(Str8Cursor& cursor, DiagBuffer& diags) {
    return coord_consume_int<i16>(cursor, base::parse::parse_i16, diags);
  }

  /** @brief Coordinator wrapper for consuming `u32` with diagnostic emission. */
  constexpr ParseResult<u32> coord_consume_u32(Str8Cursor& cursor, DiagBuffer& diags) {
    return coord_consume_int<u32>(cursor, base::parse::parse_u32, diags);
  }

  /** @brief Coordinator wrapper for consuming `i32` with diagnostic emission. */
  constexpr ParseResult<i32> coord_consume_i32(Str8Cursor& cursor, DiagBuffer& diags) {
    return coord_consume_int<i32>(cursor, base::parse::parse_i32, diags);
  }

  /** @brief Coordinator wrapper for consuming `u64` with diagnostic emission. */
  constexpr ParseResult<u64> coord_consume_u64(Str8Cursor& cursor, DiagBuffer& diags) {
    return coord_consume_int<u64>(cursor, base::parse::parse_u64, diags);
  }

  /** @brief Coordinator wrapper for consuming `i64` with diagnostic emission. */
  constexpr ParseResult<i64> coord_consume_i64(Str8Cursor& cursor, DiagBuffer& diags) {
    return coord_consume_int<i64>(cursor, base::parse::parse_i64, diags);
  }

  /**
   * @brief Coordinator wrapper for consuming `f64` with diagnostic emission.
   *
   * Emits diagnostics for consumed strict failures only.
   */
  constexpr ParseResult<f64> coord_consume_f64(Str8Cursor& cursor, DiagBuffer& diags) {
    u64 start = cursor.offset;
    ParseResult<f64> res = cursor.consume_f64();
    if (!res.is_ok() && cursor.offset > start) {
      base::parse::ParseDiagnostic diag = make_error_diag(res.error, start, cursor.offset);
      (void)diag_buffer_push(diags, diag);
    }
    return res;
  }

  /**
   * @brief Coordinator wrapper for consuming `f32` with diagnostic emission.
   *
   * This delegates windowing and diagnostic emission to @ref coord_consume_f64
   * and narrows successful values to `f32`.
   */
  constexpr ParseResult<f32> coord_consume_f32(Str8Cursor& cursor, DiagBuffer& diags) {
    auto res = coord_consume_f64(cursor, diags);
    if (!res.is_ok()) {
      return ParseResult<f32>::err(res.error);
    }
    return ParseResult<f32>::ok(static_cast<f32>(res.value));
  }

  /**
   * @brief Coordinator wrapper for string literal parsing with diagnostics.
   *
   * Strict parsing remains in @ref Str8Cursor::parse_string_literal. This
   * wrapper binds that result to source-span diagnostics.
   */
  constexpr ParseResult<u64> coord_parse_string_literal(Str8Cursor& cursor, DiagBuffer& diags) {
    u64 start = cursor.offset;
    ParseResult<u64> res = cursor.parse_string_literal();
    if (!res.is_ok() && cursor.offset > start) {
      base::parse::ParseDiagnostic diag = make_error_diag(res.error, start, cursor.offset);
      (void)diag_buffer_push(diags, diag);
    }
    return res;
  }

  /**
   * @brief Coordinator wrapper for trivia skipping with diagnostics.
   *
   * Reports unterminated block comments while preserving lexer cursor progress.
   */
  constexpr ParseResult<u8> coord_skip_trivia(Str8Cursor& cursor, TriviaConfig config, DiagBuffer& diags) {
    while (!cursor.is_eof()) {
      // skip standard whitespace
      if (cursor.is_whitespace(cursor.peek())) {
        cursor.offset++;
        continue;  // while(!cursor.is_eof)
      }

      // skip line comments
      if (config.line_mark.len > 0 && cursor_starts_with(cursor, config.line_mark)) {
        while (!cursor.is_eof() && cursor.peek() != '\n') {
          cursor.offset++;
        }
        continue;  // while(!cursor.is_eof)
      }

      // Skip block comments; report unterminated blocks
      if (config.block_start.len > 0 && cursor_starts_with(cursor, config.block_start)) {
        u64 block_start_offset = cursor.offset;
        cursor.offset += config.block_start.len;
        u32 depth = 1;

        while (!cursor.is_eof() && depth > 0) {
          if (config.nested && cursor_starts_with(cursor, config.block_start)) {
            depth++;
            cursor.offset += config.block_start.len;
          } else if (cursor_starts_with(cursor, config.block_end)) {
            depth--;
            cursor.offset += config.block_end.len;
          } else {
            cursor.offset++;
          }
        }

        if (depth > 0) {
          auto code = base::parse::ParseDiagCode::UnterminatedBlockComment;
          base::parse::ParseDiagnostic diag = make_error_diag(code, block_start_offset, cursor.offset);
          (void)diag_buffer_push(diags, diag);
          return ParseResult<u8>::err(code);
        }
        continue;  // while(!cursor.is_eof)
      }
      break;    // while(!cursor.is_eof)
    }
    return ParseResult<u8>::ok(0);
  }
}   // namespace base::lex