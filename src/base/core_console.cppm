module;
#include <stdio.h>

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

/**
 * @namespace base
 * @brief Shared low-level console utilities.
 */
export namespace base {

  // ==========================================================================
  // Output
  // ==========================================================================

  /**
   * @brief Writes raw text to stdout without appending a newline.
   * @param text UTF-8 byte slice to write.
   */
  inline void print(Str8 text) {
    if (text.len > 0) { 
      fwrite(text.str, 1, text.len, stdout);
    }
  }

  /**
   * @brief Writes text to stdout followed by a newline.
   * @param text UTF-8 byte slice to write.
   */
  inline void printl(Str8 text) {
    print(text);
    fwrite("\n", 1, 1, stdout);
    fflush(stdout);
  }

  /**
   * @brief Flushes stdout explicitly.
   * @note Useful after partial-line output written with print().
   */
  inline void flush_console() {
    fflush(stdout);
  }

  /**
   * @brief Writes text to stderr and always appends a newline.
   * @param text UTF-8 byte slice to write.
   */
  inline void print_err(Str8 text) {
    if (text.len > 0) {
      fwrite(text.str, 1, text.len, stderr);
    }
    fwrite("\n", 1, 1, stderr);
  }

  // ==========================================================================
  // Input
  // ==========================================================================

  /**
   * @brief Reads one line from stdin into arena-backed storage.
   * @param arena Destination arena used for temporary line storage.
   * @param max_bytes Maximum bytes to reserve/read including terminating null from fgets.
   * @return Line bytes excluding trailing newline; empty Str8 on EOF or allocation/input failure.
   */
  inline Str8 read_line(Arena& arena, u64 max_bytes = 1024) {
    u8* buffer = arena.alloc_array<u8>(max_bytes);
    if (!buffer) return Str8{};

    if (fgets(reinterpret_cast<char*>(buffer), static_cast<int>(max_bytes), stdin) == nullptr) {
      arena.offset -= max_bytes;
      return Str8{};
    }

    u64 len = 0;
    while (len < max_bytes && buffer[len] != 0 && buffer[len] != '\n') {
      len++;
    }

    arena.offset -= (max_bytes - len);
    return Str8(buffer, len);
  }

}
