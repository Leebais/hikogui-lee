// Copyright Take Vos 2021.
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at https://www.boost.org/LICENSE_1_0.txt)

#pragma once

#include "text_field_delegate.hpp"
#include "../observable.hpp"
#include "../label.hpp"
#include "../charconv.hpp"
#include "../type_traits.hpp"
#include <type_traits>
#include <memory>
#include <vector>
#include <concepts>

namespace hi::inline v1 {
template<typename T>
class default_text_field_delegate;

template<std::integral T>
class default_text_field_delegate<T> : public text_field_delegate {
public:
    using value_type = T;

    observable<value_type> value;

    default_text_field_delegate(auto&& value) noexcept : value(hi_forward(value))
    {
        _value_cbt = this->value.subscribe([&](auto...) {
            this->_notifier();
        });
    }

    std::optional<label> validate(text_field_widget& sender, std::string_view text) noexcept override
    {
        try {
            [[maybe_unused]] auto dummy = from_string<value_type>(text, 10);
        } catch (parse_error const&) {
            return {tr{"Invalid integer"}};
        }

        return {};
    }

    std::string text(text_field_widget& sender) noexcept override
    {
        return to_string(*value);
    }

    void set_text(text_field_widget& sender, std::string_view text) noexcept override
    {
        try {
            value = from_string<value_type>(text, 10);
        } catch (std::exception const&) {
            // Ignore the error, don't modify the value.
            return;
        }
    }

private:
    typename decltype(value)::token_type _value_cbt;
};

template<std::floating_point T>
class default_text_field_delegate<T> : public text_field_delegate {
public:
    using value_type = T;

    observable<value_type> value;

    default_text_field_delegate(auto&& value) noexcept : value(hi_forward(value))
    {
        _value_cbt = this->value.subscribe([&](auto...) {
            this->_notifier();
        });
    }

    label validate(text_field_widget& sender, std::string_view text) noexcept override
    {
        try {
            [[maybe_unused]] auto dummy = from_string<value_type>(text);
        } catch (parse_error const&) {
            return {elusive_icon::WarningSign, tr{"Invalid floating point number"}};
        }

        return {};
    }

    std::string text(text_field_widget& sender) noexcept override
    {
        return to_string(*value);
    }

    void set_text(text_field_widget& sender, std::string_view text) noexcept override
    {
        try {
            value = from_string<value_type>(text);
        } catch (std::exception const&) {
            // Ignore the error, don't modify the value.
            return;
        }
    }

private:
    typename decltype(value)::token_type _value_cbt;
};

template<typename Value>
default_text_field_delegate(Value&&) -> default_text_field_delegate<observable_argument_t<std::remove_cvref_t<Value>>>;

std::unique_ptr<text_field_delegate> make_unique_default_text_field_delegate(auto&& value, auto&&...args) noexcept
{
    using value_type = observable_argument_t<std::remove_cvref_t<decltype(value)>>;
    return std::make_unique<default_text_field_delegate<value_type>>(hi_forward(value), hi_forward(args)...);
}

} // namespace hi::inline v1
