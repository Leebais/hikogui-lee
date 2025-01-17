// Copyright Take Vos 2020.
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at https://www.boost.org/LICENSE_1_0.txt)

#include "URL.hpp"
#include "datum.hpp"
#include "log.hpp"
#include "jsonpath.hpp"
#include "observable.hpp"
#include "pickle.hpp"
#include <typeinfo>

#pragma once

namespace hi::inline v1 {
class preferences;

namespace detail {

class preference_item_base {
public:
    preference_item_base(preferences &parent, std::string_view path) noexcept;

    preference_item_base(preference_item_base const &) = delete;
    preference_item_base(preference_item_base &&) = delete;
    preference_item_base &operator=(preference_item_base const &) = delete;
    preference_item_base &operator=(preference_item_base &&) = delete;
    virtual ~preference_item_base() = default;

    /** Reset the value.
     */
    virtual void reset() noexcept = 0;

    /** Load a value from the preferences.
     */
    void load() noexcept;

protected:
    preferences &_parent;
    jsonpath _path;

    /** Encode the value into a datum.
     *
     * @return A datum representing the value, or undefined if same as the initial value.
     */
    [[nodiscard]] virtual datum encode() const noexcept = 0;

    virtual void decode(datum const &data) = 0;
};

template<typename T>
class preference_item : public preference_item_base {
public:
    preference_item(preferences &parent, std::string_view path, observable<T> const &value, T init) noexcept :
        preference_item_base(parent, path), _value(value), _init(std::move(init))
    {
        _value_cbt = _value.subscribe([this](auto...) {
            if (auto tmp = this->encode(); not holds_alternative<std::monostate>(tmp)) {
                this->_parent.write(_path, this->encode());
            } else {
                this->_parent.remove(_path);
            }
        });
    }

    void reset() noexcept override
    {
        _value = _init;
    }

protected:
    [[nodiscard]] datum encode() const noexcept override
    {
        if (*_value != _init) {
            return hi::pickle<T>{}.encode(*_value);
        } else {
            return datum{std::monostate{}};
        }
    }

    void decode(datum const &data) override
    {
        _value = hi::pickle<T>{}.decode(data);
    }

private:
    T _init;
    observable<T> _value;
    typename decltype(_value)::token_type _value_cbt;
};

} // namespace detail

/** user preferences.
 *
 * A preferences objects maintains a link between observables in the application and
 * a preferences file.
 *
 * When loading preferences the observables are set to the value
 * in the preferences file. When an observable changes a value the preferences file is
 * updated to reflect this change. For performance reasons multiple modifications are
 * combined into a single save.
 *
 * An application may open multiple preferences files, for example an application preferences file
 * and a project-specific preferences file. The name of the project-specific preferences file
 * can then be selected by the user.
 *
 * The preferences file is updated by using the operating system specific call to
 * overwrite an existing file atomically.
 */
class preferences {
public:
    /** Mutex used to synchronize changes to the preferences.
     *
     * This mutex may be used externally to atomically combine multiple observer modification
     * into a single change of the preferences file.
     */
    mutable std::mutex mutex;

    /** Construct a preferences instance.
     *
     * No current preferences file will be selected.
     *
     * It is recommended to call `preferences::load(URL)` after the constructor.
     */
    preferences() noexcept;

    /** Construct a preferences instance.
     *
     * The current preferences file is changed to the location give.
     *
     * @param location The location of the preferences file to load from.
     */
    preferences(URL location) noexcept;

    ~preferences();
    preferences(preferences const &) = delete;
    preferences(preferences &&) = delete;
    preferences &operator=(preferences const &) = delete;
    preferences &operator=(preferences &&) = delete;

    /** Save the preferences.
     *
     * This will load the preferences from the current selected file.
     */
    void save() const noexcept;

    /** Save the preferences.
     *
     * This will change the current preferences file to the location given.
     *
     * @param location The file to save the preferences to.
     */
    void save(URL location) noexcept;

    /** Load the preferences.
     *
     * This will load the preferences from the current selected file.
     */
    void load() noexcept;

    /** Load the preferences.
     *
     * This will change the current preferences file to the location given.
     *
     * @param location The file to save the preferences to.
     */
    void load(URL location) noexcept;

    /** Reset data members to their default value.
     */
    void reset() noexcept;

    /** Register an observable to a preferences file.
     *
     * @param path The json-path inside the preference file.
     * @param item The observable to monitor.
     * @param init The value of the observable when it is not present in the preferences file.
     */
    template<typename T>
    void add(std::string_view path, observable<T> const &item, T init = T{}) noexcept
    {
        auto item_ = std::make_unique<detail::preference_item<T>>(*this, path, item, std::move(init));
        item_->load();
        _items.push_back(std::move(item_));
    }

private:
    /** The location of the preferences file.
     */
    URL _location;

    /** The data from the preferences file.
     */
    datum _data;

    /** The data was modified.
     * When this flag is true the preferences should be saved.
     */
    mutable bool _modified = false;

    loop::timer_token_type _check_modified_cbt;

    /** List of registered items.
     */
    std::vector<std::unique_ptr<detail::preference_item_base>> _items;

    void _load() noexcept;
    void _save() const noexcept;

    /** Check if there are modification in data and save when necessary.
     */
    void check_modified() noexcept;

    /** Write a value to the data.
     */
    void write(jsonpath const &path, datum const value) noexcept;

    /** Read a value from the data.
     */
    datum read(jsonpath const &path) noexcept;

    /** Remove a value from the data.
     */
    void remove(jsonpath const &path) noexcept;

    friend class detail::preference_item_base;
    template<typename T>
    friend class detail::preference_item;
};

} // namespace hi::inline v1
