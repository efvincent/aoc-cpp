export module core_result_s;

import core_types;
import core_result;
import core_string;

export namespace base {

// -----------------------------------------------------------------------------
// BIT-PACKED RESULTS (Strict 16-Byte Register Return)
// -----------------------------------------------------------------------------

// The MSB mask: 10000000 00000000 ...
constexpr base::u64 RESULT_OK_BIT = 0x8000000000000000ull;
// The Length mask: 01111111 11111111 ...
constexpr base::u64 RESULT_LEN_MASK = 0x7FFFFFFFFFFFFFFFull;

template <typename T>
struct ResultS {
  // Structural Defense: T must fit in the first 8 bytes so it doesn't
  // collide with our smuggled bit in the second 8 bytes.
  static_assert(sizeof(T) <= 8,
                "FATAL: Packed ResultS requires payload T to be 8 bytes or smaller (e.g., u64 or pointer).");

  union {
    Str8 error;
    T value;
  };

  // --- Core Interaction Methods ---

  constexpr bool is_ok() const {
    return (this->error.len & RESULT_OK_BIT) != 0;
  }

  constexpr Str8 get_error() const {
    return Str8(this->error.str, this->error.len & RESULT_LEN_MASK);
  }

  // --- Static Constructors ---

  constexpr static ResultS ok(T val) {
    ResultS res = {};         // Correctly zero-initialized!
    res.value = val;
    // Smuggle the '1' into the top bit of the unused padding space
    res.error.len |= RESULT_OK_BIT;
    return res;
  }

  constexpr static ResultS err(base::Str8 err_str) {
    ResultS res = {};
    res.error.str = err_str.str;
    // Ensure the top bit is '0', leaving the raw string length perfectly intact
    res.error.len = err_str.len & RESULT_LEN_MASK;
    return res;
  }
};
}