#include "ui/settings_screen.hpp"

#include "i18n/strings.hpp"
#include "ui/color_scheme.hpp"
#include "ui/settings_layout.hpp"

#include <algorithm>

using namespace ftxui;

namespace grotto::ui {

namespace {

std::string voice_meter_bar(float level, float threshold, int width = 24) {
    const int clamped_width = std::max(8, width);
    const float clamped_level = std::clamp(level, 0.0f, 1.0f);
    const float clamped_threshold = std::clamp(threshold, 0.0f, 1.0f);
    const int filled = std::clamp(static_cast<int>(clamped_level * clamped_width + 0.5f), 0, clamped_width);
    const int threshold_index = std::clamp(static_cast<int>(clamped_threshold * clamped_width), 0, clamped_width - 1);

    std::string bar;
    bar.reserve(static_cast<size_t>(clamped_width + 2));
    bar.push_back('[');
    for (int i = 0; i < clamped_width; ++i) {
        char ch = (i < filled) ? '=' : ' ';
        if (i == threshold_index) {
            ch = (i < filled) ? '!' : '|';
        }
        bar.push_back(ch);
    }
    bar.push_back(']');
    return bar;
}

} // namespace

Element SettingsScreen::render_voice() {
    using namespace settings_layout;

    const bool is_ptt_mode = voice_mode_selected_ == 0;
    const bool is_vox_mode = voice_mode_selected_ == 1;

    auto input_volume_control = voice_input_volume_slider_->Render() | flex;
    auto output_volume_control = voice_output_volume_slider_->Render() | flex;
    auto vad_control = voice_vad_threshold_slider_->Render() | flex;
    auto limiter_control = voice_limiter_threshold_slider_->Render() | flex;
    auto jitter_control = voice_jitter_buffer_slider_->Render() | flex;

    const auto metrics = voice_test_metrics_ ? voice_test_metrics_() : VoiceTestMetrics{};
    const auto threshold_ratio =
        std::clamp(static_cast<float>(voice_vad_threshold_percent_) / 100.0f, 0.0f, 1.0f);
    const auto current_level_ratio = std::clamp(metrics.input_rms, 0.0f, 1.0f);

    auto ptt_control = hbox({
        text(voice_ptt_key_) | bold | color(palette::cyan()),
        text("  "),
        voice_capture_key_button_->Render(),
    });

    auto meter_control = text(voice_meter_bar(current_level_ratio, threshold_ratio)) |
        color(metrics.vad_open ? palette::online() : palette::cyan());
    auto meter_value = hbox({
        text(std::to_string(static_cast<int>(current_level_ratio * 100.0f + 0.5f)) + "%") |
            color(palette::cyan()),
        text("  "),
        text("VAD " + std::to_string(voice_vad_threshold_percent_) + "%") |
            color(metrics.vad_open ? palette::online() : palette::comment()),
    });

    auto stats_control = hbox({
        text("Peak " +
             std::to_string(static_cast<int>(std::clamp(metrics.input_peak, 0.0f, 1.0f) * 100.0f + 0.5f)) +
             "%") | color(palette::cyan()),
        text("  "),
        text(std::string("Limiter ") + (metrics.limiter_active ? "active" : "idle")) |
            color(metrics.limiter_active ? palette::yellow() : palette::comment()),
        text("  "),
        text(std::string("Clip ") + (metrics.clipped ? "yes" : "no")) |
            color(metrics.clipped ? palette::error_c() : palette::comment()),
        text("  "),
        text("Buffer " + std::to_string(std::max(metrics.loopback_buffer_ms, 0)) + " ms") |
            color(palette::comment()),
    });

    auto status_control = hbox({
        text(i18n::tr(voice_test_active_
                          ? i18n::I18nKey::VOICE_SELF_TEST_ACTIVE
                          : i18n::I18nKey::VOICE_SELF_TEST_INACTIVE)) |
            color(voice_test_active_ ? palette::online() : palette::comment()),
        text("  "),
        voice_self_test_button_->Render(),
    });

    auto device_rows = std::vector<Element>{
        row(i18n::tr(i18n::I18nKey::VOICE_INPUT_DEVICE_LABEL),
            voice_input_device_dropdown_->Render() | border | flex),
        row(i18n::tr(i18n::I18nKey::VOICE_OUTPUT_DEVICE_LABEL),
            voice_output_device_dropdown_->Render() | border | flex),
    };

    auto level_rows = std::vector<Element>{
        row(i18n::tr(i18n::I18nKey::VOICE_INPUT_VOLUME_LABEL),
            std::move(input_volume_control),
            text(std::to_string(voice_input_volume_value_) + "%") | color(palette::cyan())),
        row(i18n::tr(i18n::I18nKey::VOICE_OUTPUT_VOLUME_LABEL),
            std::move(output_volume_control),
            text(std::to_string(voice_output_volume_value_) + "%") | color(palette::cyan())),
    };

    auto send_rows = std::vector<Element>{
        row(i18n::tr(i18n::I18nKey::VOICE_MODE_LABEL), voice_mode_dropdown_->Render() | border),
        row(i18n::tr(i18n::I18nKey::VOICE_PTT_HOTKEY_LABEL), std::move(ptt_control), is_ptt_mode),
        row(i18n::tr(i18n::I18nKey::VOICE_VAD_THRESHOLD_LABEL),
            std::move(vad_control),
            text(std::to_string(voice_vad_threshold_percent_) + "%") | color(palette::cyan()),
            is_vox_mode),
    };

    auto processing_rows = std::vector<Element>{
        toggle_row(voice_noise_suppression_cb_->Render()),
        toggle_row(voice_limiter_cb_->Render()),
        row(i18n::tr(i18n::I18nKey::VOICE_LIMITER_THRESHOLD_LABEL),
            std::move(limiter_control),
            text(std::to_string(voice_limiter_threshold_percent_) + "%") | color(palette::cyan()),
            voice_limiter_enabled_),
        row(i18n::tr(i18n::I18nKey::VOICE_JITTER_BUFFER_LABEL),
            std::move(jitter_control),
            text(std::to_string(voice_jitter_buffer_frames_) + " fr") | color(palette::cyan())),
    };

    auto test_rows = std::vector<Element>{
        row(i18n::tr(i18n::I18nKey::VOICE_SELF_TEST_LABEL), std::move(status_control)),
        row("Level:", std::move(meter_control), std::move(meter_value)),
        row("Stats:", std::move(stats_control)),
    };

    return page(
        i18n::tr(i18n::I18nKey::CATEGORY_VOICE),
        {
            section("Laitteet", std::move(device_rows)),
            section("Tasot", std::move(level_rows)),
            section("Lähetys", std::move(send_rows), i18n::tr(i18n::I18nKey::VOICE_SETTINGS_HINT)),
            section("Käsittely", std::move(processing_rows)),
            section("Mikrofonitesti", std::move(test_rows), i18n::tr(i18n::I18nKey::VOICE_SELF_TEST_HINT)),
        });
}

} // namespace grotto::ui
