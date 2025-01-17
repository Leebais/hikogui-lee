// Copyright Take Vos 2021.
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at https://www.boost.org/LICENSE_1_0.txt)

#include "gui_system_win32.hpp"
#include "keyboard_bindings.hpp"
#include "theme_book.hpp"
#include "../GFX/gfx_system_vulkan.hpp"
#include "../text/font_book.hpp"
#include "../locked_memory_allocator.hpp"
#include "../trace.hpp"
#include "../log.hpp"
#include <memory>

namespace hi::inline v1 {

[[nodiscard]] std::unique_ptr<gui_system> gui_system::make_unique(std::weak_ptr<gui_system_delegate> delegate) noexcept
{
    auto font_book = std::make_unique<hi::font_book>(std::vector<URL>{URL::urlFromSystemfontDirectory()});
    font_book->register_elusive_icon_font(URL("resource:elusiveicons-webfont.ttf"));
    font_book->register_hikogui_icon_font(URL("resource:hikogui_icons.ttf"));
    font_book->post_process();

    auto theme_book = std::make_unique<hi::theme_book>(*font_book, std::vector<URL>{URL::urlFromResourceDirectory() / "themes"});

    auto gfx_system = std::make_unique<hi::gfx_system_vulkan>();

    auto keyboard_bindings = std::make_unique<hi::keyboard_bindings>();
    try {
        keyboard_bindings->load_bindings(URL{"resource:win32.keybinds.json"}, true);
    } catch (std::exception const &e) {
        hi_log_fatal("Could not load keyboard bindings. \"{}\"", e.what());
    }

    auto r = std::make_unique<gui_system_win32>(
        std::move(gfx_system),
        std::move(font_book),
        std::move(theme_book),
        std::move(keyboard_bindings),
        std::move(delegate));
    r->init();
    return r;
}

gui_system_win32::gui_system_win32(
    std::unique_ptr<gfx_system> gfx,
    std::unique_ptr<hi::font_book> font_book,
    std::unique_ptr<hi::theme_book> theme_book,
    std::unique_ptr<hi::keyboard_bindings> keyboard_bindings,
    std::weak_ptr<gui_system_delegate> delegate) :
    gui_system(
        std::move(gfx),
        std::move(font_book),
        std::move(theme_book),
        std::move(keyboard_bindings),
        std::move(delegate))
{
}

} // namespace hi::inline v1
