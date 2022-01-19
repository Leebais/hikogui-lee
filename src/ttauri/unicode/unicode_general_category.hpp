// Copyright Take Vos 2020.
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at https://www.boost.org/LICENSE_1_0.txt)

#pragma once

#include <cstdint>

namespace tt::inline v1 {

enum class unicode_general_category : uint8_t {
    Lu,
    Ll,
    Lt,
    Lm,
    Lo,
    Mn,
    Mc,
    Me,
    Nd,
    Nl,
    No,
    Pc,
    Pd,
    Ps,
    Pe,
    Pi,
    Pf,
    Po,
    Sm,
    Sc,
    Sk,
    So,
    Zs,
    Zl,
    Zp,
    Cc,
    Cf,
    Cs,
    Co,
    Cn
};

[[nodiscard]] constexpr bool is_LC(unicode_general_category const &rhs) noexcept
{
    using enum unicode_general_category;
    return rhs >= Lu and rhs <= Lt;
}

[[nodiscard]] constexpr bool is_L(unicode_general_category const &rhs) noexcept
{
    using enum unicode_general_category;
    return rhs >= Lu and rhs <= Lo;
}

[[nodiscard]] constexpr bool is_M(unicode_general_category const &rhs) noexcept
{
    using enum unicode_general_category;
    return rhs >= Mn and rhs <= Me;
}

[[nodiscard]] constexpr bool is_Mn_or_Mc(unicode_general_category const &rhs) noexcept
{
    using enum unicode_general_category;
    return rhs == Mn or rhs == Mc;
}

[[nodiscard]] constexpr bool is_N(unicode_general_category const &rhs) noexcept
{
    using enum unicode_general_category;
    return rhs >= Nd and rhs <= No;
}

[[nodiscard]] constexpr bool is_P(unicode_general_category const &rhs) noexcept
{
    using enum unicode_general_category;
    return rhs >= Pc and rhs <= Po;
}

[[nodiscard]] constexpr bool is_S(unicode_general_category const &rhs) noexcept
{
    using enum unicode_general_category;
    return rhs >= Sm and rhs <= So;
}

[[nodiscard]] constexpr bool is_Z(unicode_general_category const &rhs) noexcept
{
    using enum unicode_general_category;
    return rhs >= Zs and rhs <= Zp;
}

[[nodiscard]] constexpr bool is_C(unicode_general_category const &rhs) noexcept
{
    using enum unicode_general_category;
    return rhs >= Cc and rhs <= Cn;
}

[[nodiscard]] constexpr bool is_visible(unicode_general_category const &rhs) noexcept
{
    using enum unicode_general_category;
    return rhs < Zs or rhs == Co;
}

} // namespace tt::inline v1