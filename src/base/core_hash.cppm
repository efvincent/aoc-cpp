module;

export module core_hash;

import core_types;
import core_string;

using namespace base;

export namespace base::hash {

  namespace detail {
    // 64-bit finalizer mix  (splitmix style) for stable, high-quality integer diffusion
    constexpr u64 mix64(u64 x) {
      x ^= (x >> 30);
      x *= 0xbf58476d1ce4e5b9ULL;
      x ^= (x >> 27);
      x *= 0x94d049bb133111ebULL;
      x ^= (x >> 31);
      return x;      
    }
  }

  template <typename K>
  struct HashTraits;      // unsupported keys fail at compile time

  // Unsigned integer key traits

  template<>
  struct HashTraits<u8> {
    static constexpr u64 hash(const u8 key) { return detail::mix64((u64)key); }
    static constexpr bool eq(const u8 a, const u8 b) { return a == b; }
  };

  template<>
  struct HashTraits<u16> {
    static constexpr u64 hash(const u16 key) { return detail::mix64((u64)key); }
    static constexpr bool eq(const u16 a, const u16 b) { return a == b; }
  };

  template <>
  struct HashTraits<u32> {
    static constexpr u64 hash(const u32 key) { return detail::mix64((u64)key); }
    static constexpr bool eq(const u32 a, const u32 b) { return a == b; }
  };

  template <>
  struct HashTraits<u64> {
    static constexpr u64 hash(const u64 key) { return detail::mix64(key); }
    static constexpr bool eq(const u64 a, const u64 b) { return a == b; }
  };

  // Signed integer key traits

    template <>
  struct HashTraits<i8> {
    static constexpr u64 hash(const i8 key) { return detail::mix64((u64)(i64)key); }
    static constexpr bool eq(const i8 a, const i8 b) { return a == b; }
  };

  template <>
  struct HashTraits<i16> {
    static constexpr u64 hash(const i16 key) { return detail::mix64((u64)(i64)key); }
    static constexpr bool eq(const i16 a, const i16 b) { return a == b; }
  };

  template <>
  struct HashTraits<i32> {
    static constexpr u64 hash(const i32 key) { return detail::mix64((u64)(i64)key); }
    static constexpr bool eq(const i32 a, const i32 b) { return a == b; }
  };

  template <>
  struct HashTraits<i64> {
    static constexpr u64 hash(const i64 key) { return detail::mix64((u64)key); }
    static constexpr bool eq(const i64 a, const i64 b) { return a == b; }
  };

  // Str8 key traits, using existing hash & equality behavior
  template<>
  struct HashTraits<Str8> {
    static constexpr u64 hash(const Str8& key) {
      return key.hash_fnv1a();
    }

    static constexpr bool eq(const Str8& a, const Str8& b) {
      return a.match(b);
    }
  };

  // Generic trait-dispatch helpers used by HashMap core
  template <typename K>
  constexpr u64 hash_of(const K& key) {
    return HashTraits<K>::hash(key);
  }

  template <typename K>
  constexpr bool key_eq(const K& a, const K& b) {
    return HashTraits<K>::eq(a, b);
  }
  
} // namespace base::hash