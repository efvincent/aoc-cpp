module;
#include <stdio.h>
#include <stdarg.h>

/**
 * @file core_console.cppm
 * @brief Console input/output helpers backed by C stdio primitives.
 */

/**
 * @module core_console
 * @brief Lightweight terminal read/write wrappers using Str8 and Arena.
 */

export module core_console;

import core_types;
import core_string;
import core_memory;
import core_result;

using namespace base;
using namespace str;

/**
 * @namespace base::console
 * @brief Low-level console IO utilities.
 */
export namespace base::console {
  /**
   * @brief Console input failure categories for line-oriented reads.
   */
  enum struct ConsoleError : u8 {
    None = 0,
    OutOfMemory,
    EndOfInput,
    ReadFailed,
    LineTooLong
  };

  /**
   * @brief Console output failure categories.
   */
  enum struct ConsoleWriteError : u8 {
    None = 0,
    FormatFailed,
    OutOfMemory,
    Truncated,
    StdoutWriteFailed,
    StderrWriteFailed,
    FlushFailed
  };

  /** @brief Result alias for console write/format operations. */
  using ConsoleWriteResult = Result<ConsoleWriteError, u64>;

  /** @brief Result alias for console line reads. */
  using ConsoleReadResult = Result<ConsoleError, Str8>;
  
  // ==========================================================================
  // Output
  // ==========================================================================

    /**
   * @brief Maps string formatting errors to console write errors.
   */
  constexpr ConsoleWriteError map_string_error_to_console(StringFormatError e) {
    using enum StringFormatError;
    switch(e) {
      case FormatFailed: return ConsoleWriteError::FormatFailed;
      case OutOfMemory:  return ConsoleWriteError::OutOfMemory;
      case Truncated:    return ConsoleWriteError::Truncated;  
      default:           return ConsoleWriteError::FormatFailed;
    }
  }

  /**
   * @brief Writes bytes to stdout with result checking.
   * @param text Bytes to write.
   * @return Ok(bytes_written) or Err(ConsoleWriteError::StdoutWriteFailed).
   */
  inline ConsoleWriteResult write_stdout(Str8 text) {
    if (text.len == 0) return ConsoleWriteResult::ok(0);
    u64 written = static_cast<u64>(fwrite(text.str, 1, text.len, stdout));
    if (written != text.len) {
      return ConsoleWriteResult::err(ConsoleWriteError::StdoutWriteFailed);
    }
    return ConsoleWriteResult::ok(written);
  }

  /**
   * @brief Writes bytes to stderr with result checking.
   * @param text Bytes to write.
   * @return Ok(bytes_written) or Err(ConsoleWriteError::StderrWriteFailed).
   */
  inline ConsoleWriteResult write_stderr(Str8 text) {
    if (text.len == 0) return ConsoleWriteResult::ok(0);
    u64 written = static_cast<u64>(fwrite(text.str, 1, text.len, stderr));
    if (written != text.len) {
      return ConsoleWriteResult::err(ConsoleWriteError::StderrWriteFailed);
    }
    return ConsoleWriteResult::ok(written);
  }

  /**
   * @brief Flushes stdout with result checking.
   * @return Ok(0) on success or Err(ConsoleWriteError::FlushFailed).
   */
  inline ConsoleWriteResult flush_stdout_checked() {
    if (fflush(stdout) != 0) {
      return ConsoleWriteResult::err(ConsoleWriteError::FlushFailed);
    }
    return ConsoleWriteResult::ok(0);
  }

  /**
   * @brief Formats and writes one line to stdout (exact two-pass formatting).
   * @param arena Arena used by formatting.
   * @param format Printf-style format string.
   * @param ... Format arguments.
   * @return Ok(total_bytes_written) or Err(ConsoleWriteError).
   */
  inline ConsoleWriteResult printfl(mem::Arena& arena, const char* format, ...) {
    va_list args;
    va_start(args, format);
    StringFormatResult fmt = vpushf(arena, format, args);
    va_end(args);

    if (!fmt.is_ok()) {
      return ConsoleWriteResult::err(map_string_error_to_console(fmt.error));
    }

    auto w = write_stdout(fmt.value);
    if (!w.is_ok()) return ConsoleWriteResult::err(w.error);

    auto n = write_stdout(Str8("\n"));
    if (!n.is_ok()) return ConsoleWriteResult::err(n.error);

    auto f = flush_stdout_checked();
    if (!f.is_ok()) return ConsoleWriteResult::err(f.error);

    return ConsoleWriteResult::ok(fmt.value.len + 1);
  }

  /**
   * @brief Formats and writes one line to stdout (single-pass capped formatting).
   * @param arena Arena used by formatting.
   * @param cap Maximum formatted payload bytes.
   * @param format Printf-style format string.
   * @param ... Format arguments.
   * @return Ok(total_bytes_written) or Err(ConsoleWriteError).
   */
  inline ConsoleWriteResult printfl_cap(mem::Arena& arena, u64 cap, const char* format, ...) {
    va_list args;
    va_start(args, format);
    StringFormatResult fmt = vpush_cap(arena, cap, format, args);
    va_end(args);

    if (!fmt.is_ok()) {
      return ConsoleWriteResult::err(map_string_error_to_console(fmt.error));
    }

    auto w = write_stdout(fmt.value);
    if (!w.is_ok()) return ConsoleWriteResult::err(w.error);

    auto n = write_stdout(Str8("\n"));
    if (!n.is_ok()) return ConsoleWriteResult::err(n.error);

    auto f = flush_stdout_checked();
    if (!f.is_ok()) return ConsoleWriteResult::err(f.error);

    return ConsoleWriteResult::ok(fmt.value.len + 1);
  }

  /**
   * @brief Formats and writes one line to stderr (single-pass capped formatting).
   * @param arena Arena used by formatting.
   * @param cap Maximum formatted payload bytes.
   * @param format Printf-style format string.
   * @param ... Format arguments.
   * @return Ok(total_bytes_written) or Err(ConsoleWriteError).
   */
  inline ConsoleWriteResult eprintfl_cap(mem::Arena& arena, u64 cap, const char* format, ...) {
    va_list args;
    va_start(args, format);
    StringFormatResult fmt = vpush_cap(arena, cap, format, args);
    va_end(args);

    if (!fmt.is_ok()) {
      return ConsoleWriteResult::err(map_string_error_to_console(fmt.error));
    }

    auto w = write_stderr(fmt.value);
    if (!w.is_ok()) return ConsoleWriteResult::err(w.error);

    auto n = write_stderr(Str8("\n"));
    if (!n.is_ok()) return ConsoleWriteResult::err(n.error);

    return ConsoleWriteResult::ok(fmt.value.len + 1);
  }

  /**
   * @brief Writes raw text to stdout without appending a newline.
   * @param text UTF-8 byte slice to write.
   */
  inline void print(Str8 text) {
    (void)write_stdout(text);
  }

  /**
   * @brief Writes text to stdout followed by a newline.
   * @param text UTF-8 byte slice to write.
   */
  inline void printl(Str8 text) {
    auto _w = write_stdout(text);
    if (!_w.is_ok()) return;
    auto _n = write_stdout(Str8("\n"));
    if (!_n.is_ok()) return;
    (void)flush_stdout_checked();
  }

  /**
   * @brief Flushes stdout explicitly.
   * @note Useful after partial-line output written with print().
   */
  inline void flush_console() {
    (void)flush_stdout_checked();
  }

  /**
   * @brief Writes text to stderr and always appends a newline.
   * @param text UTF-8 byte slice to write.
   */
  inline void print_err(base::Str8 text) {
    (void)write_stderr(text);
    (void)write_stderr(base::Str8("\n"));
  }

  // ==========================================================================
  // Input
  // ==========================================================================

  /**
   * @brief Reads one line from stdin into arena-backed storage.
   * @param arena Destination arena used for temporary line storage.
   * @param max_bytes Maximum bytes to reserve/read including terminating null from fgets.
   * @return Ok(Str8) with the line excluding trailing newline, or Err(ConsoleError)
   *         for EOF, allocation failure, input failure, or truncation.
   * @note An empty line is returned as Ok(Str8{}).
   */
  inline ConsoleReadResult read_line(base::mem::Arena& arena, u64 max_bytes = 1024) {
    using enum ConsoleError;
    u64 snapshot = arena.offset;

    u8* buffer = arena.alloc_array<u8>(max_bytes);
    if (!buffer) {
      return ConsoleReadResult::err(OutOfMemory);
    }

    if (fgets(reinterpret_cast<char*>(buffer), static_cast<int>(max_bytes), stdin) == nullptr) {
      arena.offset = snapshot;
      if (feof(stdin)) {
        return ConsoleReadResult::err(EndOfInput);
      }
      return ConsoleReadResult::err(ReadFailed);
    }

    u64 len = 0;
    bool found_newline = false;
    bool found_null = false;

    while (len < max_bytes) {
      if (buffer[len] == '\n') {
        found_newline = true;
        break;
      }
      if (buffer[len] == 0) {
        found_null = true;
        break;
      }
      len++;
    }

    if (len == 0) {
      arena.offset = snapshot;
      return ConsoleReadResult::ok(base::Str8{});
    }

    if (!found_newline && !found_null && len == max_bytes - 1 && !feof(stdin)) {
      arena.offset = snapshot;
      return ConsoleReadResult::err(LineTooLong);
    }

    arena.offset -= (max_bytes - len);
    return ConsoleReadResult::ok(base::Str8(buffer, len));
  }

}   // namespace base::console
