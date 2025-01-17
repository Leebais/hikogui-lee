// Copyright Take Vos 2020-2021.
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at https://www.boost.org/LICENSE_1_0.txt)

#pragma once

#include "widget.hpp"
#include "label_widget.hpp"
#include "overlay_widget.hpp"
#include "scroll_widget.hpp"
#include "row_column_widget.hpp"
#include "menu_button_widget.hpp"
#include "selection_delegate.hpp"
#include "default_selection_delegate.hpp"
#include "../observable.hpp"
#include "../weak_or_unique_ptr.hpp"
#include <memory>
#include <string>
#include <array>
#include <optional>
#include <future>

namespace hi::inline v1 {

/** A graphical control element that allows the user to choose only one of a
 * predefined set of mutually exclusive options.
 *
 * @image html selection_widget.gif
 *
 * The following example creates a selection widget with three options.
 * which will monitor and modify `value` to display the options from
 * the `option_list`. At application startup, value is zero and none
 * of the options is selected:
 *
 * @snippet widgets/selection_example.cpp Create selection
 */
class selection_widget final : public widget {
public:
    using super = widget;
    using delegate_type = selection_delegate;

    observable<label> unknown_label;

    ~selection_widget();

    /** Construct a selection widget with a delegate.
     *
     * @param window The window the selection widget will be displayed on.
     * @param parent The owner of the selection widget.
     * @param delegate The delegate which will control the selection widget.
     */
    selection_widget(gui_window &window, widget *parent, std::weak_ptr<delegate_type> delegate) noexcept;

    /** Construct a selection widget which will monitor an option list and a
     * value.
     *
     * @tparam Value The type of @a value. Must be convertible to `std::size_t`.
     * @param window The window the selection widget will be displayed on.
     * @param parent The owner of the selection widget.
     * @param option_list An vector or an observable vector of pairs of keys and
     *                    labels. The keys are of the same type as the @a value.
     *                    The labels are of type `label`.
     * @param value The value or observable value to monitor.
     */
    template<typename OptionList, typename Value, typename... Args>
    selection_widget(gui_window &window, widget *parent, OptionList &&option_list, Value &&value, Args &&...args) noexcept
        requires(not std::is_convertible_v<Value, weak_or_unique_ptr<delegate_type>>) :
        selection_widget(
            window,
            parent,
            make_unique_default_selection_delegate(
                std::forward<OptionList>(option_list),
                std::forward<Value>(value),
                std::forward<Args>(args)...))
    {
    }

    /// @privatesection
    [[nodiscard]] generator<widget *> children() const noexcept override
    {
        co_yield _overlay_widget.get();
        co_yield _current_label_widget.get();
        co_yield _unknown_label_widget.get();
    }

    widget_constraints const &set_constraints() noexcept override;
    void set_layout(widget_layout const &layout) noexcept override;
    void draw(draw_context const &context) noexcept override;
    bool handle_event(gui_event const& event) noexcept override;
    [[nodiscard]] hitbox hitbox_test(point3 position) const noexcept override;
    [[nodiscard]] bool accepts_keyboard_focus(keyboard_focus_group group) const noexcept override;
    [[nodiscard]] color focus_color() const noexcept override;
    /// @endprivatesection
private:
    weak_or_unique_ptr<delegate_type> _delegate;
    notifier<>::token_type _delegate_cbt;

    std::unique_ptr<label_widget> _current_label_widget;
    std::unique_ptr<label_widget> _unknown_label_widget;

    aarectangle _option_rectangle;
    aarectangle _left_box_rectangle;

    glyph_ids _chevrons_glyph;
    aarectangle _chevrons_rectangle;

    bool _selecting = false;
    bool _has_options = false;

    aarectangle _overlay_rectangle;
    std::unique_ptr<overlay_widget> _overlay_widget;
    vertical_scroll_widget<> *_scroll_widget = nullptr;
    column_widget *_column_widget = nullptr;

    decltype(unknown_label)::token_type _unknown_label_cbt;
    std::vector<menu_button_widget *> _menu_button_widgets;
    std::vector<notifier<>::token_type> _menu_button_tokens;

    selection_widget(gui_window &window, widget *parent, weak_or_unique_ptr<delegate_type> delegate) noexcept;
    [[nodiscard]] menu_button_widget const *get_first_menu_button() const noexcept;
    [[nodiscard]] menu_button_widget const *get_selected_menu_button() const noexcept;
    void start_selecting() noexcept;
    void stop_selecting() noexcept;
    void repopulate_options() noexcept;
    void draw_outline(draw_context const &context) noexcept;
    void draw_left_box(draw_context const &context) noexcept;
    void draw_chevrons(draw_context const &context) noexcept;
};

} // namespace hi::inline v1
