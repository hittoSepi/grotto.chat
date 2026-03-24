#include <grotto/tui/settings_view.hpp>

#include <algorithm>

namespace grotto::tui {

using namespace ftxui;

SettingsView::SettingsView(ConfigChangeCallback on_change)
    : on_change_(std::move(on_change)) {}

void SettingsView::set_config(std::vector<protocol::ConfigEntry> entries) {
    std::lock_guard<std::mutex> lock(mutex_);
    entries_ = std::move(entries);
    if (selected_ >= static_cast<int>(entries_.size())) {
        selected_ = entries_.empty() ? 0 : static_cast<int>(entries_.size()) - 1;
    }
}

Element SettingsView::render() {
    std::lock_guard<std::mutex> lock(mutex_);

    Elements rows;
    rows.push_back(text("Asetukset") | bold);
    rows.push_back(separator());

    if (entries_.empty()) {
        rows.push_back(text("  Ei asetuksia ladattu.") | color(Color::GrayDark));
        return vbox(std::move(rows));
    }

    for (int i = 0; i < static_cast<int>(entries_.size()); ++i) {
        const auto& entry = entries_[i];

        Element value_elem;
        if (editing_ && i == selected_) {
            // Show edit cursor
            value_elem = text(edit_value_ + "\xe2\x96\x88") | color(Color::Yellow);
        } else {
            value_elem = text(entry.value) | color(entry.read_only ? Color::GrayDark : Color::White);
        }

        Element ro_label = entry.read_only
            ? text("  (restart)") | color(Color::GrayDark)
            : text("");

        auto row = hbox({
            text(entry.key) | size(WIDTH, EQUAL, 25),
            text(" = "),
            value_elem,
            ro_label,
        });

        if (i == selected_) {
            row = row | inverted;
        }

        rows.push_back(row);
    }

    return vbox(std::move(rows));
}

bool SettingsView::on_key(ftxui::Event& event) {
    std::lock_guard<std::mutex> lock(mutex_);

    if (entries_.empty()) {
        return false;
    }

    if (!editing_) {
        if (event == Event::ArrowUp) {
            if (selected_ > 0) --selected_;
            return true;
        }
        if (event == Event::ArrowDown) {
            if (selected_ < static_cast<int>(entries_.size()) - 1) ++selected_;
            return true;
        }
        if (event == Event::Return) {
            if (selected_ >= 0 && selected_ < static_cast<int>(entries_.size()) &&
                !entries_[selected_].read_only) {
                editing_ = true;
                edit_value_ = entries_[selected_].value;
            }
            return true;
        }
        return false;
    }

    // Editing mode
    if (event == Event::Escape) {
        editing_ = false;
        return true;
    }
    if (event == Event::Return) {
        const auto& key = entries_[selected_].key;
        entries_[selected_].value = edit_value_;
        editing_ = false;
        if (on_change_) {
            on_change_(key, edit_value_);
        }
        return true;
    }
    if (event == Event::Backspace) {
        if (!edit_value_.empty()) {
            edit_value_.pop_back();
        }
        return true;
    }
    // Character input
    if (event.is_character()) {
        edit_value_ += event.character();
        return true;
    }

    return false;
}

} // namespace grotto::tui
