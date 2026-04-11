#include "ui/settings_screen.hpp"

#include "i18n/strings.hpp"
#include "ui/color_scheme.hpp"
#include "ui/settings_layout.hpp"

using namespace ftxui;

namespace grotto::ui {

Element SettingsScreen::render_connection() {
    using namespace settings_layout;

    auto reconnect_control = hbox({
        reconnect_delay_input_->Render() | size(WIDTH, GREATER_THAN, 5) | border,
        text(" "),
        text(i18n::tr(i18n::I18nKey::SECONDS)) | color(palette::comment()),
    });
    auto timeout_control = hbox({
        timeout_input_->Render() | size(WIDTH, GREATER_THAN, 5) | border,
        text(" "),
        text(i18n::tr(i18n::I18nKey::SECONDS)) | color(palette::comment()),
    });

    return page(
        i18n::tr(i18n::I18nKey::CATEGORY_CONNECTION),
        {
            section(i18n::tr(i18n::I18nKey::RECONNECT_BEHAVIOR),
                    {
                        toggle_row(auto_reconnect_cb_->Render()),
                        row(i18n::tr(i18n::I18nKey::RECONNECT_DELAY_LABEL), std::move(reconnect_control)),
                    }),
            section(i18n::tr(i18n::I18nKey::TIMEOUT_SETTINGS),
                    {
                        row(i18n::tr(i18n::I18nKey::CONNECTION_TIMEOUT_LABEL), std::move(timeout_control)),
                    }),
            section(i18n::tr(i18n::I18nKey::TLS_OPTIONS),
                    {
                        toggle_row(tls_verify_cb_->Render()),
                        row(i18n::tr(i18n::I18nKey::CERT_PIN_LABEL),
                            cert_pin_input_->Render() | size(WIDTH, GREATER_THAN, 40) | border),
                    },
                    i18n::tr(i18n::I18nKey::CONNECTION_NOTE)),
        });
}

Element SettingsScreen::render_notifications() {
    using namespace settings_layout;

    return page(
        i18n::tr(i18n::I18nKey::CATEGORY_NOTIFICATIONS),
        {
            section(i18n::tr(i18n::I18nKey::NOTIFICATION_SETTINGS),
                    {
                        toggle_row(desktop_notif_cb_->Render()),
                        toggle_row(sound_alerts_cb_->Render()),
                    }),
            section(i18n::tr(i18n::I18nKey::MENTION_SETTINGS),
                    {
                        toggle_row(mention_cb_->Render()),
                        toggle_row(dm_cb_->Render()),
                        row(i18n::tr(i18n::I18nKey::MENTION_KEYWORDS_LABEL),
                            keywords_input_->Render() | size(WIDTH, GREATER_THAN, 30) | border),
                    },
                    i18n::tr(i18n::I18nKey::MENTION_KEYWORDS_HINT)),
        });
}

Element SettingsScreen::render_privacy() {
    using namespace settings_layout;

    return page(
        i18n::tr(i18n::I18nKey::CATEGORY_PRIVACY),
        {
            section(i18n::tr(i18n::I18nKey::PRIVACY_SETTINGS),
                    {
                        toggle_row(share_typing_indicators_cb_->Render()),
                        toggle_row(share_read_receipts_cb_->Render()),
                        toggle_row(auto_away_cb_->Render()),
                        row(i18n::tr(i18n::I18nKey::AUTO_AWAY_MINUTES_LABEL),
                            auto_away_minutes_input_->Render() | size(WIDTH, GREATER_THAN, 8) | border,
                            auto_away_enabled_),
                    },
                    i18n::tr(i18n::I18nKey::AUTO_AWAY_NOTE) + " " +
                        i18n::tr(i18n::I18nKey::PRIVACY_NOTE)),
        });
}

Element SettingsScreen::render_account() {
    using namespace settings_layout;

    std::string pk_display = public_key_hex_.empty() ? i18n::tr(i18n::I18nKey::PUBLIC_KEY_NOT_AVAILABLE) : public_key_hex_;
    if (pk_display.size() > 50) {
        pk_display = pk_display.substr(0, 47) + "...";
    }

    auto import_export_actions = hbox({
        export_button_persistent_->Render(),
        text("  "),
        import_button_persistent_->Render(),
    });

    return page(
        i18n::tr(i18n::I18nKey::CATEGORY_ACCOUNT),
        {
            section(i18n::tr(i18n::I18nKey::ACCOUNT_SETTINGS),
                    {
                        row(i18n::tr(i18n::I18nKey::NICKNAME_LABEL),
                            nickname_input_->Render() | size(WIDTH, GREATER_THAN, 25) | border),
                    },
                    i18n::tr(i18n::I18nKey::NICKNAME_HINT)),
            section(i18n::tr(i18n::I18nKey::PUBLIC_KEY_LABEL),
                    {
                        action_row(text(pk_display) | color(palette::cyan())),
                    }),
            section(i18n::tr(i18n::I18nKey::IMPORT_EXPORT),
                    {
                        action_row(std::move(import_export_actions)),
                    }),
            section(i18n::tr(i18n::I18nKey::DANGER_ZONE),
                    {
                        action_row(logout_button_persistent_->Render() | color(palette::red())),
                    }),
        });
}

} // namespace grotto::ui
