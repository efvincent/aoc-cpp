export module core_lexer;

import core_types;
import core_string;
import core_result;
import core_text_parse;
import core_portability;

using namespace base;
using namespace str;
using namespace parse;

export namespace base::lex {
  template <typename T>
  using ParseResult = ParseResult<T>;

  /**
   * @defgroup LexerCore Zero-Allocation Lexer Primitives
   * @brief Token and cursor utilities for byte-slice lexing with no heap usage.
   *
   * This module keeps all lexical state in small POD-style structs and raw
   * `Str8` slices so higher layers can compose fast, data-oriented scanners.
   *
   * @par Error Accumulation Contract (Language Server Oriented)
   * The lexer layer is designed for whole-buffer parsing with recovery and
   * diagnostic accumulation:
   * - Cursor progress rule: a top-level lex step must either emit a token or
   *   advance by at least one byte.
   * - No silent consumed failure: if a routine consumes a candidate window and
   *   fails validation, that failure must be surfaced to the coordinator layer.
   * - Strict parsing remains delegated to text-parse helpers; cursor methods own
   *   navigation and consumed-window boundaries.
   * - Recovery behavior is intentional: some consume paths advance on failure to
   *   support forward progress in multi-error pipelines.
   */

  /**
   * @ingroup LexerCore
   * @brief Byte-sized token category used by higher-level parsers.
   */
  enum struct TokenType : u8 {
    // meta
    Eof = 0,
    Error,
    Unknown,      // the "escape valve" for unhandled symbols

    // Literals & data
    Identifier,
    String,
    Number,

    // Context and whitespace
    Whitespace,
    Comment,

    // Exact Punctuation (Zero-cost branch evaluations)
    Dot,
    Comma,
    Colon,
    Semicolon,
    OpenParen,
    CloseParen,
    OpenCurly,
    CloseCurly,
    OpenBracket,
    CloseBracket,
    AngleLeft,
    AngleRight,
    Plus,
    Minus,
    Asterisk,
    Slash,
    Backslash,        
    Percent,
    Caret,
    Ampersand,
    Equals,
    Pipe,
    Bang
  };

  /**
   * @ingroup LexerCore
   * @brief Borrowed token view into the source input.
   */
  struct Token {
    /** @brief Raw token bytes in the original buffer. */
    Str8 text;
    /** @brief Token classification for dispatch. */
    TokenType type;
  };

  /**
   * @brief Build an error diagnostic over a consumed source window. 
   */
  constexpr ParseDiagnostic make_error_diag(ParseDiagCode code, u64 start_offset, u64 end_offset) {
    return ParseDiagnostic{
      code,
      DiagnosticSeverity::Error,
      start_offset,
      end_offset
    };
  } 

  /**
   * @ingroup LexerCore
   * @brief Stateful cursor over a `Str8` input slice.
   *
   * The cursor is intentionally minimal: offset arithmetic plus inlined
   * byte traits and scanners that avoid dynamic allocation.
   */
  struct Str8Cursor {
    /** @brief Borrowed input buffer. */
    Str8 input;
    /** @brief Current byte offset in `input`. */
    u64 offset;

    // -- Core primitives --

    /**
     * @brief Read current or lookahead byte without advancing.
     * @param lookahead Byte distance ahead of `offset`.
     * @return Byte value or `0` when lookahead is out of bounds.
     */
    constexpr u8 peek(u64 lookahead = 0) {
      if (this->offset + lookahead >= this->input.len) {
        return 0; // null byte fallback prevents out-of-bounds reads
      }
      return this->input.str[this->offset + lookahead];
    }

    /**
     * @brief Advance cursor by `count` bytes and clamp at end-of-input.
     */
    constexpr void advance(u64 count = 1) {
      this->offset += count;
      if (this->offset > this->input.len) {
        this->offset = this->input.len;
      }
    }

    /** @brief Check whether cursor reached end-of-input. */
    constexpr bool is_eof() {
      return this->offset >= this->input.len;
    }

    // -- Inline character traits --

    /** @brief ASCII alphabetic predicate. */
    constexpr bool is_alpha(u8 c) {
      return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
    }

    /** @brief ASCII decimal digit predicate. */
    constexpr bool is_numeric(u8 c) {
      return (c >= '0' && c <= '9');
    }

    /** @brief ASCII alphanumeric predicate. */
    constexpr bool is_alphanumeric(u8 c) {
      return is_alpha(c) || is_numeric(c);
    }

    /** @brief Whitespace predicate for ASCII space controls. */
    constexpr bool is_whitespace(u8 c) {
      return c == ' ' || c == '\t' || c == '\n' || c == '\r';
    }

    // -- Zero-allocation scanners

    /**
     * @brief Skip contiguous whitespace bytes from current cursor position.
     */
    constexpr void skip_whitespace() {
      while (!is_eof() && is_whitespace(peek())) {
        this->offset++;
      }
    }

    /**
     * @brief Parse an identifier slice `[A-Za-z_][A-Za-z0-9_]*`.
     * @return Borrowed identifier slice, or empty slice if no identifier starts
     *         at the current position.
     */
    constexpr Str8 parse_ident() {
      // Guard against null-base pointer arithmetic for empty/null input slices
      if (this->input.str == nullptr || this->input.len == 0) {
        return Str8(nullptr, 0);
      }

      u64 start = this->offset;
      u8 first = peek();

      if (is_alpha(first) || first == '_') {
        advance();
        while (!is_eof()) {
          u8 c = peek();
          if (is_alphanumeric(c) || c == '_') {
            advance();
          } else {
            break;
          }
        }
      }
      return Str8(this->input.str + start, this->offset - start);
    }

    /**
     * @brief Parse a double-quoted string literal in-place.
     *
     * Assumes the cursor currently points at the opening quote. Escapes are
     * traversed without decoding (zero-allocation policy).
     *
     * @return `ok(length)` when a closing quote is found, where `length`
     *         includes both delimiters; otherwise
     *         `err(ParseDiagCode::UnterminatedString)` on EOF.
     * @note Diagnostic accumulation is coordinator-layer policy; see
     *       core_lexer_coord wrappers.
     */
    constexpr ParseResult<u64> parse_string_literal() {
      u64 start = this->offset;
      advance();  // advance past opening quote
      while (!is_eof()) {
        u8 c = peek();
        if (c == '"') {
          advance();  // consume closing quote
          return ParseResult<u64>::ok(this->offset - start);
        }
        if (c == '\\') {
          // blind skip - bypass the backslash and the escaped character
          advance(2);
        } else {
          advance(1);
        }
      }
      return ParseResult<u64>::err(ParseDiagCode::UnterminatedString);
    }

    /**
     * @brief Consume an integer prefix and parse it through a strict parser backend.
     *
     * Unsigned targets consume `[0-9]+`. Signed targets consume `[+|-]?[0-9]+`.
     * The consumed window is then delegated to a strict `base::parse::parse_*`
     * function.
     *
     * Failure semantics:
     * - If no valid integer prefix exists at the current offset, this returns error
     *   and does not advance the cursor.
     * - If a candidate prefix is consumed and strict parsing fails (for example,
     *   overflow), the cursor remains advanced to the end of that consumed prefix.
     *   This is intentional for forward progress and error-accumulation workflows.
     *
     * @tparam T Integer output type.
     * @param parser Strict parser callback (`ParseResult<T> (*)(Str8)`).
     */
    template <typename T>
    constexpr ParseResult<T> consume_int_impl(ParseResult<T> (*parser)(Str8)) {
      u64 start = this->offset;
      u64 scan = start;

      if constexpr (portability::is_signed_v<T>) {
        if (scan < this->input.len && (this->input.str[scan] == '+' || this->input.str[scan] == '-')) {
          scan++;
        }
      }

      u64 digits_start = scan;
      while (scan < this->input.len && is_numeric(this->input.str[scan])) {
        scan++;
      }

      if (scan == digits_start) {
        return ParseResult<T>::err(ParseDiagCode::NoDigits);
      }

      this->offset = scan;
      Str8 consumed{this->input.str + start, scan - start};
      return parser(consumed);
    }

    /**
     * @brief Consume `[0-9]+` and parse as `u8`.
     *
     * This method performs prefix window discovery and delegates numeric
     * validation/range checks to strict `base::parse::parse_u8`.
     */
    constexpr ParseResult<u8> consume_u8() {
      return consume_int_impl<u8>(parse_u8);
    }

    /**
     * @brief Consume `[+|-]?[0-9]+` and parse as `i8`.
     */
    constexpr ParseResult<i8> consume_i8() {
      return consume_int_impl<i8>(parse_i8);
    }

    /** @brief Consume `[0-9]+` and parse as `u16`. */
    constexpr ParseResult<u16> consume_u16() {
      return consume_int_impl<u16>(parse_u16);
    }

    /** @brief Consume `[+|-]?[0-9]+` and parse as `i16`. */
    constexpr ParseResult<i16> consume_i16() {
      return consume_int_impl<i16>(parse_i16);
    }

    /** @brief Consume `[0-9]+` and parse as `u32`. */
    constexpr ParseResult<u32> consume_u32() {
      return consume_int_impl<u32>(parse_u32);
    }

    /** @brief Consume `[+|-]?[0-9]+` and parse as `i32`. */
    constexpr ParseResult<i32> consume_i32() {
      return consume_int_impl<i32>(parse_i32);
    }

    /** @brief Consume `[0-9]+` and parse as `u64`. */
    constexpr ParseResult<u64> consume_u64() {
      return consume_int_impl<u64>(parse_u64);
    }

    /** @brief Consume `[+|-]?[0-9]+` and parse as `i64`. */
    constexpr ParseResult<i64> consume_i64() {
      return consume_int_impl<i64>(parse_i64);
    }

    /**
     * @brief Consume `[+|-]?([0-9]+(\.[0-9]*)?|\.[0-9]+)([eE][+|-]?[0-9]+)?` and parse as `f64`.
     *
     * Failure semantics:
     * - If no valid floating prefix exists at the current offset, this returns error
     *   and does not advance the cursor.
     * - If a syntactically valid candidate prefix is consumed, strict parse
     *   validation is applied to that slice. If strict parsing fails, the cursor
     *   remains advanced to the end of the consumed prefix (no rewind).
     *
     * This behavior is intentional for recovery-friendly, whole-buffer parsing
     * pipelines such as language-server style error accumulation.
     */
    constexpr ParseResult<f64> consume_f64() {
      u64 start = this->offset;
      u64 scan = start;

      if (scan < this->input.len && (this->input.str[scan] == '+' || this->input.str[scan] == '-')) {
        scan++;
      }

      bool has_digits = false;
      while (scan < this->input.len && is_numeric(this->input.str[scan])) {
        has_digits = true;
        scan++;
      }

      if (scan < this->input.len && this->input.str[scan] == '.') {
        scan++;
        while (scan < this->input.len && is_numeric(this->input.str[scan])) {
          has_digits = true;
          scan++;
        }
      }

      if (!has_digits) {
        return ParseResult<f64>::err(ParseDiagCode::NoDigits);
      }

      if (scan < this->input.len && (this->input.str[scan] == 'e' || this->input.str[scan] == 'E')) {
        u64 exp_scan = scan + 1;
        if (exp_scan < this->input.len && (this->input.str[exp_scan] == '+' || this->input.str[exp_scan] == '-')) {
          exp_scan++;
        }
        u64 exp_digits_start = exp_scan;
        while (exp_scan < this->input.len && is_numeric(this->input.str[exp_scan])) {
          exp_scan++;
        }

        // If exponent marker has no digits, do not consume it here.
        // Leave 'e'/'E' for subsequent tokenization and consume mantissa only.
        if (exp_scan != exp_digits_start) {
          scan = exp_scan;
        }
      }

      this->offset = scan;
      Str8 consumed{this->input.str + start, scan - start};
      return parse_f64(consumed);
    }

    /** @brief Consume and parse an `f32` prefix via `consume_f64`. */
    constexpr ParseResult<f32> consume_f32() {
      auto res = consume_f64();
      if (!res.is_ok()) {
        return ParseResult<f32>::err(res.error);
      }
      return ParseResult<f32>::ok(static_cast<f32>(res.value));
    }

  };    // struct Str8Cursor

  /**
   * @brief Language-specific trivia markers for whitespace/comment skipping.
   */
  struct TriviaConfig {
    /** @brief Line comment introducer (for example `//` or `#`). */
    Str8 line_mark;
    /** @brief Block comment opener (for example slash-star). */
    Str8 block_start;
    /** @brief Block comment closer (for example star-slash). */
    Str8 block_end;
    /** @brief Enable nested block comment depth handling. */
    bool nested;
  };

  /**
   * @brief Check whether cursor input starts with `mark` at current offset.
   *
   * This helper is read-only and does not advance the cursor. Taking
   * `const Str8Cursor&` makes that guarantee explicit for callers.
   */
  constexpr bool cursor_starts_with(const Str8Cursor& cursor, Str8 mark) {
    if (mark.len == 0 || cursor.offset + mark.len > cursor.input.len) {
      return false;
    }
    for (u64 i = 0; i < mark.len; ++i) {
      if (cursor.input.str[cursor.offset + i] != mark.str[i]) {
        return false;
      }
    }
    return true;
  }

  /**
   * @brief Skip whitespace and comments according to a trivia configuration.
   *
   * This primitive only performs cursor movement and does not emit diagnostics.
   * Coordinator-layer wrappers own unterminated-comment reporting policy.
   *
   * This function advances `cursor.offset` in place and can handle nested block
   * comments when `config.nested` is enabled.
   */
  constexpr void skip_trivia(Str8Cursor& cursor, TriviaConfig config) {
    while (!cursor.is_eof()) {

      // skip standard whitespace
      if (cursor.is_whitespace(cursor.peek())) {
        cursor.offset++;
        continue; // outer while
      }

      // skip line comments
      if  (config.line_mark.len > 0 && cursor_starts_with(cursor, config.line_mark)) {
        while (!cursor.is_eof() && cursor.peek() != '\n') {
          cursor.offset++;
        }
        continue; // outer while
      }

      // skip block comments
      if (config.block_start.len > 0 && cursor_starts_with(cursor, config.block_start)) {
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
        continue; // outer while
      }
      break;  // outer while
    }
  }
}     // namespace base::lex