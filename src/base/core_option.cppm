module;

/**
 * @file core_option.cppm
 * @brief Lightweight optional value carrier for explicit presence semantics.
 */

export module core_option;

import core_types;
import core_portability;

/**
 * @module core_option
 * @brief Optional payload wrappers designed for low-level core APIs.
 */
export namespace base {

  /**
   * @brief Optional value wrapper for small trivially-copyable payloads.
   * @tparam T Payload type.
   */
  template <typename T>
  struct Option  {
    T value;
    bool has;

    static_assert(portability::is_trivially_copyable_v<T>,
                  "FATAL: Option<T> requires trivially copyable T.");
    static_assert(sizeof(T) <= 8,
                  "FATAL: Option<T> payload too large. Use pointer/slice handle types.");
    
    constexpr bool is_some() const { return this->has; }
    constexpr bool is_none() const { return !this->has; }

    constexpr T& unwrap() { return this->value; }
    constexpr const T& unwrap() const { return this->value; }

    constexpr T value_or(T fallback) const {
      return this->has ? this->value : fallback;
    }

    constexpr static Option some(T v) {
      Option out = {};
      out.value = v;
      out.has = true;
      return out;
    }

    constexpr static Option none() {
      Option out = {};
      out.has = false;
      return out;
    }
  };

  /**
   * @brief Pointer specialization that uses nullptr as the none-state.
   * @tparam T Pointee type.
   */
  template <typename T>
  struct Option<T*> {
    T* value;

    constexpr bool is_some() const { return this->value != nullptr; }
    constexpr bool is_none() const { return this->value == nullptr; }

    constexpr T* unwrap() const { return this->value; }

    constexpr T* value_or(T* fallback) const {
      return this->value ? this->value : fallback;
    }

    constexpr static Option some(T* v) {
      Option out = {};
      out.value = v;
      return out;
    }

    constexpr static Option none() {
      Option out = {};
      out.value = nullptr;
      return out;
    }
  };

  static_assert(sizeof(Option<void*>) == sizeof(void*),
                "FATAL: Option<T*> must remain pointer-sized.");
  static_assert(sizeof(Option<u64>) <= 16,
                "FATAL: Option<u64> exceeded 16-byte register-return envelope.");
}