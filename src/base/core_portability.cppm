/**
 * @file core_portability.cppm
 * @brief Compiler and standard-library gap shims used by low-level modules.
 */

/**
 * @module core_portability
 * @brief Project-owned portability layer for intrinsics, traits, and shims.
 */
export module core_portability;

import core_types;

/**
 * @namespace base::portability
 * @brief Compiler-facing portability helpers that replace selected std facilities.
 */
export namespace base::portability {

  #if defined(__clang__)
  #pragma clang diagnostic push
  #pragma clang diagnostic ignored "-Wvla-cxx-extension"
  #endif

  #if defined(_MSC_VER) || defined(__clang__) || defined(__GNUC__)
  /** @brief Compiler-backed trivially-copyable trait wrapper. */
  template <class T>
  inline constexpr bool is_trivially_copyable_v = __is_trivially_copyable(T);
  #else
    #error "core_portability requires compiler support for __is_trivially_copyable"
  #endif

  /**
   * @brief Starts array object lifetime over already-allocated raw storage.
   * @tparam T Element type.
   * @param p Base pointer to suitably aligned raw storage.
   * @param n Number of elements whose lifetime should begin.
   * @return Typed pointer to the first element in the array region.
   * @note Restricted to trivially copyable types in this project.
   */
  template <class T>
  [[gnu::always_inline]] inline T* start_lifetime_as_array(void* p, u64 n) noexcept {
    // Conservative gate: implicit-lifetime is broader, but this is a safe subset.
    static_assert(is_trivially_copyable_v<T>,
                  "Arena lifetime shim requires trivially copyable T");

    T* q = reinterpret_cast<T*>(p);
    if (n == 0) return q;

  #if defined(__GNUC__) || defined(__clang__)
    // Same optimizer barrier idea used by libstdc++.
    auto region = (__extension__ reinterpret_cast<T(*)[n]>(p));
    __asm__ __volatile__("" : "=r"(q), "=m"(*region) : "0"(q), "m"(*region));
  #endif
    return q;
  }

  #if defined(__clang__)
  #pragma clang diagnostic pop
  #endif
  /**
   * @brief Marks a control-flow path as unreachable for supported compilers.
   * @warning Executing this function is undefined behavior.
   */
  [[noreturn]] inline void unreachable() noexcept {
  #if defined(_MSC_VER)
    __assume(0);
  #elif defined(__clang__) || defined(__GNUC__)
    __builtin_unreachable();
  #else
    #error "base_unreachable requires compiler-specific unreachable intrinsic"
  #endif
  }

  /** @brief Trait mapping signedness for project scalar types. */
  template <class T>
  struct is_signed {
    static constexpr bool value = false;
  };

  template <>
  struct is_signed<i8> {
    static constexpr bool value = true;
  };

  template <>
  struct is_signed<i16> {
    static constexpr bool value = true;
  };

  template <>
  struct is_signed<i32> {
    static constexpr bool value = true;
  };

  template <>
  struct is_signed<i64> {
    static constexpr bool value = true;
  };

  template <>
  struct is_signed<u8> {
    static constexpr bool value = false;
  };

  template <>
  struct is_signed<u16> {
    static constexpr bool value = false;
  };

  template <>
  struct is_signed<u32> {
    static constexpr bool value = false;
  };

  template <>
  struct is_signed<u64> {
    static constexpr bool value = false;
  };

  template <class T>
  inline constexpr bool is_signed_v = is_signed<T>::value;

  #if defined(_MSC_VER) || defined(__clang__) || defined(__GNUC__)
  /** @brief Compiler-backed standard-layout trait wrapper. */
  template <class T>
  inline constexpr bool is_standard_layout_v = __is_standard_layout(T);
  #else
    #error "core_portability requires compiler support for __is_standard_layout"
  #endif

  /** @brief Maps project scalar types to their unsigned counterparts. */
  template <class T>
  struct make_unsigned;

  template <>
  struct make_unsigned<i8> {
    using type = u8;
  };

  template <>
  struct make_unsigned<i16> {
    using type = u16;
  };

  template <>
  struct make_unsigned<i32> {
    using type = u32;
  };

  template <>
  struct make_unsigned<i64> {
    using type = u64;
  };

  template <>
  struct make_unsigned<u8> {
    using type = u8;
  };

  template <>
  struct make_unsigned<u16> {
    using type = u16;
  };

  template <>
  struct make_unsigned<u32> {
    using type = u32;
  };

  template <>
  struct make_unsigned<u64> {
    using type = u64;
  };

  template <class T>
  using make_unsigned_t = typename make_unsigned<T>::type;

  /** @brief Project-owned numeric limits wrapper for core scalar aliases. */
  template <class T>
  struct numeric_limits;

  template <>
  struct numeric_limits<i8> {
    static constexpr i8 min() noexcept { return min_i8; }
    static constexpr i8 max() noexcept { return max_i8; }
  };

  template <>
  struct numeric_limits<i16> {
    static constexpr i16 min() noexcept { return min_i16; }
    static constexpr i16 max() noexcept { return max_i16; }
  };

  template <>
  struct numeric_limits<i32> {
    static constexpr i32 min() noexcept { return min_i32; }
    static constexpr i32 max() noexcept { return max_i32; }
  };

  template <>
  struct numeric_limits<i64> {
    static constexpr i64 min() noexcept { return min_i64; }
    static constexpr i64 max() noexcept { return max_i64; }
  };

  template <>
  struct numeric_limits<u8> {
    static constexpr u8 min() noexcept { return 0; }
    static constexpr u8 max() noexcept { return max_u8; }
  };

  template <>
  struct numeric_limits<u16> {
    static constexpr u16 min() noexcept { return 0; }
    static constexpr u16 max() noexcept { return max_u16; }
  };

  template <>
  struct numeric_limits<u32> {
    static constexpr u32 min() noexcept { return 0; }
    static constexpr u32 max() noexcept { return max_u32; }
  };

  template <>
  struct numeric_limits<u64> {
    static constexpr u64 min() noexcept { return 0; }
    static constexpr u64 max() noexcept { return max_u64; }
  };

}