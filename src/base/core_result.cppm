export module core_result;

import core_types;

export namespace base {

template <typename E, typename T>
struct Result {
  // Structural Defense guards
  static_assert(sizeof(E) <= 8,
                "FATAL: Error E is too large. Use an enum or a lightweight primitive.");

  static_assert(sizeof(T) <= 16,
                "FATAL: Payload T is too large. It will trigger stack spilling. Return a pointer or a Str8 slice instead.");

  union {
    E error; // the "left" or error state
    T value; // the "right" or success state
  };

  // Obfuscate the raw data so the pipeline uses the method
  bool _is_ok;

  // --- Core Interaction Methods ---
  constexpr bool is_ok() const {
    return this->_is_ok;
  }

  // --- Zero-cost static constructors ---
  constexpr static Result ok(T v) {
    Result res = {};         // FIX: Zero-initialize memory to appease constexpr evaluation
    res.value = v;
    res._is_ok = true;
    return res;
  }

  constexpr static Result err(E e) {
    Result res = {};         // FIX: Zero-initialize memory
    res.error = e;
    res._is_ok = false;
    return res;
  }
};



} // namespace base