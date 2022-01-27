

#pragma once

#include "../aasert.hpp"
#include "../memory.hpp"
#include <atomic>
#include <cstdint>
#include <cstddef>

namespace tt::inline v1 {

namespace detail {

template<typename K, typename V>
struct wfree_hash_map_item
{
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
        key(std::forward<Key>(key)), value(std::forward<Value>(value)) {}
};

template<typename K, typename V>
struct wfree_hash_map_slot {
    using key_type = K;
    using value_type = V;
    using item_type = wfree_hash_map_item<key_type, value_type>;

    /** The hash key and state.
     *
     * Values:
     *  - 0: Empty
     *  - 1: Fail
     *  - 2: Reserves
     *  - 3: Tomb stone
     *  - other: Key/Value are valid.
     *
     * Get operation:
     *  - Find all slots with same key (forward).
     *  - Entomb all found slots but the last one.
     *  - Return the item of the last slot.
     *
     * Set operation:
     *  - Find previous slots with same key (forward).
     *  - Reserve failed slot after the last found, or reserve empty slot.
     *  - Create key/value pair.
     *  - Set hash value.
     *  - Entomb any previous values.
     *
     * Try-set operation:
     *  - Find slots with same key, if found stop.
     *  - Reserve first failed slot, or reserve empty slot.
     *  - Create key/value pair.
     *  - Set hash value.
     *
     * Remove operation:
     *  - Find all slots with the same key (forward).
     *  - Entomb any found slot in order.
     * 
     *
     */
    std::atomic<size_t> _hash;
    alignas(item_type) std::array<char,sizeof(item_type)> _buffer;

    ~wfree_hash_map_slot() noexcept
    {
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
     */
    void entomb() noexcept
    {
        tt_axiom(_hash.load(std::memory_order::relaxed) != 0);
        _hash.store(2, std::memory_order::relaxed);
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
    template<is_forward_for<key_type> Key, is_forward_for<value_type> Value>
    void emplace(Key &&key, Value &&value, size_t hash) noexcept
    {
        tt_axiom(hash > 2);
        tt_axiom(_hash.load(std::memory_order::relaxed) == 1);

        std::construct_at(item_ptr(), std::forward<Key>(key), std::forward<Value>(value));
        _hash.store(hash, std::memory_order::release);
    }
};

template<typename Key, typename T>
struct wfree_hash_map_table {
    using item_type = wfree_hash_map_item<Key, T>;
    constexpr size_t header_size = sizeof(uint64_t);
    constexpr size_t data_offset = tt::ceil(header_size, sizeof(item_type));

    size_t const capacity;
    size_t size;
    size_t remove_index;

    wfree_hash_map_table(wfree_hash_map_table const &) = delete;
    wfree_hash_map_table(wfree_hash_map_table &&) = delete;
    wfree_hash_map_table &operator=(wfree_hash_map_table const &) = delete;
    wfree_hash_map_table &operator=(wfree_hash_map_table &&) = delete;

    ~wfree_hash_map_table() noexcept
    {
        std::destroy(begin(), end());
    }

    wfree_hash_map_table(size_t capacity) noexcept :
        capacity(capacity), size(0), remove_index(capacity + 1)
    {
        for (auto it = begin(); it != end(); ++it) {
            std::construct_at(it);
        }
    }

    wfree_hash_map_slot *begin() noexcept
    {
        auto *data = reinterpret_cast<char *>(this) + data_offset;
        return reinterpret_cast<wfree_hash_map_item*>(data);
    }

    wfree_hash_map_slot *end() noexcept
    {
        return begin() + capacity;
    }

    wfree_hash_map_slot &operator[](size_t index) noexcept
    {
        return *(begin() + index);
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
static_assert(header_size == sizeof(wfree_hash_map_table));


class wfree_hash_map_base {
public:

    /** Increment the use count on the secondary table.
     */
    void increment_use_count() noexcept
    {
        use_count.fetch_add(1, std::memory_order::acquire);
    }

    /** Decrement the use count on the secondary table.
     */
    void decrement_use_count() noexcept
    {
        use_count.fetch_sub(1, std::memory_order::release);
    }

private:
    std::atomic<size_t> use_count;
}


template<typename T>
class wfree_hash_map_pointer {
public:
    using value_type = T;
    using pointer = value_type *;
    using reference = value_type &;

    ~wfree_hash_map_pointer()
    {
        if (_secondary) {
            _secondary->decrement_ref_count();
        }
    }

    wfree_hash_map_pointer(wfree_hash_map_pointer const &other) noexcept : _secondary(other._secondary), _ptr(other._ptr)
    {
        if (_secondary) {
            _secondary->increment_ref_count();
        }
    }

    wfree_hash_map_pointer &operator=(wfree_hash_map_pointer const &other) noexcept
    {
        if (_secondary != _other._secondary) {
            if (_secondary) {
                _secondary->decrement_ref_count();
            }
            _secondary = other._secondary;
            if (_secondary) {
                _secondary->increment_ref_count();
            }
        }
        _ptr = other._ptr;
    }

    wfree_hash_map_pointer(wfree_hash_map_pointer &&other) noexcept :
        _secondary(std::exchange(other._secondary, nullptr)), _ptr(std::exchange(other._ptr, nullptr)) {}

    wfree_hash_map_pointer &operator=(wfree_hash_map_pointer &&other) noexcept
    {
        if (_secondary) {
            _secondary->decrement_ref_count();
        }
        _secondary = std::exchange(other._secondary, nullptr);
        _ptr = std::exchange(other._ptr, nullptr);
    }

    [[nodiscard]] static wfree_hash_map_pointer make_pointer(wfree_hash_map_base *map, T *ptr)
    {
        return wfree_hash_map_pointer{map, ptr};
    }

    T *operator->() noexcept
    {
        return _ptr;
    }

    T &operator*() noexcept
    {
        return *_otr;
    }

private:
    wfree_hash_map_base *_secondary;
    T *_ptr;

    /** Construct a pointer.
     *
     * @pre secondary->increment_ref_count() must be called first.
     * @param secondary Pointer to the secondary table or nullptr
     * @param ptr The pointer to the value.
     */
    wfree_hash_map_pointer(wfree_hash_map_base *secondary, T *ptr) noexcept :
        _secondary(secondary), _ptr(ptr)
    {
    }
};

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
    using reference = value_type &;
    using const_reference = value_type const &;
    using pointer = value_type *;
    using const_pointer = value_type const *;

    /** Remove a key from the hash map.
     *
     * This operation will fetch the value associated with the key and
     * remove the value from the table.
     *
     * @note remove has O(1) complexity
     * @note remove is wait-free.
     * @param key The key to remove from the hash map.
     * @param hash The hash value calculated from the key.
     * @return The previous value.
     */
    const_pointer fetch_and_remove(key_type const &key, size_t hash) noexcept
    {
        auto [primary, secondary] = get_tables_grow_and_cleanup();

        // Entomb in order of secondary to primary.
        value_type const *old_value = nullptr;
        if (secondary) {
            old_value = secondary->entomb<true>(hash, key);
        }

        auto old_primary_value = primary->entomb<false>(hash, key);
        if (old_primary_value) {
            old_value = old_primary_value;
        }

        return old_value;
    }

    /** Remove a key from the hash map.
     *
     * This operation will fetch the value associated with the key and
     * remove the value from the table.
     *
     * @note remove has O(1) complexity
     * @note remove is wait-free.
     * @param key The key to remove from the hash map.
     * @return The previous value.
     */
    const_pointer fetch_and_remove(key_type const &key) noexcept
    {
        return fetch_and_remove(key, hasher{}(key));
    }

    [[nodiscard]] const_pointer get(size_t hash, key_type const &key) const noexcept
    {
        auto [primary, secondary] = get_tables();

        auto old_value = primary->get<false>(hash, key);

        // If the value was not available in the primary, try the secondary.
        if (not old_value and secondary) {
            old_value = secondary->get<true>(hash, key);
            primary->maybe_set<false>(hash, key, *old_value);

            // We use false on this secondary because we are not going to read the value..
            secondary->entomb<false>(old_value);
        }
        return old_value;
    }

    const_pointer set(size_t hash, key_type const &key, value_type const &value) noexcept
    {
        auto [primary, secondary] = get_tables_grow_and_cleanup();

        auto old_value = primary->set<false>(hash, key, value);

        if (secondary) {
            auto old_secondary_value = secondary->entomb<true>(hash, key);
            if (not old_value) {
                old_value = secondary_value;
            }
        }

        return old_value;
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

