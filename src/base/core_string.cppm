module;
#include <cstdarg>
#include <cstring>
#include <stdio.h>

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
// import core_result;

/**
 * @namespace base
 * @brief Shared low-level string and memory utilities.
 */
export namespace base {
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
     * @brief Returns a sub-view from [start, end).
     * @param start Inclusive starting byte offset.
     * @param end Exclusive ending byte offset.
     * @return A Str8 view into the original storage.
      * @pre start <= end.
      * @pre end <= len.
     */
    constexpr Str8 slice(u64 start, u64 end) const {
      // Dev-time check using universal macro
      #include "core_debug.h"
      BASE_ASSERT(start <= end);
      BASE_ASSERT(end <= this->len);
      return Str8(this->str + start, end - start);
    }
  };

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
    void push(Arena* scratch_arena, Str8 string) {
      if (string.len == 0) return;
      Str8Node* node = scratch_arena->alloc_struct<Str8Node>();
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
  };

  /**
   * @brief Concatenates all segments in a Str8List into one contiguous Str8.
   * @param permanent_arena Arena used for destination allocation.
   * @param list Input linked list of segments.
   * @return Joined string view; empty view if input is empty.
    * @post Returned memory is owned by \p permanent_arena.
   */
  Str8 str8_list_join(Arena* permanent_arena, Str8List list) {
    if (list.total_len == 0) return Str8{};

    // One single allocation for the entire compound string
    u8* buffer = permanent_arena->alloc_array<u8>(list.total_len);
    BASE_ASSERT(buffer != nullptr);

    u64 write_offset = 0;
    for (Str8Node* node = list.first; node != nullptr; node = node->next) {
      if (node->string.len > 0) {
        memcpy(buffer + write_offset, node->string.str, node->string.len);
        write_offset += node->string.len;
      }
    }
    return Str8(buffer, list.total_len);
  }

  /**
   * @brief Appends formatted text into arena memory and returns it as Str8.
   * @param arena Arena used to allocate destination bytes.
   * @param format Printf-style format string.
   * @param ... Format arguments.
   * @return Formatted string view, excluding trailing null terminator.
    * @post Returned memory is owned by \p arena.
   */
  Str8 str8_pushf(Arena* arena, const char* format, ...) {
    va_list args;

    // Query exact space required by running a dry run
    va_start(args,format);
    u32 formal_len = vsnprintf(nullptr, 0, format, args);
    va_end(args);

    if (formal_len <= 0) return Str8{};
    u64 safe_len = static_cast<u64>(formal_len);

    // Allocate the exact destination block right out of the arena
    u8* buffer = arena->alloc_array<u8>(safe_len + 1);  // +1 for the 
                                                        // null terminator safety
                                                        // inside C functions
    BASE_ASSERT(buffer != nullptr);
    
    // Format directly into the freshly bumped arena memory segment
    va_start(args, format);
    vsnprintf(reinterpret_cast<char*>(buffer), safe_len + 1, format, args);
    va_end(args);

    // Return the view tracking the explicit characters, ommitting the
    // trailing null byte
    return Str8(buffer, safe_len);
  }

  /**
   * @brief Byte-wise equality comparison between two Str8 views.
   * @param a First string view.
   * @param b Second string view.
   * @return True if lengths and bytes are equal.
   */
  constexpr bool str8_match(Str8 a, Str8 b) {
    if (a.len != b.len) return false;
    for (u64 i = 0; i < a.len; i++) {
      if (a.str[i] != b.str[i]) return false;
    }
    return true;
  }

  /**
   * @brief Computes the 64-bit FNV-1a hash of a string view.
   * @param string Input string view.
   * @return 64-bit hash value.
    * @note Hash is stable for identical byte sequences.
   */
  constexpr base::u64 str8_hash_fnv1a(Str8 string) {
    base::u64 hash = 14695981039346656037ULL;
    for (base::u64 i = 0; i < string.len; ++i) {
        hash ^= string.str[i];
        hash *= 1099511628211ULL;
    }
    return hash;
  }
}