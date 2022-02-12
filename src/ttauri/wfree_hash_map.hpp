// Copyright Take Vos 2022.
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at https://www.boost.org/LICENSE_1_0.txt)

#pragma once

#include "../aasert.hpp"
#include "../memory.hpp"
#include <atomic>
#include <cstdint>
#include <cstddef>

namespace tt::inline v1 {

namespace detail {

template<typename K, typename V>
struct wfree_hash_map_item {
    using key_type = K;
    using value_type = V;

    key_type key;
    value_type value;

    wfree_hash_map_item(wree_hash_map_item const &) = delete;
    wfree_hash_map_item(wree_hash_map_item &&) = delete;
    wfree_hash_map_item &operator=(wree_hash_map_item const &) = delete;
    wfree_hash_map_item &operator=(wree_hash_map_item &&) = delete;

    template<is_forward_for<key_type> Key, is_forward_for<value_type> Value>
    constexpr wfree_hash_map_item(Key &&key, Value &&value) noexcept :
        key(std::forward<Key>(key)), value(std::forward<Value>(value))
    {
    }
};

template<typename K, typename V>
struct wfree_hash_map_slot {
    using key_type = K;
    using value_type = V;
    using item_type = wfree_hash_map_item<key_type, value_type>;

    using enum commit_type { move = 0, normal = 1, set = 2 };

    /** The state of the slot.
     *
     * Values:
     *  -               0: empty
     *  -               1: reserved
     *  - <generation>010: Tombstone with the generation when it was created.
     *  -             011: -- NEVER USE --
     *  -       <hash>100: `move()`-commit.
     *  -       <hash>101: normal-commit.
     *  -       <hash>110: `set()`-commit.
     *  -       <hash>111:
     */
    std::atomic<state_type> _state;
    alignas(item_type) std::array<char, sizeof(item_type)> _buffer;

    ~wfree_hash_map_slot() noexcept
    {
        tt_axiom(_hash.load(std::memory_order::relaxed) != 1);

        // If the slot isn't empty destroy the item.
        if (_hash.load(std::memory_order::acquire) != 0) {
            destroy();
        }
    }

    wfree_hash_map_slot(wree_hash_map_slot const &) = delete;
    wfree_hash_map_slot(wree_hash_map_slot &&) = delete;
    wfree_hash_map_slot &operator=(wree_hash_map_slot const &) = delete;
    wfree_hash_map_slot &operator=(wree_hash_map_slot &&) = delete;
    constexpr wfree_hash_map_slot() noexcept = default;

    [[nodiscard]] uint64_t state() const noexcept
    {
        return _state.load(std::memory_order::relaxed);
    }

    [[nodiscard]] item_type *item_ptr() noexcept
    {
        return reinterpret_cast<item_type *>(_buffer.data());
    }

    [[nodiscard]] item_type &item() noexcept
    {
        ttlet ptr = item_ptr();
        tt_axiom(ptr != nullptr);
        return *ptr;
    }

    /** Get a reference to the key of the slot.
     */
    [[nodiscard]] key_type const &key() noexcept
    {
        return item().key;
    }

    /** Get a reference to the value of the slot.
     */
    [[nodiscard]] value_type const &value() noexcept
    {
        return item().value;
    }

    /** Reserve the slot.
     *
     * @note It is undefined behavior when this is not a tombstone or empty.
     * @param state The loaded state.
     * @return 1 if the reserve was successful, or the new state on race condition.
     */
    [[nodiscard]] bool reserve(uint64_t state) noexcept
    {
        tt_axiom(state == 0 or (state & 0b110) == 0b010); // empty or tombstone.
        if (_hash.compare_exchange_strong(state, 1, std::memory_order::acquire, std::memory_order::relaxed)) {
            return 1;
        } else {
            return state;
        }
    }

    template<bool DestroyItem>
    [[nodiscard]] void clear() noexcept
    {
        if constexpr (DestroyItem) {
            destroy();
        }

        _hash.store(0, std::memory_order::release);
    }

    /** reclaim the slot and mark it empty.
     *
     * @note It is undefined behavior when this is not a tombstone.
     * @post If successfully reclaimed, the stored key/value is destructed.
     * @param state The value that was loaded during processing of slots.
     * @return True when the slot was marked empty.
     */
    bool reclaim(uint64_t state) noexcept
    {
        tt_axiom((state & 0b110) == 0b010); // tombstone.
        auto reserved = reserve(state);
        if (reserved) {
            clear<true>();
        }
        return reserved;
    }

    /** Check if this is an old tombstone.
     *
     * @return True if this is an old tombstone.
     */
    [[nodiscard]] constexpr static bool is_old_tombstone(uint64_t state, uint64_t generation) noexcept
    {
        state ^= 0b010;
        state = std::rotr(state, 3);
        // If not a tombstone, one of the three top bits will be set and the value will be larger than current generation.
        return state < generation;
    }

    /** Set the slot of tombstone.
     *
     * @note It is undefined behavior when old_value is not used or reserved.
     * @param state The state that was loaded during processing of slots.
     * @param generation The generation value to store.
     * @return True when the slot was tombstoned.
     */
    [[nodiscard]] bool tombstone(uint64_t state, uint64_t generation) noexcept
    {
        tt_axiom(old_value == reserved or (old_value & 0b100));
        tt_axiom(generation <= 0x1fff'ffff'ffff'ffff);
        generation <<= 3;
        generation |= 0b010;
        return _hash.compare_exchange_strong(old_value, generation, std::memory_order::relaxed, std::memory_order::relaxed);
    }

    /** Commit the slot from the `set()` function.
     *
     * @note It is undefined behavior if the slot is not reserved.
     * @tparam CommitType The commit type to use.
     * @param hash The hash value to use.
     */
    template<commit_type CommitType>
    void commit(uint64_t hash) noexcept
    {
        static_assert(State >= 0b100 and State <= 0b111);

        hash <<= 3;
        hash |= to_underlying(CommitType);
        _hash.store(hash, std::memory_order_release);
    }

    /** Destroy the key/value in the slot.
     */
    void destroy() noexcept
    {
        std::destroy_at(item_ptr());
    }

    /** Create a key/value in the slot.
     *
     * This constructs an key/value pair in the slot's buffer.
     *
     * @note It is undefined behavior to emplace into a slot where `reserve()` returned false.
     * @pre `reserve()` must be called before this function.
     * @param key The key to store.
     * @param value The value to store.
     */
    template<forward_for<key_type> Key, forward_for<value_type> Value>
    void emplace(Key &&key, Value &&value) noexcept
    {
        std::construct_at(item_ptr(), std::forward<Key>(key), std::forward<Value>(value));
    }
};

template<typename K, typename V, typename KeyEqual>
struct wfree_hash_map_table {
    using key_type = K;
    using value_type = V;
    using slot_type = wfree_hash_map_slot<key_type, value_type>;
    using key_equal = KeyEqual;
    using commit_type = slot_type::commit_type;

    constexpr size_t data_offset = tt::ceil(sizeof(wfree_hash_map_header), sizeof(slot_type));
    constexpr size_t slot_offset = data_offset / sizeof(slot_type);

    slot_type const *_end;
    size_t const capacity;

    std::atomic<size_t> num_reservations;
    std::atomic<size_t> num_tombstones;
    std::atomic<size_t> clean_index;

    wfree_hash_map_table(wfree_hash_map_table const &) = delete;
    wfree_hash_map_table(wfree_hash_map_table &&) = delete;
    wfree_hash_map_table &operator=(wfree_hash_map_table const &) = delete;
    wfree_hash_map_table &operator=(wfree_hash_map_table &&) = delete;
    wfree_hash_map_table(size_t capacity) noexcept :
        capacity(capacity), num_reservations(0), num_tombstones(0), clean_index(capacity + 1), use_count(0)
    {
    }

    ~wfree_hash_map_table() noexcept
    {
        std::destroy(begin(), end());
    }

    wfree_hash_map_table(size_t capacity) noexcept : wfree_hash_map_header(capacity), _end(begin() + capacity)
    {
        for (auto it = begin(); it != end(); ++it) {
            std::construct_at(it);
        }
    }

    void increment_reservations() noexcept
    {
        num_reservations.fetch_add(1, std::memory_order::relaxed);
    }

    void decrement_reservations() noexcept
    {
        num_reservations.fetch_sub(1, std::memory_order::relaxed);
    }

    void increment_tombstones() noexcept
    {
        num_tombstones.fetch_add(1, std::memory_order::relaxed);
    }

    void decrement_tombstones() noexcept
    {
        num_tombstones.fetch_sub(1, std::memory_order::relaxed);
    }

    void size_t hash_to_index(uint64_t hash) noexcept
    {
        // Top three bits must be zero.
        tt_axiom((hash >> 61) == 0);
        return static_cast<size_t>(index % capacity);
    }

    slot_type *begin() noexcept
    {
        return reinterpret_cast<slot_type *>(this) + slot_offset;
    }

    slot_type *end() noexcept
    {
        return _end;
    }

    [[nodiscard]] slot_type *increment_slot(slot_type *it) noexcept
    {
        return ++it == end() ? begin() : it;
    }

    [[nodiscard]] slot_type *decrement_slot(slot_type *it) noexcept
    {
        return it-- == begin() ? end() - 1 : it;
    }

    [[nodiscard]] slot_type *first_slot(size_t hash) noexcept
    {
        return begin() + hash_to_index(hash);
    }

    [[nodiscard]] slot_type *last_slot(slot_type *it) noexcept
    {
        while (it->hash()) {
            it = increment_slot(it);
        }
        return it;
    }

    /** Search for the key in the table.
     *
     * @param key The key to search for.
     * @param hash The hash of the key.
     * @return The slot that matches the key, or nullptr
     */
    slot_type *get(key_type const &key, uint64_t hash, uint64_t generation) noexcept
    {
        ttlet commit_state = (hash << 3) | 0b100;

        slot_type *it = first_slot(hash);
        slot_type *move_match = nullptr;
        uint64_t move_state = 0;
        while (ttlet state = it->state()) {
            if (ttlet match = state ^ commit_state; match <= 3 and key_equal{}(it->key, key)) {
                if (move_match) {
                    if (move_match->tombstone(move_state, generation)) {
                        increment_tombstones();
                    }
                }

                if (match) {
                    // Found commit or `set()`-commit.
                    return it;

                } else {
                    // Found `move()`-commit.
                    move_match = it;
                    move_state = state;
                }
            }

            it = increment_slot(it);
        }
        return move_match;
    }

    /** Remove a key from the table.
     *
     * @param first The first slot matching the key.
     * @param last One beyond the last slot matching the key.
     * @param key The key to remove from the table.
     * @param hash The hash of the key lower bits must '100'.
     * @param generation The current generation. lower bits must be '010'.
     * @return The previous matching slot.
     */
    slot_type *remove(slot_type *first, slot_type *last, key_type const &key, uint64_t hash, uint64_t generation) noexcept
    {
        ttlet commit_state = (hash << 3) | 0b100;
        ttlet old_generation = generation - 3;

        auto it = last;
        auto lowest_slot = last;
        slot_type *found = nullptr;
        do {
            it = decrement_slot(it);
            ttlet state = it->state();

            if (slot_type::is_old_tombstone(state, old_generation) and it < lowest_slot and it->clear<True>(state)) {
                // Found a tombstone of more than 3 generations old.
                //
                // This works because `set` iterates forward and uses 2nd generation slots and on contention:
                // - When `set()` wins it will use the slot.
                // - When `set()` looses it will see an empty slot that it will now use.
                // - In either case there will be no other 3rd generation slots left for this thread to reclaim.
                //
                // If there is no contention with `set` it can reclaim multiple 3rd generation slots.
                decrement_tombstones();
                decrement_reservations();

            } else if (ttlet match = state ^ commit_state; match <= 1 and key_equal{}(it->key, key)) {
                // Found a matching `move()`-commit or normal-commit. IGNORE `set()`-commit.
                if (match) {
                    // Found a normal-commit.
                    // Report the first normal commit in a chain.
                    found == it;
                    if (it->set_tombstone(h, generation)) {
                        increment_tombstones();
                    }
                } else {
                    // Found a `move()`-commit.
                    // Only report the last `move()` commit in a chain, if no other commits are available.
                    if (not found) {
                        found = it;
                    }
                    if (it->set_tombstone(h, generation)) {
                        increment_tombstones();
                    }
                }

            } else if (state & 0b100) {
                // Found a used slot, check where its location was supposed to be.
                inplace_min(lowest_slot, first_slot(state >> 3));
            }
        } while (it != first);

        return found;
    }

    /** Remove a key from the table.
     *
     * @param key The key to remove from the table.
     * @param hash The hash of the key lower bits must '100'.
     * @param generation The current generation. lower bits must be '010'.
     * @return The previous matching slot.
     */
    slot_type *remove(key_type const &key, uint64_t hash, uint64_t generation) noexcept
    {
        auto first = first_slot(hash);
        auto last = last_slot(first);
        return remove(first, last, key, hash, generation);
    }

    /** Set a new key value in the table, overwriting existing.
     *
     * @tparam Try If true then set will not insert the key if it already exists.
     * @tparam Move If true then set will insert a moved key/value.
     * @param key The key to add to the table.
     * @param value The value to add to the table.
     * @param hash The hash of the key.
     * @return The previous matching slot.
     */
    template<forward_for<value_type> Value, bool Try, bool Move>
    slot_type *set(key_type const &key, Value &&value, uint64_t hash, uint64_t generation) noexcept
    {
        ttlet commit_state = (hash << 3) | 0b100;
        ttlet old_generation = generation - 2;

        ttlet first = first_slot(hash);
        auto it = first;
        while (true) {
            ttlet state = it->state();

            if (slot_type::is_old_tombstone(state, old_generation)) {
                ttlet new_state = it->reserve(state);

                // Due to race with `remove()` the tombstone may have be reclaimed and the slot has become empty.
                if (new_state == 1 or (new_state == 0 and it->reserve(new_state) == 1)) {
                    // Successfully Reserved.
                    increment_reservations();
                    break;
                }

            } else if (Try and (state ^ commit_state) <= 3 and key_equal{}(it->key, key)) {
                // Found match, don't set the value.
                return it;

            } else if (state == 0 and it->reserve(state) == 1) {
                // Successfully reserved.
                increment_reservations();
                break;
            }

            it = increment_slot(it);
        }

        // Start using the reserved slot. Commit as read-only so that it won't get removed.
        it->emplace(key, std::forward<Value>(value));

        if constexpr (Move) {
            // `move()`-commit; A very fragile insertion, may be deleted when duplicates are found.
            it->commit<commit_type::move>(hash);
            return it;

        } else {
            // `set()`-commit; can not be deleted by `remove()`, but it can be found with `get()`.
            it->commit<commit_type::set>(hash);

            // Go all the way to the end, then execute a remove to remove duplicates.
            ttlet last = last_slot(it);
            ttlet match = remove(first, last, key, hash, generation);

            // normal-commit, it may now be deleted.
            it->commit<commit_type::normal>(hash);

            return Try ? nullptr : match;
        }
    }

    /** Move an entry from another hash map table.
     *
     * @post The key/value may be added to this table and is removed from the other table.
     * @param other_table A pointer to the other table.
     * @param other_slot A slot from the other table to move.
     * @param hash The hash value of the key in the other_slot.
     * @return The slot used in the new table.
     */
    slot_type *move_from(wfree_hash_map_table *other_table, slot_type *other_slot, uint64_t hash, uint64_t generation) noexcept
    {
        tt_axiom(other_table != nullptr);
        tt_axiom(other_slot != nullptr);

        // Copy the key and value from the other slot.
        slot_type *new_slot = set<true, true>(other_slot->key, other_slot->value, hash, generation);

        // Tombstone the other slot, if it is still in committed state.
        ttlet state = other_slot->state();
        ttlet commit_state = (hash >> 3) ^ 0b100;
        if ((state ^ commit_state) <= 3) {
            if (other_slot->tombstone(state, generation)) {
                other_table->increment_tombstone();
            }
        }

        return new_slot;
    }

    /** Get the amount of bytes needed to allocate the table for a capacity.
     */
    [[nodiscard]] static size_t capacity_to_num_bytes(size_t capacity) noexcept
    {
        return capacity * sizeof(item_type) + data_offset;
    }

    /** Allocate a table.
     *
     * @post The returned table is allocated and constructed.
     * @param allocator The `char` allocator to use.
     * @param capacity The capacity of the table to allocate.
     * @return A pointer to the allocated table.
     * @throw std::bad_alloc
     */
    template<typename Allocator>
    [[nodiscard]] static wfree_hash_map_table *
    allocate(Allocator const &allocator, size_t capacity) requires(std::same_v<Allocator::value_type, char>)
    {
        static_assert(std::same_v<Allocator::value_type, char>);

        ttlet ptr = std::allocator_traits<Allocator>::allocate(allocator, capacity_to_num_bytes(capacity));
        return std::construct_at(reinterpret_cast<wfree_hash_map_table *>(ptr), capacity);
    }

    /** Deallocate a table.
     *
     * @post The given table is destroyed and deallocated.
     * @param allocator The `char` allocator to use.
     * @param table_ptr A pointer to the previously allocated table.
     */
    template<typename Allocator>
    static void deallocate(Allocator const &allocator, wfree_hash_map_table *table_ptr) noexcept
        requires(std::same_v<Allocator::value_type, char>)
    {
        tt_axiom(table_ptr != nullptr);

        ttlet capacity = table_ptr->capacity;
        std::destroy_at(table_ptr);
        std::allocator_traits<Allocator>::deallocate(
            allocator, reinterpret_cast<char *>(table_ptr), capacity_to_num_bytes(capacity));
    }
};

class wfree_hash_map_base {
public:
    /** Increment the use count.
     *
     * @return Current generation, used for reclaiming.
     *         No other thread has a generation number two lower than this value.
     */
    uint64_t increment_use_count() noexcept
    {
        if (_use_count.fetch_add(1, std::memory_order::acquire) == 0) {
            return _generation.fetch_add(1, std::memory_order::acquire);
        } else {
            return _generation.load(std::memory_order::acquire);
        }
    }

    /** Decrement the use count.
     */
    void decrement_use_count() noexcept
    {
        _use_count.fetch_sub(1, std::memory_order::release);
    }

private:
    std::atomic<uint64_t> _use_count = 0;
    std::atomic<uint64_t> _generation = 0;
};


/** A proxy object to a hash map slot.
 *
 * This object maintains a use-count with the specific hash map.
 */
template<typename S>
class wfree_hash_map_proxy {
public:
    using slot_type = S;

    /** Destruct the proxy object.
     *
     * @note decrements use count of table.
     */
    ~wfree_hash_map_proxy()
    {
        if (_hash_map) {
            _hash_map->decrement_use_count();
        }
    }

    /** Copies the proxy object.
     *
     * @note increments use count of table.
     */
    wfree_hash_map_proxy(wfree_hash_map_proxy const &other) noexcept : _hash_map(other._hash_map), _slot(other._slot)
    {
        if (_table) {
            _table->increment_use_count();
        }
    }

    /** Assigns a copy of the proxy object.
     *
     * @note decrement use count of previous table.
     * @note increments use count of table.
     */
    wfree_hash_map_proxy &operator=(wfree_hash_map_proxy const &other) noexcept
    {
        if (_hash_map != _other._hash_map) {
            if (_hash_map) {
                _hash_map->decrement_use_count();
            }
            _hash_map = other._hash_map;
            if (_hash_map) {
                _hash_map->increment_use_count();
            }
        }
        _slot = other._slot;
    }

    /** Moves the proxy object.
     */
    wfree_hash_map_proxy(wfree_hash_map_proxy &&other) noexcept :
        _hash_map(std::exchange(other._hash_map, nullptr)), _slot(std::exchange(other._slot, nullptr))
    {
    }

    /** Assigns by moving the proxy object.
     *
     * @note decrements use count of previous table.
     */
    wfree_hash_map_proxy &operator=(wfree_hash_map_proxy &&other) noexcept
    {
        if (_hash_map) {
            _hash_map->decrement_use_count();
        }
        _hash_map = std::exchange(other._hash_map, nullptr);
        _slot = std::exchange(other._slot, nullptr);
    }

    /** Construct a pointer.
     *
     * @note This constructor takes ownership of the use count of the caller.
     * @pre `increment_use_count()` must be called before.
     * @param hash_map Pointer to the hash_map.
     * @param slot The pointer to the slot.
     */
    wfree_hash_map_proxy(wfree_hash_map_base *hash_map, slot_type const *slot) noexcept : _hash_map(hash_map), _slot(slot)
    {
        tt_axiom(hash_map != nullptr);
        tt_axiom(slot != nullptr);
    }

    [[nodiscard]] bool empty() const noexcept
    {
        return _slot == nullptr;
    }

    explicit operator bool() const noexcept
    {
        return not empty();
    }

    [[nodiscard]] slot_type::key_type const &key() const noexcept
    {
        tt_axiom(_slot != nullptr);
        return _slot->key();
    }

    [[nodiscard]] slot_type::value_type const &value() const noexcept
    {
        tt_axiom(_slot != nullptr);
        return _slot->value();
    }

    slot_type::value_type const *operator->() const noexcept
    {
        return &value();
    }

    slot_type::value_type const &operator*() const noexcept
    {
        return value();
    }

private:
    wfree_hash_map_base *_hash_map;
    slot_type const *_slot;
};


} // namespace detail

/** Wait-free hash map.
 *
 *
 * Grow algorithm:
 * 1. secondary is nullptr
 * 2. secondary = primary
 * 3. -> other threads will treat secondary still as primary.
 * 3. primary = new table
 * 4. -> other threads when accessing the secondary will keep track of use_count.
 * 5. -> other threads will opportunistically move entries that are being searched.
 *    - `get()` if not found in primary will move the item in secondary when found.
 *    - `remove()` will remove items in both secondary and primary.
 *    - `set()` will add new item and remove old item in secondary and primary.
 * 6. Slowly iterate in reverse through the secondary
 *    table and moving items from secondary to primary. It does this at a speed so that
 *    the secondary table is empty before the primary table is full.
 * 7. tertiary = secondary
 * 8. secondary = nullptr
 * 9. When tertiary->use_count is zero deallocate tertiary.
 */
template<
    typename Key,
    typename T,
    typename Hash = std::hash<Key>,
    typename KeyEqual = std::equal_to<Key>,
    typename Allocator = std::allocator<wfree_hash_map_item<Key, T>>>
class wfree_hash_map : detail::wfree_hash_map_base {
public:
    using key_type = Key;
    using value_type = T;
    using table_type = detail::wfree_hash_map_table<Key, T, KeyEqual>;
    using size_type = std::size_t;
    using difference_type = std::ptrdiff_t;
    using hasher = Hash;
    using key_equal = KeyEqual;
    using allocator_type = Allocator;
    using const_pointer = detail::wfree_hash_map_pointer<value_type>;

    const_pointer get(key_type const &key) const noexcept
    {
        return get(make_hash(key), key);
    }

    const_pointer remove(key_type const &key) noexcept
    {
        return get(make_hash(key), key);
    }

    const_pointer set(key_type const &key, value_type const &value) noexcept
    {
        return set(make_hash(key), key, value);
    }

    const_pointer try_set(key_type const &key, value_type const &value) noexcept
    {
        return try_set(make_hash(key), key, value);
    }

private:
    std::atomic<table_type *> _primary = nullptr;
    std::atomic<table_type *> _secondary = nullptr;

    allocator_type _allocator;

    [[nodiscard]] static constexpr unit64_t make_hash(key_type const &key) noexcept
    {
        auto hash = static_cast<uint64_t>(hasher{}(key));

        // FNV hash, to create lots of upper bits for the next full width multiplication..
        auto tmp = (hash ^ 14695981039346656037) * 1099511628211;

        // Golden Ratio 64 bit.
        auto [lo, hi] = mul_carry(tmp, 11400714819323198485);
        // The upper 32 bits of lo and lower 32 bits of hi should have most information, simply mix them.
        lo ^= hi;

        // Top three bits must be cleared.
        lo <<= 3;
        lo >>= 3;
        return lo;
    }

    std::pair<table_type *, table_type *> get_tables() const noexcept
    {
        // During allocation on the other thread, the secondary will first
        // get a copy before the primary get set to nullptr. By loading
        // in this order we get the values in a consistent order.
        auto *primary = _primary.load(std::memory_order::acquire);
        auto *secondary = _secondary.load(std::memory_order::acquire);

        if ((primary == nullptr and secondary != nullptr) or primary == secondary) {
            // Another thread is currently allocating a new secondary table.
            // Use the pointer in the secondary table.
            primary = std::exchange(secondary, nullptr);
        }

        tt_axiom(primary != nullptr);
        return {primary, secondary};
    }

    /** Get and entry from the hash map.
     *
     * @param key The key to search.
     * @param hash The hash calculated from the key.
     * @return A pointer to the entry, or empty.
     */
    [[nodiscard]] const_pointer get(key_type const &key, uint64_t hash) const noexcept
    {
        ttlet[primary, secondary] = get_tables();

        // Search the primary table for matching entries.
        ttlet generation = increment_use_count();

        if (primary) {
            if (ttlet found = primary->get(key, hash, generation)) {
                return {this, found};
            }
        }

        // Search the secondary table for matching entries
        // Move the entry to the primary table and return the new primary entry.
        if (secondary) {
            tt_axiom(primary);
            if (ttlet found = secondary->get(key, hash, generation)) {
                ttlet new_ptr = primary->move_from(secondary, found, hash, generation);
                return {this, new_ptr};
            }
        }

        decrement_use_count();
        return {};
    }

    template<forward_for<key_type> Key, forward_for<value_type> Value>
    const_pointer set(Key &&key, Value &&value, uint64_t hash) noexcept
    {
        ttlet[primary, secondary] = get_tables_grow_and_cleanup();
        tt_axiom(primary);

        ttlet generation = increment_use_count();
        if (ttlet found = primary->set<false, false>(std::forward<Key>(key), std::forward<Value>(value), hash, generation)) {
            return {this, found};
        }

        decrement_use_count();
        return {};
    }

    template<forward_for<key_type> Key, forward_for<value_type> Value>
    const_pointer try_set(Key &&key, Value &&value, uint64_t hash) noexcept
    {
        ttlet[primary, secondary] = get_tables_grow_and_cleanup();

        ttlet generation = increment_use_count();
        if (ttlet found = primary->set<true, false>(std::forward<Key>(key), std::forward<Value>(value), hash, generation)) {
            return {this, found};
        }

        decrement_use_count();
        return {};
    }

    [[nodiscard]] const_pointer remove(key_type const &key, uint64_t hash) noexcept
    {
        ttlet[primary, secondary] = get_tables_grow_and_cleanup();

        ttlet generation = increment_use_count();

        const_pointer *secondary_slot = secondary ? secondary->remove(key, hash, generation) : nullptr;
        const_pointer *primary_slot = primary ? primary->remove(key, hash, generation) : nullptr;

        if (primary_slot) {
            return {this, primary_slot};
        } else if (secondary_slot) {
            return {this, secondary_slot};
        } else {
            decrement_use_count();
            return {};
        }
    }
};

} // namespace tt::inline v1
