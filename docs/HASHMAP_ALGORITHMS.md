# HashMap Algorithms: Robin Hood Hashing, Arena Allocation, and a HashSet for Free

This document explains the algorithms used in this project's hash map and its set variant. It is written as a standalone guide, not as API reference.

The goal is to explain not just what the code does, but why this particular design is a good fit for a low-level, arena-backed Advent of Code codebase.

## The Problem We Want to Solve

We want a hash container with these properties:

- fast lookups and inserts;
- no per-node heap allocation;
- predictable memory layout;
- compatibility with the project's arena allocator;
- support for both key-value maps and key-only sets;
- explicit error handling through `Result<E,T>`.

That combination points strongly toward open addressing rather than separate chaining.

In a separate-chaining table, each bucket points to a linked structure or small side container. That makes deletion easier, but it also introduces pointer chasing, branchy traversals, and more allocator pressure. Those are all costs we would rather avoid in this codebase.

Instead, this project uses an open-addressed table with Robin Hood insertion.

## The High-Level Shape

At a high level, the table is four parallel arrays:

- keys;
- values;
- cached full hashes;
- control bytes describing whether a slot is empty or full.

The set variant is the same table without the values array.

That means the set is not a separate algorithm. It is the same probing strategy with one column removed.

## Why Open Addressing Fits Arena Allocation

Arena allocators are excellent when you want:

- a small number of large allocations;
- no per-object frees;
- simple ownership rules;
- bulk reset at a larger lifetime boundary.

Open addressing fits that model naturally.

When the table grows, we do not mutate individual entry ownership. We allocate a new, larger group of arrays from the arena and reinsert the live entries into that new storage. The old storage remains in the arena until the arena itself is reset or freed.

That gives us a simple rule:

- growth is allocate-and-rehash, never in-place node surgery.

This is a good trade when the arena lifetime already matches the workload lifetime.

## Power-of-Two Capacity and Bitmask Indexing

The table capacity is always normalized to a power of two.

That gives an efficient initial bucket computation:

```text
index = hash & (capacity - 1)
```

This is equivalent to modulo when capacity is a power of two, but it is cheaper and it simplifies wraparound arithmetic during probing.

The same trick is used to compute probe distance and to advance to the next slot.

## Open Addressing Refresher

In open addressing, all entries live inside the table itself.

To insert a key:

1. Compute its hash.
2. Convert that hash into a home slot.
3. If the slot is occupied, probe to another slot.
4. Continue until you either find the key or find a place to store it.

To look up a key:

1. Compute the same hash.
2. Start from the same home slot.
3. Walk the same probe sequence until you find the key or prove it is absent.

The interesting question is what kind of probing policy to use.

## Why Robin Hood Hashing

This table uses Robin Hood hashing.

The central idea is simple:

- when two entries compete for a slot, the entry that is farther from its home position gets priority.

If the incoming entry has probed farther than the resident entry, the incoming one steals the slot and the resident entry continues probing.

That sounds aggressive, but it has a useful effect: it reduces variance in probe lengths.

Instead of letting some unlucky keys drift very far while other keys stay close to home, Robin Hood hashing tends to equalize displacement across entries. In practice that gives more stable lookup behavior than plain linear probing at similar load factors.

This is the source of the name: keys that are “poor” in probe distance steal from “rich” keys that are still close to home.

## Probe Distance

Every occupied entry has:

- a home slot determined by its hash;
- a current slot where it actually lives;
- a probe distance, which is how far it had to move from home.

With power-of-two capacity, wrapped distance is computed as:

```text
distance(slot, home) = (slot + capacity - home) & (capacity - 1)
```

That formula is cheap and handles wraparound correctly.

## Insertion Step by Step

The insertion algorithm used by the map version works like this:

1. Compute the incoming key's full hash.
2. Start at its home slot.
3. If the slot is empty, store key, value, and cached hash there.
4. If the slot contains the same key, overwrite the value.
5. Otherwise compare probe distances.
6. If the resident entry is closer to home than the incoming entry, swap them.
7. Continue probing with the displaced entry.

The set variant is the same algorithm, except there is no value to store or swap.

One way to think about it is that insertion carries a moving candidate entry through the table until it finally finds its resting place.

## Early Exit During Lookup

Robin Hood hashing gives a useful lookup optimization.

Suppose you are searching for a key at probe distance $d$. If you arrive at a resident entry whose probe distance is smaller than $d$, the searched key cannot appear later in the probe sequence.

Why?

Because if that key had existed, Robin Hood insertion would have forced it to steal from the smaller-distance resident earlier.

So lookup can stop early in two cases:

1. it hits an empty slot;
2. it reaches a resident entry whose displacement is smaller than the current search displacement.

That is one of the cleanest benefits of Robin Hood probing: insertion policy creates stronger lookup invariants.

## Why We Cache Full Hashes

The first version of the table recomputed resident hashes during probing.

That is acceptable for tiny integer keys, but it is a poor trade for larger or more expensive key types like `Str8`. During insert or lookup, a long probe chain could repeatedly re-hash the same resident keys.

The current design caches the full hash for every occupied slot.

That helps in three ways:

1. the resident home slot is derived from cached hash, not recomputed from key bytes;
2. equality checks can use hash equality as a cheap prefilter before key comparison;
3. rehash during growth can reuse the stored key with one normal insertion path.

This is not the most compressed metadata design possible, but it is a strong performance improvement for a small complexity increase.

## Why Separate Arrays

The table stores keys, values, hashes, and control bytes in separate arrays instead of one array of large slot structs.

There are trade-offs here.

Advantages:

- clear, regular layout;
- easy omission of values in the set specialization;
- cheap scans over control bytes or hashes in future optimizations;
- fewer mixed-field loads when only metadata is needed.

Costs:

- multiple parallel arrays must stay in lockstep;
- insertion and swap logic must update several arrays together.

For this project, the design is a good fit because it keeps the set specialization simple and leaves room for future metadata-focused optimizations.

## Why the Set Is `HashMap<K, void>`

Instead of writing a second independent hash-set implementation, the project uses a partial specialization:

- `HashMap<K, V>` stores keys and values;
- `HashMap<K, void>` stores only keys.

That means the set variant inherits:

- the same hashing traits;
- the same probe strategy;
- the same growth rules;
- the same error model.

The only major difference is that successful lookup returns a key pointer instead of a value pointer, and insertion does not carry or overwrite a mapped value.

This keeps behavior aligned across the two container shapes.

## Load Factor Choice

The current implementation targets an 80% load factor.

That is a practical middle ground:

- lower load factors waste memory but reduce probe lengths;
- higher load factors improve memory efficiency but can lengthen probe chains.

Robin Hood hashing usually tolerates moderately high load factors better than naive linear probing, but there is still a real cost as occupancy rises. Around 80% is a common compromise for a general-purpose fast table that is not yet using more advanced metadata tricks.

## Growth by Rehash

When the next insertion would exceed the load threshold, the table grows.

The growth flow is:

1. choose a larger power-of-two capacity;
2. allocate fresh arrays from the arena;
3. initialize all control bytes to empty;
4. reinsert every live entry into the new storage;
5. publish the new arrays.

If allocation or reinsertion fails, the code restores the arena offset snapshot so a failed growth attempt does not silently leak arena space.

That rollback behavior matters because arena allocators are intentionally simple. If a failed reserve advanced the arena cursor permanently, even an error return would still damage future allocation capacity.

## Failure Model

The container reports failures through:

- `HashMapE`, the error enum;
- `HashMapR<T>`, the result alias.

That matches the broader project style:

- no exceptions;
- explicit success and failure states;
- lightweight error categories.

The two main operational failures today are:

- out-of-memory during reserve or growth;
- probe exhaustion, which should be exceptional if the table invariants hold.

## Usage Examples

The following examples show the intended calling style for the current API.

They are not trying to hide the low-level nature of the container. The design is explicit on purpose:

- callers provide an arena;
- callers handle `HashMapR<T>` directly;
- lookups return pointers rather than copies.

### Example: `HashMap<u64, u64>`

This is the simplest map case: integer keys and integer values.

```cpp
import core_types;
import core_memory;
import core_hash_map;

using namespace base;

void example_map() {
	Arena arena = {};
	bool ok = arena.init(MB(1));
	BASE_ASSERT(ok);

	HashMap<u64, u64> counts = {};

	auto init_r = counts.init(arena, 32);
	BASE_ASSERT(init_r.is_ok());

	auto ins_r = counts.upsert(arena, 42, 1);
	BASE_ASSERT(ins_r.is_ok());

	auto overwrite_r = counts.upsert(arena, 42, 99);
	BASE_ASSERT(overwrite_r.is_ok());
	BASE_ASSERT(overwrite_r.value == false); // existing key was updated

	auto find_r = counts.find_ptr(42);
	BASE_ASSERT(find_r.is_ok());
	BASE_ASSERT(find_r.value != nullptr);
	BASE_ASSERT(*find_r.value == 99);

	auto contains_r = counts.contains(7);
	BASE_ASSERT(contains_r.is_ok());
	BASE_ASSERT(contains_r.value == false);

	arena.free();
}
```

### Example: count-with-default pattern

Because lookup returns pointers, a common pattern is:

1. look up the key;
2. mutate in place if present;
3. insert if absent.

```cpp
import core_types;
import core_memory;
import core_hash_map;

using namespace base;

void bump_count(Arena& arena, HashMap<u64, u64>& counts, u64 key) {
	auto find_r = counts.find_ptr(key);
	BASE_ASSERT(find_r.is_ok());

	if (find_r.value) {
		*find_r.value += 1;
		return;
	}

	auto ins_r = counts.upsert(arena, key, 1);
	BASE_ASSERT(ins_r.is_ok());
}
```

This is one of the main reasons the current API exposes pointer-based lookup rather than only returning copied values.

### Example: `HashSet<u64>` via `HashMap<u64, void>`

The set specialization uses the same table but stores keys only.

```cpp
import core_types;
import core_memory;
import core_hash_map;

using namespace base;

void example_set() {
	Arena arena = {};
	bool ok = arena.init(MB(1));
	BASE_ASSERT(ok);

	HashSet<u64> seen = {};

	auto init_r = seen.init(arena, 64);
	BASE_ASSERT(init_r.is_ok());

	auto first_r = seen.upsert(arena, 1234);
	BASE_ASSERT(first_r.is_ok());
	BASE_ASSERT(first_r.value == true);

	auto second_r = seen.upsert(arena, 1234);
	BASE_ASSERT(second_r.is_ok());
	BASE_ASSERT(second_r.value == false); // already present

	auto contains_r = seen.contains(1234);
	BASE_ASSERT(contains_r.is_ok());
	BASE_ASSERT(contains_r.value == true);

	arena.free();
}
```

### Example: `HashMap<Str8, u64>`

String-slice keys are supported through the project's `HashTraits<Str8>` specialization.

The main thing to remember is that `Str8` is a non-owning view. The bytes referenced by the key must outlive the table entry.

```cpp
import core_types;
import core_memory;
import core_string;
import core_hash_map;

using namespace base;
using namespace base::str;

void example_str8_map(Arena& arena) {
	HashMap<Str8, u64> word_counts = {};

	auto init_r = word_counts.init(arena, 16);
	BASE_ASSERT(init_r.is_ok());

	Str8 hello = Str8::from_cstr("hello");
	auto ins_r = word_counts.upsert(arena, hello, 1);
	BASE_ASSERT(ins_r.is_ok());

	auto find_r = word_counts.find_ptr(Str8::from_cstr("hello"));
	BASE_ASSERT(find_r.is_ok());
	BASE_ASSERT(find_r.value != nullptr);
}
```

In real parsing code, `Str8` keys often point into input buffers that already live for the duration of the table, which makes this a good fit for arena-scoped workloads.

### Example: custom key type via `HashTraits<MyType>`

If your key type is not one of the built-in specializations, add a `HashTraits<T>` specialization in `base::hash`.

The key rule is consistency:

- if `eq(a, b)` is true, `hash(a)` and `hash(b)` must be the same.

```cpp
import core_types;
import core_hash;
import core_hash_map;
import core_memory;

using namespace base;

struct Point {
	u32 x;
	u32 y;
};

namespace base::hash {
	template <>
	struct HashTraits<Point> {
		static constexpr u64 hash(const Point& p) {
			// Pack x/y and run the same integer mixing path used elsewhere.
			u64 packed = (static_cast<u64>(p.x) << 32) | static_cast<u64>(p.y);
			return detail::mix64(packed);
		}

		static constexpr bool eq(const Point& a, const Point& b) {
			return a.x == b.x && a.y == b.y;
		}
	};
}

void example_custom_key(Arena& arena) {
	HashMap<Point, u64> visits = {};
	auto init_r = visits.init(arena, 64);
	BASE_ASSERT(init_r.is_ok());

	Point p{10, 20};
	auto upsert_r = visits.upsert(arena, p, 1);
	BASE_ASSERT(upsert_r.is_ok());

	auto find_r = visits.find_ptr(Point{10, 20});
	BASE_ASSERT(find_r.is_ok());
	BASE_ASSERT(find_r.value != nullptr);
}
```

For set-style usage, the same specialization works unchanged with `HashSet<Point>`.

### Example: reserve when you know approximate size

If the caller can estimate the number of inserts ahead of time, reserving early reduces rehash frequency.

```cpp
import core_types;
import core_memory;
import core_hash_map;

using namespace base;

void example_reserve(Arena& arena, u64 expected_items) {
	HashMap<u64, u64> table = {};

	auto reserve_r = table.reserve(arena, expected_items * 2);
	BASE_ASSERT(reserve_r.is_ok());
}
```

The exact requested number does not need to be a power of two. The implementation normalizes it internally.

### Example: clear without releasing storage

`clear()` resets occupancy but does not return memory to the arena.

```cpp
import core_types;
import core_memory;
import core_hash_map;

using namespace base;

void example_clear(Arena& arena) {
	HashSet<u64> set = {};
	auto init_r = set.init(arena, 32);
	BASE_ASSERT(init_r.is_ok());

	auto ins_r = set.upsert(arena, 7);
	BASE_ASSERT(ins_r.is_ok());

	set.clear();
	BASE_ASSERT(set.empty());
}
```

That is useful when the table object is reused repeatedly inside a larger arena lifetime.

### Example: slot-based traversal (map and set)

The container now exposes a minimal, index-based traversal surface:

- `first_live_slot()`
- `next_live_slot(slot)`
- `npos()`
- `is_end_slot(slot)`
- `key_at(slot)`
- `value_at(slot)` (map only)

This keeps iteration explicit and close to the underlying table representation.

Map traversal:

```cpp
for (u64 slot = map.first_live_slot();
		 !map.is_end_slot(slot);
		 slot = map.next_live_slot(slot)) {
	const K* key = map.key_at(slot);
	const V* value = map.value_at(slot);
	// use *key, *value
}
```

Set traversal:

```cpp
for (u64 slot = set.first_live_slot();
		 !set.is_end_slot(slot);
		 slot = set.next_live_slot(slot)) {
	const K* key = set.key_at(slot);
	// use *key
}
```

This style avoids heavyweight iterator abstractions while still preventing call sites from depending directly on raw control-byte checks.

## What the Current Design Does Not Yet Do

The table is intentionally focused.

It does not yet implement:

- erase;
- tombstones;
- SIMD-friendly metadata groups;
- heterogeneous lookup;

That is a good thing for now. The implementation is still small enough to understand directly, and the current decisions are visible rather than buried under layers of generalization.

## How This Relates to Other Modern Hash Tables

This table is not trying to be a clone of SwissTable, Folly F14, or other industrial hash tables.

Those designs push harder on:

- packed metadata fingerprints;
- group probing;
- SIMD-accelerated negative lookup filtering;
- deeper engineering for deletion and iterator stability.

The current project takes a simpler step first:

- Robin Hood open addressing;
- cached full hashes;
- arena-friendly growth;
- a direct set specialization.

That keeps the implementation teachable while still achieving most of the big structural wins over naive chaining or naive linear probing.

## Suggested References

These are useful references if you want to go deeper on the ideas behind this implementation.

### Papers and classic references

1. Pedro Celis, Per-Ake Larson, and J. Ian Munro, “Robin Hood Hashing.”
- The foundational reference for the technique used here.

2. Donald Knuth, *The Art of Computer Programming*, Volume 3: *Sorting and Searching*.
- Still one of the best references for open addressing, probe behavior, and hash-table tradeoffs.

3. Thomas H. Cormen, Charles E. Leiserson, Ronald L. Rivest, and Clifford Stein, *Introduction to Algorithms*.
- Good general background on hashing and table load-factor behavior.

### Blog posts and engineering writeups

1. Emmanuel Goossaert, “Robin Hood hashing.”
- A very approachable explanation of the insertion and search invariants.

2. Sebastian Sylvan, writing on Robin Hood hashing and cache-friendly tables.
- Helpful for intuition about variance reduction and real-world behavior.

3. Abseil's SwissTable design notes and related talks.
- Useful as a contrast case for where metadata-heavy high-performance tables go next.

4. Malte Skarupke's blog posts on fast hash tables.
- Good practical perspective on engineering tradeoffs in modern C++ hash containers.

### Videos and talks

1. Conference talks on SwissTable and Abseil containers.
- Useful for understanding modern metadata-driven probing.

2. Talks on data-oriented design and cache-aware container layout.
- Helpful for understanding why this project prefers contiguous open-addressed storage.

## Final Takeaway

This project's hash map is built around a simple idea:

- keep the data contiguous,
- keep ownership arena-friendly,
- use Robin Hood hashing to stabilize probe lengths,
- cache hashes so string keys are not punished during probing,
- and get the set variant almost for free through specialization.

That combination is small enough to study, strong enough to be useful, and flexible enough to improve later if benchmarks justify more advanced metadata techniques.