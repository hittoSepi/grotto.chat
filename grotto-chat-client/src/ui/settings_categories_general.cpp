#include "ui/settings_screen.hpp"

#include "i18n/strings.hpp"
#include "ui/color_scheme.hpp"
#include "ui/settings_layout.hpp"

using namespace ftxui;

namespace grotto::ui {

Element SettingsScreen::render_general() {
    using namespace settings_layout;

    std::vector<Element> rows;
    rows.push_back(toggle_row(copy_selection_on_release_cb_->Render()));
    auto preview_rows = std::vector<Element>{
        toggle_row(inline_images_cb_->Render()),
        row(i18n::tr(i18n::I18nKey::IMAGE_COLUMNS_LABEL),
            image_columns_input_->Render() | size(WIDTH, GREATER_THAN, 6) | border),
        row(i18n::tr(i18n::I18nKey::IMAGE_ROWS_LABEL),
            image_rows_input_->Render() | size(WIDTH, GREATER_THAN, 6) | border),
        row(i18n::tr(i18n::I18nKey::TERMINAL_GRAPHICS_LABEL),
            terminal_graphics_toggle_->Render() | border),
    };

    return page(
        i18n::tr(i18n::I18nKey::CATEGORY_GENERAL),
        {
            section(i18n::tr(i18n::I18nKey::CLIPBOARD_SETTINGS), std::move(rows)),
            section(i18n::tr(i18n::I18nKey::PREVIEW_SETTINGS),
                    std::move(preview_rows),
                    i18n::tr(i18n::I18nKey::TERMINAL_GRAPHICS_HINT)),
        });
}

Element SettingsScreen::render_appearance() {
    using namespace settings_layout;

    auto theme_control = hbox({
        theme_toggle_->Render() | border,
        text(" "),
        text("(" + std::to_string(theme_options_.size()) + i18n::tr(i18n::I18nKey::THEME_AVAILABLE) + ")") |
            color(palette::comment()),
    });

    auto theme_rows = std::vector<Element>{
        row(i18n::tr(i18n::I18nKey::THEME_LABEL), std::move(theme_control)),
    };

    auto display_rows = std::vector<Element>{
        toggle_row(show_timestamps_cb_->Render()),
        toggle_row(show_user_colors_cb_->Render()),
        row(i18n::tr(i18n::I18nKey::TIMESTAMP_FORMAT_LABEL),
            timestamp_format_input_->Render() | size(WIDTH, GREATER_THAN, 15) | border),
        row(i18n::tr(i18n::I18nKey::MAX_MESSAGES_LABEL),
            max_messages_input_->Render() | size(WIDTH, GREATER_THAN, 10) | border),
        row(i18n::tr(i18n::I18nKey::LANGUAGE_LABEL), language_toggle_->Render() | border),
        row(i18n::tr(i18n::I18nKey::FONT_SCALE_LABEL),
            text(std::to_string(font_scale_) + "%") | color(palette::cyan())),
    };

    return page(
        i18n::tr(i18n::I18nKey::CATEGORY_APPEARANCE),
        {
            section(i18n::tr(i18n::I18nKey::THEME_SETTINGS), std::move(theme_rows)),
            section(i18n::tr(i18n::I18nKey::DISPLAY_OPTIONS),
                    std::move(display_rows),
                    i18n::tr(i18n::I18nKey::THEME_NOTE)),
        });
}

} // namespace grotto::ui
