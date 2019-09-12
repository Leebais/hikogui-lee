// Copyright 2019 Pokitec
// All rights reserved.

#pragma once

#include "TTauri/Diagnostic/exceptions.hpp"
#include "TTauri/Required/URL.hpp"
#include <gsl/gsl>
#include <unordered_map>
#include <cstdint>
#include <cstddef>

namespace TTauri {

struct StaticResources {
    gsl_suppress(lifetime.4)
    std::unordered_map<std::string,gsl::span<std::byte const>> intrinsic;

    StaticResources() noexcept;

    gsl::span<std::byte const> const get(std::string const &filename) const;
};

}