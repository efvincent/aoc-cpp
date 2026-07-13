module;
#include <stdarg.h>
#include <string.h>
#include <stdio.h>
#include "core_debug.h"

/**
 * @file core_string.cppm
 * @brief UTF-8 byte-string views, list utilities, formatting, and hashing.
 */

/**
 * @module core_string
 * @brief String primitives designed for arena-backed allocation workflows.
 */
export module core_string;

import core_types;
import core_memory;
import core_result;

using namespace base;
using namespace mem;

/**
 * @namespace base::str
 * @brief String and formatting primitives for the base layer.
 */
export namespace base::str {

  /**
   * @brief Failure categories for arena-backed string formatting.
   */  
  enum struct StringFormatError : u8 {
    None = 0,
    FormatFailed,
    OutOfMemory,
    Truncated
  };

  struct Str8;
  using StringFormatResult = Result<StringFormatError, Str8>;

  /**
   * @brief Non-owning UTF-8 byte-string view.
   * @note The view is not null-terminated by contract.
   * @note Lifetime is owned externally, commonly by Arena.
   */
  struct Str8 {
    /** @brief Pointer to first byte of the string view. */
    u8* str;
    /** @brief Number of bytes in the view. */
    u64 len;


    /** @brief Default empty string view constructor. */
    constexpr Str8() {
      this->str = nullptr;
      this->len = 0;
    }

    /**
     * @brief Constructs a string view from raw pointer and explicit length.
     * @param target_str Pointer to first byte.
     * @param target_len Length in bytes.
     * @post The instance references the provided memory without owning it.
     */
    constexpr Str8(u8* target_str, u64 target_len) {
      this->str = target_str;
      this->len = target_len;
    }

    /**
     * @brief Creates a Str8 view from a null-terminated C-string.
     * @warning This performs an O(N) strlen calculation.
     */
    static constexpr Str8 from_cstr(const char* cstr) {
      if (!cstr) return Str8{};
      return Str8(reinterpret_cast<u8*>(const_cast<char*>(cstr)), static_cast<u64>(strlen(cstr)));
    }

    /**
     * @brief Constructs a view from a string literal, excluding trailing null.
     * @tparam N Literal array length including null terminator.
     * @post len is N - 1.
     */
    template<u64 N>
    constexpr Str8(const char (&literal)[N]) {
      this->str = reinterpret_cast<u8*>(const_cast<char*>(literal));
      this->len = N - 1;    // Omit the null terminator
    }

    /**
     * @brief Constructs a null terminated c-string from a Str8
     * 
     * @param arena Arena from which to allocate new memory for the cstr 
     * @return const char* A null terminated C string
     */
    const char* to_cstr(Arena& arena) const {
      BASE_ASSERT(this->str != nullptr);
      auto buffer = arena.alloc_array<char>(this->len+1);
      BASE_ASSERT(buffer != nullptr);

      memcpy(buffer, this->str, this->len);
      buffer[this->len] = 0;
      return buffer;
    }

    /**
     * @brief Returns a sub-view from [start, end).
     * @param start Inclusive starting byte offset.
     * @param end Exclusive ending byte offset.
     * @return A Str8 view into the original storage.
     * @pre start <= end.
     * @pre end <= len.
     */
    constexpr Str8 slice(u64 start, u64 end) const {
      // Dev-time check using universal macro
      BASE_ASSERT(start <= end);
      BASE_ASSERT(end <= this->len);
      return Str8(this->str + start, end - start);
    }

    /**
    * @brief Byte-wise equality comparison between two Str8 views.
    * @param other Comparison target string view.
    * @return True if lengths and bytes are equal.
    */
    constexpr bool match(Str8 other) const {
      if (this->len != other.len) return false;
      for (u64 i = 0; i < this->len; i++) {
        if (this->str[i] != other.str[i]) return false;
      }
      return true;
    }

    /**
    * @brief Computes the 64-bit FNV-1a hash of this string view.
    * @return 64-bit hash value.
    * @note Hash is stable for identical byte sequences.
    */
    constexpr u64 hash_fnv1a() const {
      u64 hash = 14695981039346656037ULL;
      for (u64 i = 0; i < this->len; ++i) {
          hash ^= this->str[i];
          hash *= 1099511628211ULL;
      }
      return hash;
    }
  };

  /**
   * @brief Extracts the next delimiter-separated segment from a string view.
   * @param remaining_str In/out remaining source view; advances past consumed bytes.
   * @param out_slice Output slice for the next segment.
   * @param delim Delimiter byte used to split segments.
   * @return True when a segment is produced; false when input is exhausted.
   */
  bool iter_next(Str8 *remaining_str, Str8 *out_slice, char delim) {
    BASE_ASSERT(remaining_str != nullptr);
    BASE_ASSERT(out_slice != nullptr);

    if (remaining_str->len == 0) {
      return false; // Exhausted
    }

    u8 *start = remaining_str->str;
    u8 *next = reinterpret_cast<u8*>(memchr(start, delim, remaining_str->len));
 
    if (!next) {
      // No more delimiters; return the rest of the string and exhaust the view
      *out_slice = *remaining_str;
      remaining_str->len = 0;
      return true;
    }

    out_slice->str = start;
    out_slice->len = (u64)(next - start);

    // Advance the remaining view past the slice and the delimiter
    u64 consumed = out_slice->len + 1;
    remaining_str->str += consumed;
    remaining_str->len -= consumed;

    return true;
  }

  //-------------------------------------------------------------
  // Str8 List
  //-------------------------------------------------------------

  /** @brief Singly linked node containing one Str8 segment. */
  struct Str8Node {
    /** @brief Next node in list, or null at tail. */
    Str8Node* next;
    /** @brief Segment value stored at this node. */
    Str8 string;
  };

  /**
   * @brief Builder structure for concatenating many Str8 segments efficiently.
   * @note Nodes are expected to come from a scratch Arena.
   */
  struct Str8List {
    /** @brief First node in list. */
    Str8Node* first;
    /** @brief Last node in list. */
    Str8Node* last;
    /** @brief Number of nodes pushed into the list. */
    u64 node_count;
    /** @brief Sum of all segment lengths (in bytes). */
    u64 total_len;

    /**
     * @brief Appends a non-empty segment to the list.
     * @param scratch_arena Arena used for node allocation.
     * @param string Segment to append.
     * @post node_count increments when string is non-empty.
     */
    void push(Arena& scratch_arena, Str8 string) {
      if (string.len == 0) return;
      Str8Node* node = scratch_arena.alloc_struct<Str8Node>();
      BASE_ASSERT(node != nullptr);

      node->string = string;
      node->next = nullptr;

      if (this->last == nullptr) {
        this->first = node;
        this->last = node;
      } else {
        this->last->next = node;
        this->last = node;
      }

      this->node_count += 1;
      this->total_len += string.len;
    }

    /**
    * @brief Concatenates all segments in a Str8List into one contiguous Str8.
    * @param arena Arena used for destination allocation.
    * @return Joined string view; empty view if input is empty.
    * @post Returned memory is owned by \p arena.
    */
    Str8 join(Arena& arena) {
      if (this->total_len == 0) return Str8{};

      // One single allocation for the entire compound string
      u8* buffer = arena.alloc_array<u8>(this->total_len);
      BASE_ASSERT(buffer != nullptr);

      u64 write_offset = 0;
      for (Str8Node* node = this->first; node != nullptr; node = node->next) {
        if (node->string.len > 0) {
          memcpy(buffer + write_offset, node->string.str, node->string.len);
          write_offset += node->string.len;
        }
      }
      return Str8(buffer, this->total_len);
    }
  };

  //-------------------------------------------------------------
  // Formatting into Str8
  //-------------------------------------------------------------

  /**
   * @brief Formats text into arena memory using exact two-pass sizing.
   * @param arena Arena used to allocate destination bytes.
   * @param format Printf-style format string.
   * @param args Active vararg list.
   * @return Ok(Str8) on success, or Err(StringFormatError) on failure.
   */
  StringFormatResult vformat(Arena& arena, const char* format, va_list args) {
    va_list count_args;
    va_copy(count_args, args);
    i32 formal_len = vsnprintf(nullptr, 0, format, count_args);
    va_end(count_args);

    if (formal_len < 0) {
      return StringFormatResult::err(StringFormatError::FormatFailed);
    }

    u64 safe_len = static_cast<u64>(formal_len);
    if (safe_len == 0) {
      return StringFormatResult::ok(Str8{});
    }

    u64 snapshot = arena.offset;
    u8* buffer = arena.alloc_array<u8>(safe_len + 1); // +1: null terminator safety
    if (!buffer) {
      return StringFormatResult::err(StringFormatError::OutOfMemory);
    }

    va_list write_args;
    va_copy(write_args, args);
    i32 written_len = vsnprintf(reinterpret_cast<char*>(buffer), static_cast<size_t>(safe_len + 1), format, write_args);
    va_end(write_args);

    if (written_len < 0 || static_cast<u64>(written_len) != safe_len) {
      arena.offset = snapshot;
      return StringFormatResult::err(StringFormatError::FormatFailed);
    }

    return StringFormatResult::ok(Str8{buffer, safe_len});
  }


  /**
   * @brief Formats text into arena memory with caller-provided capacity cap.
   * @param arena Arena used to allocate destination bytes.
   * @param cap Maximum payload bytes (excluding null terminator safety byte).
   * @param format Printf-style format string.
   * @param args Active vararg list.
   * @return Ok(Str8) on success, or Err(StringFormatError) for format failure,
   *         out-of-memory, or truncation.
   */
  StringFormatResult vformat_cap(Arena& arena, u64 cap, const char* format, va_list args) {
    if (cap == 0) {
      return StringFormatResult::ok(Str8{});
    }

    u64 snapshot = arena.offset;
    u8* buffer = arena.alloc_array<u8>(cap + 1); // +1 for null terminator safety
    if (!buffer) {
      return StringFormatResult::err(StringFormatError::OutOfMemory);
    }

    va_list write_args;
    va_copy(write_args, args);
    i32 written_len = vsnprintf(reinterpret_cast<char*>(buffer), static_cast<size_t>(cap + 1), format, write_args);
    va_end(write_args);

    if (written_len < 0) {
      arena.offset = snapshot;
      return StringFormatResult::err(StringFormatError::FormatFailed);
    }

    u64 written_u64 = static_cast<u64>(written_len);

    // vsnprintf returns full required length on truncation.
    if (written_u64 > cap) {
      arena.offset = snapshot;
      return StringFormatResult::err(StringFormatError::Truncated);
    }

    if (written_u64 == 0) {
      arena.offset = snapshot;
      return StringFormatResult::ok(Str8{});
    }

    arena.offset = snapshot + written_u64; // trim to exact payload bytes
    return StringFormatResult::ok(Str8{buffer, written_u64});
  }

  /**
   * @brief Formats text into arena memory using exact two-pass sizing.
   * @param arena Arena used to allocate destination bytes.
   * @param format Printf-style format string.
   * @param ... Format arguments.
   * @return Ok(Str8) on success, or Err(StringFormatError) on failure.
   */
  StringFormatResult format(Arena& arena, const char* format, ...) {
    va_list args;
    va_start(args, format);
    StringFormatResult result = vformat(arena, format, args);
    va_end(args);
    return result;
  }

  /**
   * @brief Formats text into arena memory using caller-provided capacity cap.
   * @param arena Arena used to allocate destination bytes.
   * @param cap Maximum payload bytes (excluding null terminator safety byte).
   * @param format Printf-style format string.
   * @param ... Format arguments.
   * @return Ok(Str8) on success, or Err(StringFormatError) on failure.
   */
  StringFormatResult format_cap(Arena& arena, u64 cap, const char* format, ...) {
    va_list args;
    va_start(args, format);
    StringFormatResult result = vformat_cap(arena, cap, format, args);
    va_end(args);
    return result;
  }

}   // namespace base::str

export namespace base {
  using Str8 = str::Str8;
}