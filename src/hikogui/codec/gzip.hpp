// Copyright Take Vos 2020.
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at https://www.boost.org/LICENSE_1_0.txt)

#pragma once

#include "../URL.hpp"
#include "../byte_string.hpp"
#include "../resource_view.hpp"
#include <cstddef>

namespace hi::inline v1 {

bstring gzip_decompress(std::span<std::byte const> bytes, std::size_t max_size = 0x01000000);

inline bstring gzip_decompress(URL const &url, std::size_t max_size = 0x01000000)
{
    return gzip_decompress(as_span<std::byte const>(*url.loadView()), max_size);
}

} // namespace hi::inline v1
