// Copyright Take Vos 2022.
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at https://www.boost.org/LICENSE_1_0.txt)

#pragma once

#include "../aasert.hpp"
#include "../memory.hpp"
#include <atomic>
#include <cstdint>
#include <cstddef>


//
// Get:
//   Walk forward, remember last match, tombstone previous match.
//   On empty
//     Return match
//
// Remove:
//   Walk forward tombstone every match
//
// Set:
//   Walk forward, remember last match, tombstone previous match.
//   On old-tombstone
//     Reserve
//     Write key/value
//     Make read-available
//     Remove duplicates forward
//     Commit
//   On empty
//     Reserve
//     Write key/value
//     Commit
//
// Try-Set:
//   Walk forward
//   On match
//    Stop
//   On old-tombstone
//     Reserve
//     Write key/value
//     Commit
//   On empty
//     Reserve
//     Write key/value
//     Commit
//
//                 000: Empty
//                 001: Reserved
//     <generation>010: Tombstone
//                 011:
//           <hash>100: Read-only
//           <hash>101: Committed
//                 110:
//                 111:


namespace tt::inline v1{

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
    {}
};

template<typename K, typename V>
class wfree_hash_map_slot {
    using key_type = K;
    using value_type = V;
    using item_type = wfree_hash_map_item<key_type, value_type>;


    ~wfree_hash_map_slot() noexcept
    {
        // If the slot has a tomb-stone or is comitted we have to destroy the item.
        if (_hash.load(std::memory_order::acquire) > 1) {
            std::destroy_at(item_ptr());
        }
    }

    wfree_hash_map_slot(wree_hash_map_slot const &) = delete;
    wfree_hash_map_slot(wree_hash_map_slot &&) = delete;
    wfree_hash_map_slot &operator=(wree_hash_map_slot const &) = delete;
    wfree_hash_map_slot &operator=(wree_hash_map_slot &&) = delete;
    constexpr wfree_hash_map_slot() noexcept = default;

    [[nodiscard]] item_type const *item_ptr() const noexcept
    {
        return reinterpret_cast<item_type const *>(_buffer.data());
    }

    [[nodiscard]] item_type *item_ptr() noexcept
    {
        return reinterpret_cast<item_type *>(_buffer.data());
    }

    [[nodiscard]] item_type &item() const noexcept
    {
        tt_axiom(_hash.load(std::memory_order::relaxed) > 1);
        ttlet ptr = item_ptr();
        tt_axiom(ptr != nullptr);
        return *ptr;
    }

    /** The hash value of this slot.
     *
     * Values:
     *  - 0: Empty
     *  - 1: Reserved
     *  - 2: Tomb-stone
     *  - other: Comitted.
     *
     */
    [[nodiscard]] size_t hash() const noexcept
    {
        return _hash.load(std::memory_order::acquire);
    }

    /** Get a reference to the key of the slot.
     */
    [[nodiscard]] key_type const &key() const noexcept
    {
        tt_axiom(_hash.load(std::memory_order::relaxed) > 1);
        return item().key;
    }

    /** Get a reference to the value of the slot.
     */
    [[nodiscard]] value_type const &value() const noexcept
    {
        tt_axiom(_hash.load(std::memory_order::relaxed) > 1);
        return item().value;
    }

    /** Reserve the slot.
     *
     * A thread reserves a slot before calling `emplace()`.
     *
     * @return true if the slot is now reserved.
     */
    [[nodiscard]] bool reserve() noexcept
    {
        size_t expected = 0;
        return _hash.compare_exchange_strong(expected, 1, std::memory_order::acquire);
    }

    /** Entomb a slot.
     *
     * @pre `emplace()` or `reserve()` must be called first.
     * @return True if this call entombed the slot, so that decrement is allowed.
     */
    [[nodiscard]] bool entomb() noexcept
    {
        tt_axiom(_hash.load(std::memory_order::relaxed) != 0);
        return _hash.except(2, std::memory_order::relaxed) != 2;
    }

    /** Destroy the item.
     *
     * @pre All slot's of the table must be empty, reseved or entombed.
     * @pre The use count of the slot's table must be zero.
     */
    void destroy() noexcept
    {
        tt_axiom(_hash.load(std::memory_order::relaxed) <= 2);
        if (_hash.load(std::memory_order::acquire) > 1) {
            std::destroy_at(item_ptr());
            _hash.store(0, std::memory_order::release);
        }
    }

    /** Create a key/value in the slot.
     *
     * This constructs an key/value pair in the slot's buffer.
     *
     * @note It is undefined behavior to emplace into a slot where `reserve()` returned false.
     * @pre `reserve()` must be called before this function.
     * @param key The key to store.
     * @param value The value to store.
     * @param hash The hash value of the key must be larger than 2.
     */
    template<forward_for<key_type> Key, forward_for<value_type> Value>
    void emplace(Key &&key, Value &&value, size_t hash) noexcept
    {
        tt_axiom(hash > 2);
        tt_axiom(_hash.load(std::memory_order::relaxed) == 1);

        std::construct_at(item_ptr(), std::forward<Key>(key), std::forward<Value>(value));
        _hash.store(hash, std::memory_order::release);
    }

private:
    std::atomic<size_t> _hash;
    alignas(item_type) std::array<char, sizeof(item_type)> _buffer;
};

struct wfree_hash_map_header {
    size_t const capacity;

    std::atomic<size_t> num_reservations;
    std::atomic<size_t> num_tombstones;
    std::atomic<size_t> clean_index;
    std::atomic<size_t> use_count;
    std::atomic<size_t> generation;

    wfree_hash_map_header(wfree_hash_map_header const &) = delete;
    wfree_hash_map_header(wfree_hash_map_header &&) = delete;
    wfree_hash_map_header &operator=(wfree_hash_map_header const &) = delete;
    wfree_hash_map_header &operator=(wfree_hash_map_header &&) = delete;
    wfree_hash_map_header(size_t capacity) noexcept :
        capacity(capacity), num_reservations(0), num_tombstones(0), clean_index(capacity + 1), use_count(0)
    {}

    void increment_reservations() noexcept
    {
        num_reservations.fetch_add(1, std::memory_order::acquire);
    }

    void increment_tombstones() noexcept
    {
        num_tombstones.fetch_add(1, std::memory_order::release);
    }

    /** Increment the use count on the secondary table.
     *
     * @return Current generation, used for reclaiming.
     *         No other thread has a generation number two lower than this value.
     */
    size_t increment_use_count() noexcept
    {
        if (use_count.fetch_add(1, std::memory_order::relaxed) == 0) {
            return generation.fetch_add(1, std::memory_order::acquire) + 1;
        } else {
            return generation.load(std::memory_order::acquire);
        }
    }

    /** Decrement the use count on the secondary table.
     */
    void decrement_use_count() noexcept
    {
        use_count.fetch_sub(1, std::memory_order::release);
    }
};

/** A proxy object to a hash map slot.
 *
 * This object maintains a use-count with the specific hash table.
 */
template<typename S>
class wfree_hash_map_proxy {
public:
    using slot_type = T;

    /** Destruct the proxy object.
     *
     * @note decrements use count of table.
     */
    ~wfree_hash_map_proxy()
    {
        if (_table) {
            _table->decrement_use_count();
        }
    }

    /** Copies the proxy object.
     *
     * @note increments use count of table.
     */
    wfree_hash_map_proxy(wfree_hash_map_proxy const &other) noexcept : _table(other._table), _slot(other._slot)
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
        if (_table != _other._table) {
            if (_table) {
                _table->decrement_use_count();
            }
            _table = other._table;
            if (_table) {
                _table->increment_use_count();
            }
        }
        _slot = other._slot;
    }

    /** Moves the proxy object.
     */
    wfree_hash_map_proxy(wfree_hash_map_proxy &&other) noexcept :
        _table(std::exchange(other._table, nullptr)), _slot(std::exchange(other._slot, nullptr))
    {}

    /** Assignes by moving the proxy object.
     *
     * @note decrements use count of previous table.
     */
    wfree_hash_map_proxy &operator=(wfree_hash_map_proxy &&other) noexcept
    {
        if (_table) {
            _table->decrement_use_count();
        }
        _table = std::exchange(other._table, nullptr);
        _slot = std::exchange(other._slot, nullptr);
    }

    /** Construct a pointer.
     *
     * @note This constructor takes ownership of the use count of the caller.
     * @pre table->increment_use_count() must be called before.
     * @param secondary Pointer to the secondary table or nullptr.
     * @param ptr The pointer to the value.
     */
    wfree_hash_map_proxy(wfree_hash_map_header *table, slot_type const *slot) noexcept : _table(table), _slot(slot) {}

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
    wfree_hash_map_header *_table;
    slot_type const *_slot;
};

template<typename K, typename V, typename KeyEqual>
struct wfree_hash_map_table : wfree_hash_map_header {
    using key_type = K;
    using value_type = V;
    using slot_type = wfree_hash_map_slot<key_type, value_type>;
    using key_equal = KeyEqual;
    constexpr size_t data_offset = tt::ceil(sizeof(wfree_hash_map_header), sizeof(slot_type));

    ~wfree_hash_map_table() noexcept
    {
        std::destroy(begin(), end());
    }

    wfree_hash_map_table(size_t capacity) noexcept : wfree_hash_map_header(capacity)
    {
        for (auto it = begin(); it != end(); ++it) {
            std::construct_at(it);
        }
    }

    slot_type *begin() noexcept
    {
        auto *data = reinterpret_cast<char *>(this) + data_offset;
        return reinterpret_cast<wfree_hash_map_item *>(data);
    }

    slot_type *end() noexcept
    {
        return begin() + capacity;
    }

    slot_type *first_slot(size_t hash) noexcept
    {
        ttlet index = (hash >> 2) % capacity;
        return begin() + index;
    }

    slot_type *increment_slot(slot_type *it) noexcept
    {
        ++it;
        return it == end() ? begin() : it;
    }

    [[nodiscard]] slot_type *reserve_slot(slot_type *it) noexcept
    {
        tt_axiom(it != nullptr);

        while (true) {
            if (it.reserve()) {
                return it;
            }
            it = incement_slot(it);
        }
    }

    /** Find a key in the table.
     *
     * @note Duplicate keys are removed from the table.
     * @param key The key to search for.
     * @param hash The hash of the key.
     * @return The slot found, end() when reserved slot found, or nullptr when not found. pointer to entry after block.
     */
    template<bool StopAtReserved=false>
    std::pair<slot_type *, slot_type *> find(key_type const &key, size_t hash) noexcept
    {
        slot_type *it = first_slot(hash);
        slot_type *found = nullptr;
        while (true) {
            ttlet h = it->hash();
            if (h == 0 or (StopAtReserved and h == 1)) {
                return {found, it};

            } else if (h == hash and key_equal{}(it->key(), key)) {
                // Remove duplicate keys.
                if (found and found->entomb()) {
                    increment_tombstones();
                }
                found = it;
            }
            it = increment_slot(it);
        }
        tt_not_reached();
    }

    /** Search for the key in the table.
     *
     * @note Duplicate keys are removed from the table.
     * @param key The key to search for.
     * @param hash The hash of the key.
     * @return The slot that matches the key, or nullptr
     */
    slot_type *get(key_type const &key, size_t hash) noexcept
    {
        ttlet[found, it] = find(key, hash);
        return found;
    }

    /** Set a new key value in the table, overwriting existing.
     *
     * @note Duplicate keys are removed from the table.
     * @param key The key to add to the table.
     * @param value The value to add to the table.
     * @param hash The hash of the key.
     * @return The previous matching slot.
     */
    template<forward_for<key_type> Key, forward_for<value_type> Value>
    slot_type *set(Key &&key, Value &&value, size_t hash) noexcept
    {
        auto [found, it] = find(key, hash);
        auto it = reserve_slot(first);
        increment_reservations();
        it->emplace(std::forward<Key>(key), std::forward<Value>(value), hash);

        if (found and found->entomb()) {
            increment_tombstones();
        }
        return nullptr;
    }

    /** Set a new key value in the table, unless the key is already in the table.
     *
     * @note Duplicate keys are removed from the table.
     * @param key The key to add to the table.
     * @param value The value to add to the table.
     * @param hash The hash of the key.
     * @return The previous matching slot.
     */
    template<forward_for<key_type> Key, forward_for<value_type> Value>
    slot_type *try_set(Key &&key, Value &&value, size_t hash) noexcept
    {
        auto [found, it] = find(key, hash);
        if (found) {
            return found;
        }

        it = reserve_slot<StopAtReserved>(it);
        increment_reservations();
        it->emplace(std::forward<Key>(key), std::forward<Value>(value), hash);
        return nullptr;
    }

    /** Remove a key from the table.
     * 
     * @note Duplicate keys are removed from the table.
     * @param key The key to remove from the table.
     * @param hash The hash of the key.
     * @return The previous matching slot.
     */
    slot_type *remove(key_type const &key, size_t hash) noexcept
    {
        ttlet[found, it] = find(key, hash);
        if (found and found->entomb()) {
            increment_tombstones();
        }
        return found;
    }

    /** Move and entry from another hash map table.
    * 
    * @post The key/value may be added to this table and is removed from the other table.
    * @param other_table A pointer to the other table.
    * @param other_slot A slot from the other table to move.
    * @param hash The hash value of the key in the other_slot.
    * @return move failed due to other thread.
    */
    other_table *move_from(wfree_hash_map_table *other_table, slot_type *other_slot, size_t hash) noexcept
    {
        tt_axiom(other_table != nullptr);
        tt_axiom(other_slot != nullptr);

        auto [found, it] = find<true>(key, hash);

        // If it can be reserved then there was no reservation in the block that was searched.
        if (not found and it->reserve()) {
            increment_reservations();
            it->emplace(std::forward<Key>(key), std::forward<Value>(value), hash);
            found = it;
        }

        if (found and other_slot->entomb()) {
            other_table->increment_tombstones();
        }

        return found;
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
    [[nodiscard]] static wfree_hash_map_table *allocate(Allocator const &allocator, size_t capacity)
        requires(std::same_v<Allocator::value_type, char>)
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
        std::allocator_traits<Allocator>::deallocate(allocator, reinterpret_cast<char *>(table_ptr), capacity_to_num_bytes(capacity));
    }
};
static_assert(sizeof(wfree_hash_map_header) == sizeof(wfree_hash_map_table));





}

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
    typename Allocator = std::allocator<wfree_hash_map_item<Key, T>>
    class wfree_hash_map : wfree_hash_map_base {
    public:
        using key_type = Key;
        using value_type = T;
        using table_type = wfree_hash_map_table<Key, T, Hash, KeyEqual>;
        using size_type = std::size_t;
        using difference_type = std::ptrdiff_t;
        using hasher = Hash;
        using key_equal = KeyEqual;
        using allocator_type = Allocator;
        using const_pointer = wfree_hash_map_pointer<value_type>;

        /** Get and entry from the hash map.
        * 
        * @param key The key to search.
        * @param hash The hash calculated from the key.
        * @return A pointer to the entry, or empty.
        */
        [[nodiscard]] const_pointer get(key_type const &key, size_t hash) const noexcept
        {
            ttlet [primary, secondary] = get_tables();

            // Search the primary table for matching entries.
            primary->increment_use_count();
            if (ttlet found = primary->get(key, hash)) {
                return {primary, found};
            }

            // Search the secondary table for matching entries
            // Move the entry to the primary table and return the new primary entry.
            if (secondary) {
                secondary->increment_use_count();
                if (ttlet found = secondary->get(key, hash)) {
                    ttlet new_ptr = primary->move_from(secondary, found, hash);
                    secondary->decrement_use_count();
                    return {primary, new_ptr};
                }
                secondary->decrement_use_count();   
            }

            primary->decrement_use_count();
            return {};
        }

        template<forward_for<key_type> Key, forward_for<value_type> Value>
        const_pointer set(Key &&key, Value &&value, size_t hash) noexcept
        {
            ttlet [primary, secondary] = get_tables_grow_and_cleanup();

            primary->increment_use_count();
            if (ttlet found = primary->set(std::forward<Key>(key), std::forward<Value>(value), hash)) {
                return {primary, found};
            }

            primary->decrement_use_count();
            return {};
        }

        template<forward_for<key_type> Key, forward_for<value_type> Value>
        const_pointer try_set(Key &&key, Value &&value, size_t hash) noexcept
        {
            ttlet [primary, secondary] = get_tables_grow_and_cleanup();

            primary->increment_use_count();
            if (ttlet found = primary->try_set(std::forward<Key>(key), std::forward<Value>(value), hash)) {
                return {primary, found};
            }

            primary->decrement_use_count();
            return {};
        }

        [[nodiscard]] const_pointer remove(key_type const &key, size_t hash) const noexcept
        {
            ttlet [primary, secondary] = get_tables_grow_and_cleanup();

            if (secondary) {
                secondary->increment_use_count();
                ttlet secondary_found = secondary->remove(key, hash);

                primary->increment_use_count();
                if (ttlet primary_found = primary->remove(key, hash)) {
                    secondary->decrement_use_count();
                    return {primary, primary_found};

                } else if (secondary_found) {
                    primary->decrement_use_count();
                    return {secondary, secondary_found};

                } else {
                    primary->decrement_use_count();
                    secondary->decrement_use_count();
                    return {};
                }

            } else {
                primary->increment_use_count();
                if (ttlet primary_found = primary->remove(key, hash)) {
                    return {primary, primary_found};
                }

                primary->decrement_use_count();
                return {};
            }
        }

        const_pointer set(key_type const &key, value_type const &value) noexcept
        {
            return set(make_hash(key), key, value);
        }

    private:
        std::atomic<table_type *> _primary;
        std::atomic<table_type *> _secondary;

        allocator_type _allocator;

        std::pair<table_type *, table_type *> get_tables() const noexcept
        {
            // During allocation on the other thread, the secondary will first
            // get a copy before the primary get set to nullptr. By loading
            // in this order we get the values in a consistant order.
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

};

}

