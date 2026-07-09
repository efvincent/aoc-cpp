module;

#include "core_debug.h"

/**
 * @file core_memory.cppm
 * @brief Arena allocator primitives backed by virtual memory mappings.
 */

/**
 * @module core_memory
 * @brief Linear arena allocation helpers for transient data.
 */
export module core_memory;

import core_types;
import core_portability;
import core_vm;

using namespace base;

/**
 * @namespace base::mem
 * @brief Arena allocation primitives for transient data.
 */
export namespace base::mem {

  /**
   * @brief Linear allocator using a single contiguous mapped memory region.
    * @note Supports bump allocation only; individual frees are not available.
   */
  struct Arena {
    /** @brief Base pointer to mapped memory; null when uninitialized/freed. */
    u8* buffer;
    /** @brief Total mapped capacity in bytes. */
    u64 capacity;
    /** @brief Current allocation cursor measured in bytes from buffer. */
    u64 offset;

    /** @brief Alignment target for transparent huge page friendly mapping. */
    static constexpr u64 THP_ALIGNMENT = (2zu * 1024zu * 1024zu);

    /** @brief Default constructs a zero-initialized arena handle. */
    inline Arena() = default;

    /**
     * @brief Initializes the arena by mapping a writable virtual memory region.
     * @param requested_capacity Minimum capacity requested in bytes.
     * @return True on success; false if the mapping fails.
      * @post On success, buffer is non-null and capacity is >= requested_capacity.
      * @note Capacity is rounded up to THP_ALIGNMENT.
     */
    inline bool init(u64 requested_capacity) {
      // round capacity up to nearest 2MB boundary for the THP daemon
      u64 total_capacity = align_forward(requested_capacity, this->THP_ALIGNMENT);

      // request writable virtual memory through the platform VM backend
      void* ptr = vm::alloc(total_capacity);
      if (!ptr) {
        return false;
      }
  
      vm::hint_hugepages(ptr, total_capacity);

      this->buffer = reinterpret_cast<u8*>(ptr);
      this->capacity = total_capacity;
      this->offset = 0;
      return true;
    }

    /**
     * @brief Allocates storage for an array of \p count objects of type \p T.
     * @tparam T Element type.
     * @param count Number of elements to reserve.
     * @return Pointer to array storage, or nullptr on out-of-memory.
      * @pre init() has succeeded.
     */
    template <typename T>
    T* alloc_array(u64 count) {
      BASE_ASSERT(this->buffer);
      u64 total_bytes = sizeof(T) * count;

      // request aligned memory matching the type's natural alignment requirement
      u8* raw_ptr = alloc_raw(total_bytes, alignof(T));
      BASE_ASSERT(raw_ptr);
      if (!raw_ptr) return nullptr;

      // Project portability shim formally begins typed array lifetime over the
      // raw storage returned by the arena.
      return portability::start_lifetime_as_array<T>(raw_ptr, count);
    }

    /**
     * @brief Allocates storage for one object of type \p T.
     * @tparam T Object type.
     * @return Pointer to allocated storage, or nullptr on failure.
      * @pre init() has succeeded.
     */
    template <typename T>
    T* alloc_struct() {
      return this->alloc_array<T>(1);
    }

    /**
     * @brief Resets the allocation cursor to the beginning of the arena.
      * @post offset is zero.
      * @warning All previously returned pointers become invalid for future writes.
     */
    constexpr void reset() {
      BASE_ASSERT(this->buffer);
      if (this->buffer) {
        this->offset = 0;
      }
    }

    /**
     * @brief Executes a scoped allocation phase and rewinds cursor afterward.
     * @tparam F Callable type accepting Arena*.
     * @param lambda_pipeline Function invoked with this arena.
      * @post offset is restored to its entry value.
     */
    template <typename F>
    inline void scoped_scratch(F&& lambda_pipeline) {
      u64 snapshot = this->offset;
      lambda_pipeline(this);
      this->offset = snapshot;
    }

    /**
     * @brief Releases the mapped memory and returns arena to an empty state.
      * @post buffer is null and capacity/offset are zero.
      * @warning All previously returned pointers are invalid after this call.
     */
    void free() {
      BASE_ASSERT(this->buffer);
      if (this->buffer) {
        vm::free(this->buffer, this->capacity);
        this->buffer = nullptr;
        this->capacity = 0;
        this->offset = 0;
      }
    }

  private:
    /**
     * @brief Aligns a byte offset up to the requested power-of-two boundary.
     * @param ptr Unaligned offset.
     * @param align Alignment value.
     * @return Aligned offset.
      * @pre \p align is a power of two.
     */
    constexpr u64 align_forward(u64 ptr, u64 align) {
      return (ptr + align - 1) & ~(align - 1);
    }

    /**
     * @brief Internal raw byte allocation primitive.
     * @param size Byte count requested.
     * @param alignment Requested power-of-two alignment.
     * @return Byte pointer to allocated span, or nullptr if capacity is exceeded.
      * @pre init() has succeeded.
     */
    u8* alloc_raw(u64 size, u64 alignment = 8) {
      BASE_ASSERT(this->buffer);

      // use the single-evaluation constexpr helper from core_types
      u64 aligned_offset = AlignUpPow2(this->offset, alignment);

      // OOM check
      if (aligned_offset + size > this->capacity) {
        return nullptr;
      }

      u8* ptr = this->buffer + aligned_offset;
      this->offset = aligned_offset + size;
      return ptr;
    }

  };

}   // namespace base::mem

export namespace base {
  using Arena = mem::Arena;
}