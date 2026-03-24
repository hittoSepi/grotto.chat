#pragma once

#include <grotto/tui/admin_protocol.hpp>
#include <ftxui/component/component.hpp>
#include <ftxui/dom/elements.hpp>
#include <deque>
#include <mutex>
#include <string>

namespace grotto::tui {

class LogView {
public:
    LogView();

    // Add a log entry (thread-safe, called from socket thread)
    void push(protocol::LogEntry entry);

    // Clear all entries
    void clear();

    // FTXUI render: returns Element for the log area
    ftxui::Element render();

    // Handle scroll events. Returns true if handled.
    bool on_scroll(int delta);

private:
    ftxui::Color color_for_level(const std::string& level);

    std::deque<protocol::LogEntry> entries_;
    std::mutex mutex_;
    int scroll_offset_ = 0;      // 0 = bottom (auto-scroll), positive = scrolled up
    bool auto_scroll_ = true;
    static constexpr size_t kMaxEntries = 10000;
};

} // namespace grotto::tui
