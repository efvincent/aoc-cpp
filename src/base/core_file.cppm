module;
#include <stdio.h>

/**
 * @file core_file.cppm
 * @brief Arena-friendly file read/write utilities built on C stdio.
 */

/**
 * @module core_file
 * @brief File IO helpers returning explicit Result-based error states.
 */

export module core_file;

import core_types;
import core_string;
import core_memory;
import core_result;

/**
 * @namespace base
 * @brief Shared low-level file utilities.
 */
export namespace base {
  // ------------------------------------------------------------
  // File Diagnostics
  // ------------------------------------------------------------
  /**
   * @brief File operation failure categories returned by this module.
   */
  enum class FileError : u8 {
    None = 0,
    NotFoundOrLocked,
    OutOfMemory,
    ReadFailed,
    WriteFailed
  };

  /** @brief Result alias for file reads (error or byte slice). */
  using FileReadResult = Result<FileError, Str8>;
  /** @brief Result alias for file writes (error or bytes written). */
  using FileWriteResult = Result<FileError, u64>;

  // ------------------------------------------------------------
  // File Reading
  // ------------------------------------------------------------

  /**
   * @brief Reads an entire file into arena-backed memory.
   * @param arena Arena receiving file bytes.
   * @param filepath Null-terminated path to open.
   * @return Ok(Str8) on success, Err(FileError) on failure.
   */
  FileReadResult file_read_all(Arena& arena, const char* filepath) {
    // Open in binary mode to avoid newline translation.
    FILE* file = fopen(filepath, "rb");
    if (!file) {
      return FileReadResult::err(FileError::NotFoundOrLocked);
    }

    fseek(file, 0, SEEK_END);
    u64 file_size = static_cast<u64>(ftell(file));
    fseek(file, 0, SEEK_SET);

    // Empty file is a valid state, not an error
    if (file_size == 0) {
      fclose(file);
      return FileReadResult::ok(Str8{});
    }

    u8* buffer = arena.alloc_array<u8>(file_size);
    if (!buffer) {
      fclose(file);
      return FileReadResult::err(FileError::OutOfMemory);
    }

    u64 bytes_read = fread(buffer, 1, file_size, file);
    fclose(file);

    // Detect hard failure when no bytes were produced for a non-empty file.
    if (bytes_read == 0 && file_size > 0) {
      arena.offset -= file_size;  // reclaim memory
      return FileReadResult::err(FileError::ReadFailed);
    }

    return FileReadResult::ok(Str8(buffer, bytes_read));
  }

  // ------------------------------------------------------------
  // File Writing
  // ------------------------------------------------------------

  /**
   * @brief Writes all bytes from a Str8 into a file.
   * @param filepath Null-terminated path to open for writing.
   * @param data Byte slice to persist.
   * @return Ok(bytes_written) on success, Err(FileError) on failure.
   */
  FileWriteResult file_write_all(const char* filepath, Str8 data) {
    FILE* file = fopen(filepath, "wb");
    if (!file) {
      return FileWriteResult::err(FileError::NotFoundOrLocked);
    }

    if (data.len == 0) {
      fclose(file);
      return FileWriteResult::ok(0);
    }

    u64 bytes_written = fwrite(data.str, 1, data.len, file);
    fclose(file);

    if (bytes_written != data.len) {
      return FileWriteResult::err(FileError::WriteFailed);
    }

    return FileWriteResult::ok(bytes_written);
  }
}