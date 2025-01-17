// Copyright Take Vos 2021.
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at https://www.boost.org/LICENSE_1_0.txt)

#pragma once

#include "tab_delegate.hpp"
#include "../observable.hpp"
#include "../cast.hpp"
#include <type_traits>
#include <memory>
#include <unordered_map>

namespace hi::inline v1 {

template<typename T>
class default_tab_delegate : public tab_delegate {
public:
    using value_type = T;

    observable<value_type> value;
    std::unordered_map<std::size_t, std::size_t> tab_indices;

    default_tab_delegate(auto &&value) noexcept : value(hi_forward(value))
    {
        _value_cbt = this->value.subscribe([&](auto...) {
            this->_notifier();
        });
    }

    void add_tab(tab_widget &sender, std::size_t key, std::size_t index) noexcept override
    {
        hi_axiom(not tab_indices.contains(key));
        tab_indices[key] = index;
    }

    [[nodiscard]] ssize_t index(tab_widget &sender) noexcept override
    {
        auto it = tab_indices.find(*value);
        if (it == tab_indices.end()) {
            return -1;
        } else {
            return static_cast<ssize_t>(it->second);
        }
    }

private:
    typename decltype(value)::token_type _value_cbt;
};

template<typename Value>
default_tab_delegate(Value &&) -> default_tab_delegate<observable_argument_t<std::remove_cvref_t<Value>>>;

template<typename Value>
std::unique_ptr<tab_delegate> make_unique_default_tab_delegate(Value &&value) noexcept
{
    using value_type = observable_argument_t<std::remove_cvref_t<Value>>;
    return std::make_unique<default_tab_delegate<value_type>>(std::forward<Value>(value));
}

} // namespace hi::inline v1
