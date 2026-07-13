module;

#include <string.h>
#include "core_debug.h"

/**
 * @file core_hash_map.cppm
 * @brief Arena-backed Robin Hood hash map and hash set primitives.
 */

/**
 * @module core_hash_map
 * @brief Open-addressed hash containers for map and set workloads.
 */
export module core_hash_map;

import core_types;
import core_memory;
import core_hash;
import core_result;

using namespace base;

/**
 * @namespace base::hashmap
 * @brief Hash container primitives built on arena allocation.
 */
export namespace base::hashmap {

  /**
   * @brief Error categories returned by HashMap operations.
   */
  enum struct HashMapE : u8 { 
    /** @brief No error. */
    None = 0,
    /** @brief Arena allocation failed while reserving or rehashing. */
    OutOfMemory,
    /** @brief Probe walk exhausted the full table unexpectedly. */
    ProbeSequenceExhausted
  };

  /**
   * @brief Result alias used by HashMap and HashSet APIs.
   * @tparam T Success payload type.
   */
  template <typename T>
  using HashMapR = Result<HashMapE, T>;

  namespace detail {
    /** @brief Control-byte marker for an empty slot. */
    constexpr u8 CTRL_EMPTY = 0;
    /** @brief Control-byte marker for an occupied slot. */
    constexpr u8 CTRL_FULL = 1;

    /** @brief Load-factor numerator (4/5 == 80%). */
    constexpr u64 LOAD_NUM = 4;   // 80%
    /** @brief Load-factor denominator (4/5 == 80%). */
    constexpr u64 LOAD_DEN = 5;

    /** @brief Smallest table capacity the implementation will allocate. */
    constexpr u64 MIN_CAPACITY = 8;

    /** @brief Tests whether x is a non-zero power of two. */
    constexpr bool is_pow2_nonzero(u64 x) {
      return x != 0 && ((x & (x - 1)) == 0);
    }

    /** @brief Rounds x up to the next power of two. */
    constexpr u64 next_pow2(u64 x) {
      if (x <= 1) return 1;
      x -= 1;
      x |= (x >> 1);
      x |= (x >> 2);
      x |= (x >> 4);
      x |= (x >> 8);
      x |= (x >> 16);
      x |= (x >> 32);
      return x + 1;
    }

    /** @brief Normalizes a caller request to a supported table capacity. */
    constexpr u64 normalize_capacity(u64 requested_capacity) {
      u64 capped = (requested_capacity < MIN_CAPACITY) ? MIN_CAPACITY : requested_capacity;
      return next_pow2(capped);
    }

    /** @brief Returns the maximum live-entry count allowed before growth. */
    constexpr u64 max_count_for_capacity(u64 capacity) {
      return (capacity * LOAD_NUM) / LOAD_DEN;
    }

    /** @brief Computes Robin Hood probe distance from a key's home slot. */
    constexpr u64 probe_distance(u64 slot, u64 home, u64 capacity) {
      return (slot + capacity - home) & (capacity - 1);
    }

    template <typename T>
    /** @brief Lightweight swap helper for trivially movable values. */
    inline void swap_values(T& a, T& b) {
      T tmp = a;
      a = b;
      b = tmp;
    }

    template <typename K, typename V>
    /**
     * @brief Inserts or overwrites one key-value entry in raw map storage.
     * @return Ok(true) when a new key is inserted, Ok(false) when an existing
     *         key is overwritten, or Err(HashMapE) on failure.
     */
    inline HashMapR<bool> robin_hood_upsert_raw_map(
      K* keys,
      V* values,
      u64* hashes,
      u8* ctrl,
      u64 capacity,
      u64& io_count,
      const K& key,
      const V& value
    ) {
      BASE_ASSERT(keys != nullptr);
      BASE_ASSERT(values != nullptr);
      BASE_ASSERT(hashes != nullptr);
      BASE_ASSERT(ctrl != nullptr);
      BASE_ASSERT(is_pow2_nonzero(capacity));

      const u64 mask = capacity - 1;
      K cur_key = key;
      V cur_value = value;
      u64 cur_hash = hash_of(cur_key);
      u64 idx = cur_hash & mask;
      u64 dist = 0;

      for (u64 steps = 0; steps < capacity; ++steps) {
        if (ctrl[idx] == CTRL_EMPTY) {
          ctrl[idx] = CTRL_FULL;
          keys[idx] = cur_key;
          values[idx] = cur_value;
          hashes[idx] = cur_hash;
          io_count += 1;
          return HashMapR<bool>::ok(true);
        }

        if (hashes[idx] == cur_hash && key_eq(keys[idx], cur_key)) {
          values[idx] = cur_value;
          return HashMapR<bool>::ok(false);
        }

        u64 resident_home = hashes[idx] & mask;
        u64 resident_dist = probe_distance(idx, resident_home, capacity);

        if (resident_dist < dist) {
          swap_values(keys[idx], cur_key);
          swap_values(values[idx], cur_value);
          swap_values(hashes[idx], cur_hash);
          dist = resident_dist;  
        }

        idx = (idx + 1) & mask;
        dist += 1;
      }

      return HashMapR<bool>::err(HashMapE::ProbeSequenceExhausted);
    }

    template <typename K>
    /**
     * @brief Inserts one key into raw set storage if it is not already present.
     * @return Ok(true) when a new key is inserted, Ok(false) when the key
     *         already exists, or Err(HashMapE) on failure.
     */
    inline HashMapR<bool> robin_hood_upsert_raw_set(
      K* keys,
      u64* hashes,
      u8* ctrl,
      u64 capacity,
      u64& io_count,
      const K& key
    ) {
      BASE_ASSERT(keys != nullptr);
      BASE_ASSERT(hashes != nullptr);
      BASE_ASSERT(ctrl != nullptr);
      BASE_ASSERT(is_pow2_nonzero(capacity));

      const u64 mask = capacity - 1;
      K cur_key = key;
      u64 cur_hash = hash_of(cur_key);
      u64 idx = cur_hash & mask;
      u64 dist = 0;

      for (u64 steps = 0; steps < capacity; ++steps) {
        if (ctrl[idx] == CTRL_EMPTY) {
          ctrl[idx] = CTRL_FULL;
          keys[idx] = cur_key;
          hashes[idx] = cur_hash;
          io_count += 1;
          return HashMapR<bool>::ok(true);
        }

        if (hashes[idx] == cur_hash && key_eq(keys[idx], cur_key)) {
          return HashMapR<bool>::ok(false);
        }

        u64 resident_home = hashes[idx] & mask;
        u64 resident_dist = probe_distance(idx, resident_home, capacity);

        if (resident_dist < dist) {
          swap_values(keys[idx], cur_key);
          swap_values(hashes[idx], cur_hash);
          dist = resident_dist;
        }

        idx = (idx + 1) & mask;
        dist += 1;
      }

      return HashMapR<bool>::err(HashMapE::ProbeSequenceExhausted);
    }

    template <typename K, typename V>
    /**
     * @brief Finds a mutable value pointer inside raw map storage.
     * @return Ok(pointer) when found, Ok(nullptr) when absent, or Err(HashMapE)
     *         on probe failure.
     */
    inline HashMapR<V*> robin_hood_find_ptr_raw_map(
      K* keys,
      V* values,
      const u64* hashes,
      const u8* ctrl,
      u64 capacity,
      const K& key
    ) {
      if (capacity == 0) {
        return HashMapR<V*>::ok(nullptr);
      }

      BASE_ASSERT(keys != nullptr);
      BASE_ASSERT(values != nullptr);
      BASE_ASSERT(hashes != nullptr);
      BASE_ASSERT(ctrl != nullptr);
      BASE_ASSERT(is_pow2_nonzero(capacity));

      const u64 mask = capacity - 1;
      u64 key_hash = hash_of(key);
      u64 idx = key_hash & mask;
      u64 dist = 0;

      for (u64 steps = 0; steps < capacity; ++steps) {
        if (ctrl[idx] == CTRL_EMPTY) {
          return HashMapR<V*>::ok(nullptr);
        }

        u64 resident_home = hashes[idx] & mask;
        u64 resident_dist = probe_distance(idx, resident_home, capacity);

        if (resident_dist < dist) {
          return HashMapR<V*>::ok(nullptr);
        }

        if (hashes[idx] == key_hash && key_eq(keys[idx], key)) {
          return HashMapR<V*>::ok(&values[idx]);
        }

        idx = (idx + 1) & mask;
        dist += 1;
      }

      return HashMapR<V*>::err(HashMapE::ProbeSequenceExhausted);
    }

    template <typename K, typename V>
    /**
     * @brief Finds a const value pointer inside raw map storage.
     * @return Ok(pointer) when found, Ok(nullptr) when absent, or Err(HashMapE)
     *         on probe failure.
     */
    inline HashMapR<const V*> robin_hood_find_ptr_raw_map_const(
      const K* keys,
      const V* values,
      const u64* hashes,
      const u8* ctrl,
      u64 capacity,
      const K& key
    ) {
      if (capacity == 0) {
        return HashMapR<const V*>::ok(nullptr);
      }

      BASE_ASSERT(keys != nullptr);
      BASE_ASSERT(values != nullptr);
      BASE_ASSERT(hashes != nullptr);
      BASE_ASSERT(ctrl != nullptr);
      BASE_ASSERT(is_pow2_nonzero(capacity));

      const u64 mask = capacity - 1;
      u64 key_hash = hash_of(key);
      u64 idx = key_hash & mask;
      u64 dist = 0;

      for (u64 steps = 0; steps < capacity; ++steps) {
        if (ctrl[idx] == CTRL_EMPTY) {
          return HashMapR<const V*>::ok(nullptr);
        }

        u64 resident_home = hashes[idx] & mask;
        u64 resident_dist = probe_distance(idx, resident_home, capacity);

        if (resident_dist < dist) {
          return HashMapR<const V*>::ok(nullptr);
        }

        if (hashes[idx] == key_hash && key_eq(keys[idx], key)) {
          return HashMapR<const V*>::ok(&values[idx]);
        }

        idx = (idx + 1) & mask;
        dist += 1;
      }

      return HashMapR<const V*>::err(HashMapE::ProbeSequenceExhausted);
    }    

    template <typename K>
    /**
     * @brief Finds a mutable key pointer inside raw set storage.
     * @return Ok(pointer) when found, Ok(nullptr) when absent, or Err(HashMapE)
     *         on probe failure.
     */
    inline HashMapR<K*> robin_hood_find_key_ptr_raw_set(
      K* keys,
      const u64* hashes,
      const u8* ctrl,
      u64 capacity,
      const K& key
    ) {
      if (capacity == 0) {
        return HashMapR<K*>::ok(nullptr);
      }

      BASE_ASSERT(keys != nullptr);
      BASE_ASSERT(hashes != nullptr);
      BASE_ASSERT(ctrl != nullptr);
      BASE_ASSERT(is_pow2_nonzero(capacity));

      const u64 mask = capacity - 1;
      u64 key_hash = hash_of(key);
      u64 idx = key_hash & mask;
      u64 dist = 0;

      for (u64 steps = 0; steps < capacity; ++steps) {
        if (ctrl[idx] == CTRL_EMPTY) {
          return HashMapR<K*>::ok(nullptr);
        }

        u64 resident_home = hashes[idx] & mask;
        u64 resident_dist = probe_distance(idx, resident_home, capacity);

        if (resident_dist < dist) {
          return HashMapR<K*>::ok(nullptr);
        }

        if (hashes[idx] == key_hash && key_eq(keys[idx], key)) {
          return HashMapR<K*>::ok(&keys[idx]);
        }

        idx = (idx + 1) & mask;
        dist += 1;
      }

      return HashMapR<K*>::err(HashMapE::ProbeSequenceExhausted);
    }

    template <typename K>
    /**
     * @brief Finds a const key pointer inside raw set storage.
     * @return Ok(pointer) when found, Ok(nullptr) when absent, or Err(HashMapE)
     *         on probe failure.
     */
    inline HashMapR<const K*> robin_hood_find_key_ptr_raw_set_const(
      const K* keys,
      const u64* hashes,
      const u8* ctrl,
      u64 capacity,
      const K& key
    ) {
      if (capacity == 0) {
        return HashMapR<const K*>::ok(nullptr);
      }

      BASE_ASSERT(keys != nullptr);
      BASE_ASSERT(hashes != nullptr);
      BASE_ASSERT(ctrl != nullptr);
      BASE_ASSERT(is_pow2_nonzero(capacity));

      const u64 mask = capacity - 1;
      u64 key_hash = hash_of(key);
      u64 idx = key_hash & mask;
      u64 dist = 0;

      for (u64 steps = 0; steps < capacity; ++steps) {
        if (ctrl[idx] == CTRL_EMPTY) {
          return HashMapR<const K*>::ok(nullptr);
        }

        u64 resident_home = hashes[idx] & mask;
        u64 resident_dist = probe_distance(idx, resident_home, capacity);

        if (resident_dist < dist) {
          return HashMapR<const K*>::ok(nullptr);
        }

        if (hashes[idx] == key_hash && key_eq(keys[idx], key)) {
          return HashMapR<const K*>::ok(&keys[idx]);
        }

        idx = (idx + 1) & mask;
        dist += 1;
      }

      return HashMapR<const K*>::err(HashMapE::ProbeSequenceExhausted);
    }

    template <typename K, typename V>
    /**
     * @brief Grows or initializes raw map storage and reinserts live entries.
     * @return Ok(true) when new storage is allocated, Ok(false) when no growth
     *         is needed, or Err(HashMapE) on failure.
     */
    inline HashMapR<bool> reserve_with_rehash_map(
      Arena& arena,
      u64    requested_capacity,
      u64&   io_capacity,
      u64&   io_count,
      K*&    io_keys,
      V*&    io_values,
      u64*&  io_hashes,
      u8*&   io_ctrl
    ) {
      u64 target_capacity = normalize_capacity(requested_capacity);
      if (io_capacity >= target_capacity) {
        return HashMapR<bool>::ok(false);
      }

      u64 snapshot = arena.offset;

      K* new_keys   = arena.alloc_array<K>(target_capacity);
      V* new_values = arena.alloc_array<V>(target_capacity);
      u64* new_hashes = arena.alloc_array<u64>(target_capacity);
      u8* new_ctrl  = arena.alloc_array<u8>(target_capacity);

      if (!new_keys || !new_ctrl || !new_values || !new_hashes) {
        arena.offset = snapshot;
        return HashMapR<bool>::err(HashMapE::OutOfMemory);
      }

      memset(new_ctrl, CTRL_EMPTY, static_cast<size_t>(target_capacity));

      u64 new_count = 0;
      for (u64 i = 0; i < io_capacity; ++i) {
        if (io_ctrl && io_ctrl[i] == CTRL_FULL) {
          auto ins = robin_hood_upsert_raw_map(
            new_keys, new_values, new_hashes, new_ctrl, target_capacity, new_count, io_keys[i], io_values[i]
          );
          if (!ins.is_ok()) {
            arena.offset = snapshot;
            return HashMapR<bool>::err(ins.error);
          }
        }
      }

      BASE_ASSERT(new_count == io_count);

      io_keys     = new_keys;
      io_values   = new_values;
      io_hashes   = new_hashes;
      io_ctrl     = new_ctrl;
      io_capacity = target_capacity;
      io_count    = new_count;

      return HashMapR<bool>::ok(true);
    }

    template <typename K>
    /**
     * @brief Grows or initializes raw set storage and reinserts live entries.
     * @return Ok(true) when new storage is allocated, Ok(false) when no growth
     *         is needed, or Err(HashMapE) on failure.
     */
    inline HashMapR<bool> reserve_with_rehash_set(
      Arena& arena,
      u64 requested_capacity,
      u64& io_capacity,
      u64& io_count,
      K*& io_keys,
      u64*& io_hashes,
      u8*& io_ctrl
    ) {
      u64 target_capacity = normalize_capacity(requested_capacity);

      if (io_capacity >= target_capacity) {
        return HashMapR<bool>::ok(false);
      }

      u64 snapshot = arena.offset;

      K* new_keys = arena.alloc_array<K>(target_capacity);
      u64* new_hashes = arena.alloc_array<u64>(target_capacity);
      u8* new_ctrl = arena.alloc_array<u8>(target_capacity);

      if (!new_keys || !new_ctrl || !new_hashes) {
        arena.offset = snapshot;
        return HashMapR<bool>::err(HashMapE::OutOfMemory);
      }

      memset(new_ctrl, CTRL_EMPTY, static_cast<size_t>(target_capacity));

      u64 new_count = 0;
      for (u64 i = 0; i < io_capacity; ++i) {
        if (io_ctrl && io_ctrl[i] == CTRL_FULL) {
          auto ins = robin_hood_upsert_raw_set<K>(
            new_keys, new_hashes, new_ctrl, target_capacity, new_count, io_keys[i]
          );
          if (!ins.is_ok()) {
            arena.offset = snapshot;
            return HashMapR<bool>::err(ins.error);
          }
        }
      }

      BASE_ASSERT(new_count == io_count);

      io_keys = new_keys;
      io_hashes = new_hashes;
      io_ctrl = new_ctrl;
      io_capacity = target_capacity;
      io_count = new_count;

      return HashMapR<bool>::ok(true);
    }

  }   // namespace detail

  template <typename K, typename V>
  /**
   * @brief Arena-backed hash map using Robin Hood open addressing.
   * @tparam K Key type.
   * @tparam V Value type.
   * @note Intended primarily for trivially copyable key/value workloads.
   */
  struct HashMap {
    /** @brief Number of allocated slots; always a power of two when non-zero. */
    u64 capacity;
    /** @brief Number of live entries currently stored in the table. */
    u64 count;
    /** @brief Key array indexed in lockstep with values, hashes, and ctrl. */
    K* keys;
    /** @brief Value array indexed in lockstep with keys, hashes, and ctrl. */
    V* values;
    /** @brief Cached full hashes for resident keys. */
    u64* hashes;
    /** @brief Occupancy control bytes for each slot. */
    u8* ctrl;

    /** @brief Constructs an empty, uninitialized map handle. */
    constexpr HashMap()
      : capacity(0), count(0), keys(nullptr), values(nullptr), hashes(nullptr), ctrl(nullptr) {}

    /** @brief Returns the minimum supported capacity for a live table. */
    static constexpr u64 min_capacity() {
      return detail::MIN_CAPACITY;
    }

    /** @brief Returns the largest allowed live-entry count before growth. */
    static constexpr u64 max_count_for_capacity(u64 cap) {
      return detail::max_count_for_capacity(cap);
    }

    /** @brief Reports whether backing storage has been initialized. */
    constexpr bool is_initialized() const {
      return this->capacity != 0;
    }

    /** @brief Reports whether the table currently stores no live entries. */
    constexpr bool empty() const {
      return this->count == 0;
    }

    /** @brief Sentinel slot value used to represent end-of-iteration. */
    static constexpr u64 npos() {
      return max_u64;
    }

    /** @brief Returns true when slot equals the end sentinel. */
    constexpr bool is_end_slot(u64 slot) const {
      return slot == npos();
    }

    /**
     * @brief Returns the first live slot index, or npos() if none exist.
     */
    inline u64 first_live_slot() const {
      if (this->capacity == 0 || this->ctrl == nullptr) {
        return npos();
      }
      for(u64 i = 0; i < this->capacity; ++i) {
        if (this->ctrl[i] == detail::CTRL_FULL) {
          return i;
        }
      }
      return npos();
    }

    /**
     * @brief Returns the next live slot after \\p slot, or npos() at end.
     */
    inline u64 next_live_slot(u64 slot) const {
      if (this->capacity == 0 || this->ctrl == nullptr || slot >= this->capacity) {
        return npos();
      }
      for (u64 i = slot + 1; i < this->capacity; ++i) {
        if (this->ctrl[i] == detail::CTRL_FULL) {
          return i;
        }
      }
      return npos();
    }

    /** @brief Returns mutable key pointer for a live slot. */
    inline K* key_at(u64 slot) {
      BASE_ASSERT(slot < this->capacity);
      BASE_ASSERT(this->ctrl[slot] == detail::CTRL_FULL);
      return &this->keys[slot];
    }

    /** @brief Returns const key pointer for a live slot. */
    inline const K* key_at(u64 slot) const {
      BASE_ASSERT(slot < this->capacity);
      BASE_ASSERT(this->ctrl[slot] == detail::CTRL_FULL);
      return &this->keys[slot];
    }

    /** @brief Returns mutable value pointer for a live slot. */
    inline V* value_at(u64 slot) {
      BASE_ASSERT(slot < this->capacity);
      BASE_ASSERT(this->ctrl[slot] == detail::CTRL_FULL);
      return &this->values[slot];
    }

    /** @brief Returns const value pointer for a live slot. */
    inline const V* value_at(u64 slot) const {
      BASE_ASSERT(slot < this->capacity);
      BASE_ASSERT(this->ctrl[slot] == detail::CTRL_FULL);
      return &this->values[slot];
    }

    /**
     * @brief Initializes storage with at least the requested capacity.
     * @param arena Arena supplying table storage.
     * @param initial_capacity Requested minimum slot count.
     */
    inline HashMapR<bool> init(Arena& arena, u64 initial_capacity = detail::MIN_CAPACITY) {
      return this->reserve(arena, initial_capacity);
    }

    /**
     * @brief Ensures the table has capacity for at least requested_capacity slots.
     * @param arena Arena supplying table storage.
     * @param requested_capacity Requested minimum slot count before normalization.
     */
    inline HashMapR<bool> reserve(Arena& arena, u64 requested_capacity) {
      return detail::reserve_with_rehash_map<K,V>(
        arena, requested_capacity, 
        this->capacity, 
        this->count, 
        this->keys, 
        this->values, 
        this->hashes,
        this->ctrl
      );
    }

    /**
     * @brief Grows the table if one more insertion would exceed the load target.
     * @param arena Arena supplying new storage if growth is needed.
     */
    inline HashMapR<bool> maybe_grow_for_insert(Arena& arena) {
      if (this->capacity == 0) {
        return this->reserve(arena, detail::MIN_CAPACITY);
      }

      u64 next_count = this->count + 1;
      if (next_count <= detail::max_count_for_capacity(this->capacity)) {
        return HashMapR<bool>::ok(false);
      }

      return this->reserve(arena, this->capacity * 2);
    }

    /**
     * @brief Finds a mutable pointer to the value for key.
     * @return Ok(pointer) when found, Ok(nullptr) when absent, or Err(HashMapE)
     *         on failure.
     */
    inline HashMapR<V*> find_ptr(const K& key) {
      return detail::robin_hood_find_ptr_raw_map(
        this->keys, this->values, this->hashes, this->ctrl, this->capacity, key
      );
    }

    /**
     * @brief Finds a const pointer to the value for key.
     * @return Ok(pointer) when found, Ok(nullptr) when absent, or Err(HashMapE)
     *         on failure.
     */
    inline HashMapR<const V*> find_ptr(const K& key) const {
      return detail::robin_hood_find_ptr_raw_map_const(
        this->keys, this->values, this->hashes, this->ctrl, this->capacity, key
      );
    }

    /**
     * @brief Tests whether the table contains key.
     * @return Ok(true) when present, Ok(false) when absent, or Err(HashMapE)
     *         on failure.
     */
    inline HashMapR<bool> contains(const K& key) const {
      auto f = this->find_ptr(key);
      if (!f.is_ok()) {
        return HashMapR<bool>::err(f.error);
      }
      return HashMapR<bool>::ok(f.value != nullptr);
    }

    /**
     * @brief Inserts or overwrites one key-value pair.
     * @return Ok(true) when a new key is inserted, Ok(false) when an existing
     *         key is updated, or Err(HashMapE) on failure.
     */
    inline HashMapR<bool> upsert(Arena& arena, const K& key, const V& value) {
      auto grow = this->maybe_grow_for_insert(arena);
      if (!grow.is_ok()) {
        return HashMapR<bool>::err(grow.error);
      }
      return detail::robin_hood_upsert_raw_map(
        this->keys, this->values, this->hashes, this->ctrl, this->capacity, this->count, key, value
      );
    }

    /** @brief Marks every slot empty without releasing arena storage. */
    inline void clear() {
      if (this->ctrl && this->capacity > 0) {
        memset(this->ctrl, detail::CTRL_EMPTY, static_cast<size_t>(this->capacity));
      }
      this->count = 0;
    }

    /** @brief Returns the slot mask used for bitmask indexing. */
    constexpr u64 bucket_mask() const {
      BASE_ASSERT(detail::is_pow2_nonzero(this->capacity));
      return this->capacity - 1;
    }
  };  // struct HashMap

  template <typename K>
  /**
   * @brief Hash-set specialization storing keys only.
   * @tparam K Key type.
   */
  struct HashMap<K, void> {
    /** @brief Number of allocated slots; always a power of two when non-zero. */
    u64 capacity;
    /** @brief Number of live keys currently stored in the table. */
    u64 count;
    /** @brief Key array indexed in lockstep with hashes and ctrl. */
    K* keys;
    /** @brief Cached full hashes for resident keys. */
    u64* hashes;
    /** @brief Occupancy control bytes for each slot. */
    u8* ctrl;

    /** @brief Constructs an empty, uninitialized set handle. */
    constexpr HashMap()
      : capacity(0), count(0), keys(nullptr), hashes(nullptr), ctrl(nullptr) {}
    
    static constexpr u64 min_capacity() {
      return detail::MIN_CAPACITY;
    }

    static constexpr u64 max_count_for_capacity(u64 cap) {
      return detail::max_count_for_capacity(cap);
    }

    /** @brief Reports whether backing storage has been initialized. */
    constexpr bool is_initialized() const {
      return this->capacity != 0;
    }

    /** @brief Reports whether the table currently stores no live entries. */
    constexpr bool empty() const {
      return this->count == 0;
    }

    /** @brief Sentinel slot value used to represent end-of-iteration. */
    static constexpr u64 npos() {
      return max_u64;
    }

    /** @brief Returns true when slot equals the end sentinel. */
    constexpr bool is_end_slot(u64 slot) const {
      return slot == npos();
    }

    /**
     * @brief Returns the first live slot index, or npos() if none exist.
     */
    inline u64 first_live_slot() const {
      if (this->capacity == 0 || this->ctrl == nullptr) {
        return npos();
      }

      for (u64 i = 0; i < this->capacity; ++i) {
        if (this->ctrl[i] == detail::CTRL_FULL) {
          return i;
        }
      }

      return npos();
    }

    /**
     * @brief Returns the next live slot after \\p slot, or npos() at end.
     */
    inline u64 next_live_slot(u64 slot) const {
      if (this->capacity == 0 || this->ctrl == nullptr) {
        return npos();
      }

      if (slot >= this->capacity) {
        return npos();
      }

      for (u64 i = slot + 1; i < this->capacity; ++i) {
        if (this->ctrl[i] == detail::CTRL_FULL) {
          return i;
        }
      }

      return npos();
    }

    /** @brief Returns mutable key pointer for a live slot. */
    inline K* key_at(u64 slot) {
      BASE_ASSERT(slot < this->capacity);
      BASE_ASSERT(this->ctrl[slot] == detail::CTRL_FULL);
      return &this->keys[slot];
    }

    /** @brief Returns const key pointer for a live slot. */
    inline const K* key_at(u64 slot) const {
      BASE_ASSERT(slot < this->capacity);
      BASE_ASSERT(this->ctrl[slot] == detail::CTRL_FULL);
      return &this->keys[slot];
    }

    /**
     * @brief Initializes storage with at least the requested capacity.
     * @param arena Arena supplying table storage.
     * @param initial_capacity Requested minimum slot count.
     */
    inline HashMapR<bool> init(Arena& arena, u64 initial_capacity = detail::MIN_CAPACITY) {
      return this->reserve(arena, initial_capacity);
    }

    /**
     * @brief Ensures the set has capacity for at least requested_capacity slots.
     * @param arena Arena supplying table storage.
     * @param requested_capacity Requested minimum slot count before normalization.
     */
    inline HashMapR<bool> reserve(Arena& arena, u64 requested_capacity) {
      return detail::reserve_with_rehash_set<K>(
        arena, 
        requested_capacity, 
        this->capacity,
        this->count,
        this->keys,
        this->hashes,
        this->ctrl
      );
    }

    /**
     * @brief Grows the set if one more insertion would exceed the load target.
     * @param arena Arena supplying new storage if growth is needed.
     */
    inline HashMapR<bool> maybe_grow_for_insert(Arena& arena) {
      if (this->capacity == 0) {
        return this->reserve(arena, detail::MIN_CAPACITY);
      }

      u64 next_count = this->count + 1;
      if (next_count <= detail::max_count_for_capacity(this->capacity)) {
        return HashMapR<bool>::ok(false);
      }

      return this->reserve(arena, this->capacity * 2);
    }

    /**
     * @brief Finds a mutable pointer to the resident key.
     * @return Ok(pointer) when found, Ok(nullptr) when absent, or Err(HashMapE)
     *         on failure.
     */
    inline HashMapR<K*> find_key_ptr(const K& key) {
      return detail::robin_hood_find_key_ptr_raw_set<K>(
        this->keys, this->hashes, this->ctrl, this->capacity, key
      );
    }

    /**
     * @brief Finds a const pointer to the resident key.
     * @return Ok(pointer) when found, Ok(nullptr) when absent, or Err(HashMapE)
     *         on failure.
     */
    inline HashMapR<const K*> find_key_ptr(const K& key) const {
      return detail::robin_hood_find_key_ptr_raw_set_const<K>(
        this->keys, this->hashes, this->ctrl, this->capacity, key
      );
    }

    /**
     * @brief Finds a mutable pointer to the resident key.
     * @return Ok(pointer) when found, Ok(nullptr) when absent, or Err(HashMapE)
     *         on failure.
     */
    inline HashMapR<K*> find_ptr(const K& key) {
      return this->find_key_ptr(key);
    }

    /**
     * @brief Finds a const pointer to the resident key.
     * @return Ok(pointer) when found, Ok(nullptr) when absent, or Err(HashMapE)
     *         on failure.
     */
    inline HashMapR<const K*> find_ptr(const K& key) const {
      return this->find_key_ptr(key);
    }

    /**
     * @brief Tests whether the set contains key.
     * @return Ok(true) when present, Ok(false) when absent, or Err(HashMapE)
     *         on failure.
     */
    inline HashMapR<bool> contains(const K& key) const {
      auto f = this->find_ptr(key);
      if (!f.is_ok()) {
        return HashMapR<bool>::err(f.error);
      }
      return HashMapR<bool>::ok(f.value != nullptr);
    }

    /**
     * @brief Inserts key if absent.
     * @return Ok(true) when a new key is inserted, Ok(false) when the key is
     *         already present, or Err(HashMapE) on failure.
     */
    inline HashMapR<bool> upsert(Arena& arena, const K& key) {
      auto grow = this->maybe_grow_for_insert(arena);
      if (!grow.is_ok()) {
        return HashMapR<bool>::err(grow.error);
      }

      return detail::robin_hood_upsert_raw_set<K>(
        this->keys, this->hashes, this->ctrl, this->capacity, this->count, key
      );
    }

    /** @brief Marks every slot empty without releasing arena storage. */
    inline void clear() {
      if (this->ctrl && this->capacity > 0) {
        memset(this->ctrl, detail::CTRL_EMPTY, static_cast<size_t>(this->capacity));
      }
      this->count = 0;
    }

    /** @brief Returns the slot mask used for bitmask indexing. */
    constexpr u64 bucket_mask() const {
      BASE_ASSERT(detail::is_pow2_nonzero(this->capacity));
      return this->capacity - 1;
    }

  };    // struct HashMap<K,void>

  /** @brief Convenience alias for key-only set usage. */
  template <typename K>
  using HashSet = HashMap<K, void>;

}     // namespace base::hashmap

export namespace base {
  using hashmap::HashMap;
  using hashmap::HashMapE;
  using hashmap::HashMapR;
  using hashmap::HashSet;
}