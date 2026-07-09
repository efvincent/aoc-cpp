module; 

#include <stdint.h>

/**
 * @file core_types.cppm
 * @brief Foundational scalar aliases, limits, and constexpr numeric helpers.
 */

/**
 * @module core_types
 * @brief Base type and arithmetic utilities shared by core modules.
 */
export module core_types;

/**
 * @namespace base
 * @brief Shared low-level primitives for runtime and puzzle code.
 */
export namespace base {

    // ==========================================================================
    // 1. Explicit Fixed-Width Scalar Aliases
    // ==========================================================================
    /** @brief Signed 8-bit integer alias. */
    using i8  = int8_t;
    /** @brief Signed 16-bit integer alias. */
    using i16 = int16_t;
    /** @brief Signed 32-bit integer alias. */
    using i32 = int32_t;
    /** @brief Signed 64-bit integer alias. */
    using i64 = int64_t;

    /** @brief Unsigned 8-bit integer alias. */
    using u8  = uint8_t;
    /** @brief Unsigned 16-bit integer alias. */
    using u16 = uint16_t;
    /** @brief Unsigned 32-bit integer alias. */
    using u32 = uint32_t;
    /** @brief Unsigned 64-bit integer alias. */
    using u64 = uint64_t;

    /** @brief 32-bit floating-point alias. */
    using f32 = float;
    /** @brief 64-bit floating-point alias. */
    using f64 = double;

    /** @brief Function pointer alias for a no-argument void callback. */
    using VoidFunc = void(*)(void);

    // ==========================================================================
    // 2. Type-Safe Limits
    // ==========================================================================
    /** @brief Minimum representable i8 value. */
    constexpr i8  min_i8  = -128;
    /** @brief Minimum representable i16 value. */
    constexpr i16 min_i16 = -32768;
    /** @brief Minimum representable i32 value. */
    constexpr i32 min_i32 = (-2147483647 - 1);
    /** @brief Minimum representable i64 value. */
    constexpr i64 min_i64 = (-9223372036854775807LL - 1LL);

    /** @brief Maximum representable i8 value. */
    constexpr i8  max_i8  = 127;
    /** @brief Maximum representable i16 value. */
    constexpr i16 max_i16 = 32767;
    /** @brief Maximum representable i32 value. */
    constexpr i32 max_i32 = 2147483647;
    /** @brief Maximum representable i64 value. */
    constexpr i64 max_i64 = 9223372036854775807LL;

    /** @brief Maximum representable u8 value. */
    constexpr u8  max_u8  = 255U;
    /** @brief Maximum representable u16 value. */
    constexpr u16 max_u16 = 65535U;
    /** @brief Maximum representable u32 value. */
    constexpr u32 max_u32 = 4294967295U;
    /** @brief Maximum representable u64 value. */
    constexpr u64 max_u64 = 18446744073709551615ULL;

    // ==========================================================================
    // 3. Constexpr Mathematical Helpers (Standardized on u64)
    // ==========================================================================
    /**
     * @brief Returns the lesser of two values.
     * @tparam T Value type supporting operator<.
     * @param a First operand.
     * @param b Second operand.
     * @return Lesser value.
     */
    template <typename T>
    constexpr T Min(T a, T b) { return (a < b) ? a : b; }

    /**
     * @brief Returns the greater of two values.
     * @tparam T Value type supporting operator>.
     * @param a First operand.
     * @param b Second operand.
     * @return Greater value.
     */
    template <typename T>
    constexpr T Max(T a, T b) { return (a > b) ? a : b; }

    /**
     * @brief Clamps to an upper bound.
     * @tparam T Value type supporting ordering.
     * @param a Candidate value.
     * @param b Upper bound.
     * @return min(a, b).
     */
    template <typename T>
    constexpr T ClampTop(T a, T b) { return Min(a, b); }

    /**
     * @brief Clamps to a lower bound.
     * @tparam T Value type supporting ordering.
     * @param a Candidate value.
     * @param b Lower bound.
     * @return max(a, b).
     */
    template <typename T>
    constexpr T ClampBottom(T a, T b) { return Max(a, b); }

    /**
     * @brief Aligns value up to the next multiple of a power-of-two alignment.
     * @param x Input value.
     * @param p Alignment, expected power of two.
     * @return Smallest value >= x aligned to p.
     * @pre \p p is a power of two.
     */
    constexpr u64 AlignUpPow2(u64 x, u64 p) {
        return (x + (p - 1)) & ~(p - 1);
    }

    /**
     * @brief Aligns value down to a power-of-two boundary.
     * @param x Input value.
     * @param p Alignment, expected power of two.
     * @return Greatest value <= x aligned to p.
     * @pre \p p is a power of two.
     */
    constexpr u64 AlignDownPow2(u64 x, u64 p) {
        return x & ~(p - 1);
    }

    /**
     * @brief Tests whether a value is zero or a power of two.
     * @param x Input value.
     * @return True if x is 0 or power-of-two; otherwise false.
     */
    constexpr bool IsPow2OrZero(u64 x) {
        return (x & (x - 1)) == 0;
    }

    // ==========================================================================
    // 4. Literal Sizing Operators (Using u64)
    // ==========================================================================
    /**
     * @brief Converts kibibytes to bytes.
     * @param x Input count in KiB.
     * @return Byte count (x * 1024).
     */
    constexpr u64 KB(u64 x) { return x << 10; }
    /**
     * @brief Converts mebibytes to bytes.
     * @param x Input count in MiB.
     * @return Byte count (x * 1024^2).
     */
    constexpr u64 MB(u64 x) { return x << 20; }
    /**
     * @brief Converts gibibytes to bytes.
     * @param x Input count in GiB.
     * @return Byte count (x * 1024^3).
     */
    constexpr u64 GB(u64 x) { return x << 30; }
    /**
     * @brief Converts tebibytes to bytes.
     * @param x Input count in TiB.
     * @return Byte count (x * 1024^4).
     */
    constexpr u64 TB(u64 x) { return x << 40; }

    /**
     * @brief Scales a value by one thousand.
     * @param x Input value.
     * @return x * 1000.
     */
    constexpr u64 Thousand(u64 x) { return x * 1'000; }
    /**
     * @brief Scales a value by one million.
     * @param x Input value.
     * @return x * 1000000.
     */
    constexpr u64 Million(u64 x)  { return x * 1'000'000; }
    /**
     * @brief Scales a value by one billion.
     * @param x Input value.
     * @return x * 1000000000.
     */
    constexpr u64 Billion(u64 x)  { return x * 1'000'000'000; }
    /**
     * @brief Scales a value by one trillion.
     * @param x Input value.
     * @return x * 1000000000000.
     */
    constexpr u64 Trillion(u64 x) { return x * 1'000'000'000'000ULL; }
}