module;

#if defined(__linux__)
  #include <sys/mman.h>
#elif defined(_WIN32)
  #error "core_vm: Windows VM backend not yet implemented"
#else
  #error "core_vm: Unsupported platform; add an explicit core_vm backend first"
#endif

/**
 * @file core_vm.cppm
 * @brief Operating-system virtual memory wrappers used by arena allocation.
 */

/**
 * @module core_vm
 * @brief VM backend operations that isolate OS-specific allocation calls.
 */
export module core_vm;

import core_types;

/**
 * @namespace base
 * @brief Shared low-level primitives including VM allocation boundaries.
 */
export namespace base {

  /**
   * @brief Allocates a contiguous writable virtual memory region.
   * @param bytes Number of bytes requested.
   * @return Base pointer to writable storage, or nullptr on failure.
   * @note Current implementation delegates to the Linux mmap backend.
   */
  inline void* vm_alloc(u64 bytes) noexcept {
    #if defined(__linux__)
    
      int map_flags = MAP_PRIVATE | MAP_ANONYMOUS;
      
      #if defined(MAP_POPULATE)
      
        map_flags |= MAP_POPULATE;
      
      #endif
      
      void* ptr = mmap(nullptr, bytes, PROT_READ | PROT_WRITE, map_flags, -1, 0);
      return (ptr == MAP_FAILED) ? nullptr : ptr;
    
    #endif
  }

  /**
   * @brief Releases a region previously returned by vm_alloc.
   * @param ptr Base pointer returned by vm_alloc.
   * @param bytes Size of the mapped region in bytes.
   */
  inline void vm_free(void* ptr, u64 bytes) noexcept {
    #if defined(__linux__)
    if (ptr) munmap(ptr, bytes);
    #endif
  }

  /**
   * @brief Best-effort huge-page hint for a mapped region.
   * @note This hint is optional and never treated as an allocation failure.
   * @param ptr Base pointer of the mapped region.
   * @param bytes Length of the mapped region.
   */
  inline void vm_hint_hugepages(void* ptr, u64 bytes) noexcept {
    #if defined(MADV_HUGEPAGE)
    madvise(ptr, bytes, MADV_HUGEPAGE);
    #else
    (void)ptr;
    (void)bytes;
    #endif
  }
}