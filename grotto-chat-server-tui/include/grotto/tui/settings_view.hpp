#pragma once

#include <grotto/tui/admin_protocol.hpp>
#include <ftxui/component/component.hpp>
#include <ftxui/component/event.hpp>
#include <ftxui/dom/elements.hpp>
#include <functional>
#include <mutex>
#include <string>
#include <vector>

namespace grotto::tui {

using ConfigChangeCallback = std::function<void(const std::string& key, const std::string& value)>;

class SettingsView {
public:
    explicit SettingsView(ConfigChangeCallback on_change);

    // Update config entries (thread-safe)
    void set_config(std::vector<protocol::ConfigEntry> entries);

    // FTXUI render
    ftxui::Element render();

    // Handle key events (Up/Down navigate, Enter edit, Escape cancel)
    bool on_key(ftxui::Event& event);

private:
    std::vector<protocol::ConfigEntry> entries_;
    std::mutex mutex_;
    int selected_ = 0;
    bool editing_ = false;
    std::string edit_value_;
    ConfigChangeCallback on_change_;
};

} // namespace grotto::tui
