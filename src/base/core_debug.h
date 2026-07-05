#pragma once

/**
 * @file core_debug.h
 * @brief Assertion macros used by base modules.
 * @note Assertions are enabled in debug/instrument builds and compiled out in release.
 */

#if defined(BUILD_DEBUG) || defined(BUILD_INSTRUMENT)
  #if defined(_MSC_VER)
    /**
     * @def BASE_ASSERT(expression)
     * @brief Terminates immediately when \p expression evaluates to false.
     * @param expression Boolean condition that must hold.
     * @note MSVC path uses __debugbreak().
     */
    #define BASE_ASSERT(expression) if(!(expression)) { __debugbreak(); }
  #else
    /**
     * @def BASE_ASSERT(expression)
     * @brief Terminates immediately when \p expression evaluates to false.
     * @param expression Boolean condition that must hold.
     * @note Clang/GCC path uses __builtin_trap().
     */
    #define BASE_ASSERT(expression) if(!(expression)) { __builtin_trap(); }
  #endif
#else
  /**
   * @def BASE_ASSERT(expression)
   * @brief No-op assertion macro for non-debug builds.
   * @param expression Ignored.
   */
  #define BASE_ASSERT(expression)
#endif