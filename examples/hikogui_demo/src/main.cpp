
#include "hikogui/GFX/RenderDoc.hpp"
#include "hikogui/GFX/gfx_system.hpp"
#include "hikogui/GUI/gui_system.hpp"
#include "hikogui/GUI/theme_book.hpp"
#include "hikogui/audio/audio_system.hpp"
#include "hikogui/widgets/toolbar_button_widget.hpp"
#include "hikogui/widgets/momentary_button_widget.hpp"
#include "hikogui/widgets/row_column_widget.hpp"
#include "hikogui/widgets/selection_widget.hpp"
#include "hikogui/widgets/toggle_widget.hpp"
#include "hikogui/widgets/checkbox_widget.hpp"
#include "hikogui/widgets/radio_button_widget.hpp"
#include "hikogui/widgets/text_field_widget.hpp"
#include "hikogui/widgets/tab_widget.hpp"
#include "hikogui/widgets/toolbar_tab_button_widget.hpp"
#include "hikogui/widgets/audio_device_widget.hpp"
#include "hikogui/log.hpp"
#include "hikogui/crt.hpp"
#include "hikogui/time_stamp_count.hpp"
#include "hikogui/metadata.hpp"
#include "hikogui/preferences.hpp"
#include "hikogui/when_any.hpp"
#include "hikogui/task.hpp"
#include "hikogui/loop.hpp"
#include <memory>

class my_preferences : public hi::preferences {
public:
    hi::observable<std::string> audio_output_device_id;
    hi::observable<bool> audio_output_exclusive;
    hi::observable<double> audio_output_sample_rate;
    hi::observable<hi::speaker_mapping> audio_output_speaker_mapping;

        hi::observable<std::string> audio_input_device_id;
    hi::observable<bool> audio_input_exclusive;
        hi::observable<double> audio_input_sample_rate;
    hi::observable<hi::speaker_mapping> audio_input_speaker_mapping;

    hi::observable<int> tab_index = 1;
    hi::observable<bool> toggle_value;
    hi::observable<int> radio_value = 0;
    hi::observable<std::vector<std::pair<std::string, hi::label>>> _audio_device_list;

    hi::observable<std::string> selected_theme;

    my_preferences(hi::URL url) : hi::preferences(std::move(url))
    {
        add("audio_output_device_id", audio_output_device_id);
        add("audio_output_exclusive", audio_output_exclusive);
        add("audio_output_sample_rate", audio_output_sample_rate);
        add("audio_input_device_id", audio_input_device_id);
        add("audio_input_exclusive", audio_input_exclusive);
        add("audio_input_sample_rate", audio_input_sample_rate);
        add("tab_index", tab_index);
        add("toggle_value", toggle_value);
        add("radio_value", radio_value);
        add("selected_theme", selected_theme);
    }
};

hi::scoped_task<> init_audio_tab(hi::grid_widget& grid, my_preferences& preferences, hi::audio_system &audio_system) noexcept
{
    using namespace hi;

    grid.make_widget<label_widget>("A1", tr("Input audio device:"));
    auto &input_config = grid.make_widget<audio_device_widget>("B1", audio_system);
    input_config.direction = audio_direction::input;
    input_config.device_id = preferences.audio_input_device_id;

    grid.make_widget<label_widget>("A2", tr("Output audio device:"));
    auto& output_config = grid.make_widget<audio_device_widget>("B2", audio_system);
    output_config.direction = audio_direction::output;
    output_config.device_id = preferences.audio_output_device_id;

    co_await std::suspend_always{};
}

hi::scoped_task<> init_theme_tab(hi::grid_widget& grid, my_preferences& preferences) noexcept
{
    using namespace hi;

    hi::observable<std::vector<std::pair<std::string, hi::label>>> theme_list;

    {
        auto& theme_book = *grid.window.gui.theme_book;
        auto proxy = theme_list.proxy();
        for (hilet& name : theme_book.theme_names()) {
            proxy->emplace_back(name, tr{name});
        }
    }

    grid.make_widget<label_widget>("A1", tr("Theme:"));
    grid.make_widget<selection_widget>("B1", theme_list, preferences.selected_theme);

    co_await std::suspend_always{};
}

hi::scoped_task<> init_license_tab(hi::grid_widget& grid, my_preferences& preferences) noexcept
{
    using namespace hi;

    grid.make_widget<label_widget>(
        "A1", tr("This is a \xd7\x9c\xd6\xb0\xd7\x9e\xd6\xb7\xd7\xaa\xd6\xb5\xd7\x92.\nAnd another sentence. One more:"));
    auto& toggle = grid.make_widget<toggle_widget>("B1", preferences.toggle_value);
    toggle.on_label = tr("true");
    toggle.off_label = tr("false");
    toggle.other_label = tr("other");

    grid.make_widget<label_widget>("A2", tr("These is a disabled checkbox:"));
    auto& checkbox2 = grid.make_widget<checkbox_widget>("B2", preferences.radio_value, 2, 0);
    checkbox2.on_label = tr("Checkbox, with a pretty large label.");

    grid.make_widget<label_widget>("A3", tr("These are radio buttons:"));
    grid.make_widget<radio_button_widget>("B3", tr("Radio 1"), preferences.radio_value, 0);
    grid.make_widget<radio_button_widget>("B4", tr("Radio 2"), preferences.radio_value, 1);
    grid.make_widget<radio_button_widget>("B5", tr("Radio 3"), preferences.radio_value, 2);

    auto option_list = std::vector{
        std::pair{0, label{tr("first")}},
        std::pair{1, label{tr("second")}},
        std::pair{2, label{tr("third")}},
        std::pair{3, label{tr("four")}},
        std::pair{4, label{tr("five")}},
        std::pair{5, label{tr("six")}},
        std::pair{6, label{tr("seven")}}};
    grid.make_widget<label_widget>("A6", tr("This is a selection box at the bottom:"));
    auto& selection3 = grid.make_widget<selection_widget>("B6", option_list, preferences.radio_value);

    grid.make_widget<label_widget>("A7", tr("Sample Rate:"));
    grid.make_widget<text_field_widget>("B7", preferences.audio_output_sample_rate);

    auto toggle_value_cbt = preferences.toggle_value.subscribe([&](bool value) {
        checkbox2.mode = value ? widget_mode::enabled : widget_mode::disabled;
        selection3.mode = value ? widget_mode::enabled : widget_mode::disabled;
    });

    co_await std::suspend_always{};
}

hi::task<> preferences_window(hi::gui_system& gui, my_preferences& preferences, hi::audio_system &audio_system)
{
    using namespace hi;

    auto window_label = label{URL{"resource:hikogui_demo.png"}, tr("Preferences")};
    auto window = gui.make_window(window_label);

    window->toolbar().make_widget<toolbar_tab_button_widget>(label{elusive_icon::Speaker, tr("Audio")}, preferences.tab_index, 0);
    window->toolbar().make_widget<toolbar_tab_button_widget>(label{elusive_icon::Key, tr("License")}, preferences.tab_index, 1);
    window->toolbar().make_widget<toolbar_tab_button_widget>(label{elusive_icon::Brush, tr("Theme")}, preferences.tab_index, 2);

    auto& tabs = window->content().make_widget<tab_widget>("A1", preferences.tab_index);
    auto& audio_tab_grid = tabs.make_widget<grid_widget>(0);
    auto& license_tab_grid = tabs.make_widget<scroll_widget<axis::both, true>>(1).make_widget<grid_widget>();
    auto& theme_tab_grid = tabs.make_widget<grid_widget>(2);

    auto audio_tab = init_audio_tab(audio_tab_grid, preferences, audio_system);
    auto license_tab = init_license_tab(license_tab_grid, preferences);
    auto theme_tab = init_theme_tab(theme_tab_grid, preferences);

    co_await window->closing;
}

hi::task<> main_window(hi::gui_system& gui, my_preferences& preferences, hi::audio_system &audio_system)
{
    using namespace hi;

    auto window_label = label{URL{"resource:hikogui_demo.png"}, tr("HikoGUI demo")};
    auto window = gui.make_window(window_label);

    auto preferences_label = label{elusive_icon::Wrench, tr("Preferences")};
    hilet& preferences_button = window->toolbar().make_widget<hi::toolbar_button_widget>(preferences_label);

    auto& column = window->content().make_widget<column_widget>("A1");
    column.make_widget<toggle_widget>(preferences.toggle_value);
    hilet& hello_world_button = column.make_widget<momentary_button_widget>(tr("Hello world"));

    hilet& vma_dump_button = column.make_widget<momentary_button_widget>(tr("vma\ncalculate stats"));

    while (true) {
        hilet result = co_await when_any(
            preferences_button.pressed,
            vma_dump_button.pressed,
            hello_world_button.pressed,
            preferences.toggle_value,
            window->closing);

        switch (result.index()) {
        case 0:
            preferences_window(gui, preferences, audio_system);
            break;
        case 1:
            gui.gfx->log_memory_usage();
            break;
        case 2:
            hi_log_info("Hello World");
            break;
        case 3:
            hi_log_info("Toggle value {}", get<bool>(result));
            break;
        case 4:
            co_return;
        default:
            hi_no_default();
        }
    }
}

int hi_main(int argc, char *argv[])
{
    using namespace hi;

    // Set the version at the very beginning, because file system paths depend on it.
    auto& m = metadata::application();
    m.name = "hikogui-demo";
    m.display_name = "HikoGUI Demo";
    m.vendor = metadata::library().vendor;
    m.version = metadata::library().version;

    // Start the logger system, so logging is done asynchronously.
    log::start_subsystem(global_state_type::log_level_info);
    time_stamp_count::start_subsystem();
    auto render_doc = RenderDoc();

    auto preferences = my_preferences(URL::urlFromApplicationPreferencesFile());

    auto gui = gui_system::make_unique();
    gui->selected_theme = preferences.selected_theme;

    auto audio_system = hi::audio_system::make_unique();

    main_window(*gui, preferences, *audio_system);
    return loop::main().resume();
}

// extern "C" const char *__asan_default_options() {
//    return "help=1:log_path=asan.log";
//}
