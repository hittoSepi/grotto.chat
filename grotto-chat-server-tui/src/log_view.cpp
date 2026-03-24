#include <grotto/tui/log_view.hpp>

#include <algorithm>
#include <cctype>

using namespace ftxui;

namespace grotto::tui {

LogView::LogView() = default;

void LogView::push(protocol::LogEntry entry) {
    std::lock_guard<std::mutex> lock(mutex_);
    entries_.push_back(std::move(entry));
    if (entries_.size() > kMaxEntries) {
        entries_.pop_front();
    }
    if (auto_scroll_) {
        scroll_offset_ = 0;
    }
}

void LogView::clear() {
    std::lock_guard<std::mutex> lock(mutex_);
    entries_.clear();
    scroll_offset_ = 0;
    auto_scroll_ = true;
}

ftxui::Element LogView::render() {
    std::lock_guard<std::mutex> lock(mutex_);

    if (entries_.empty()) {
        return vbox({
            text("No log entries.") | color(Color::GrayDark),
        }) | yflex;
    }

    // Determine visible range based on scroll offset
    int total = static_cast<int>(entries_.size());
    int end = total - scroll_offset_;
    if (end < 0) end = 0;

    Elements lines;
    // We render all entries from 0..end and let FTXUI handle overflow.
    // The vbox with yflex + focus at bottom achieves auto-scroll.
    for (int i = 0; i < end; ++i) {
        const auto& entry = entries_[static_cast<size_t>(i)];

        // Extract HH:MM:SS from ISO 8601 timestamp
        std::string time_str;
        if (entry.ts.size() >= 19) {
            time_str = entry.ts.substr(11, 8);
        } else {
            time_str = entry.ts;
        }

        // Uppercase the level
        std::string upper_level = entry.level;
        for (auto& c : upper_level) {
            c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
        }

        lines.push_back(hbox({
            text("[" + time_str + "]") | color(Color::GrayDark),
            text(" "),
            text("[" + upper_level + "]") | color(color_for_level(entry.level)),
            text(" "),
            text(entry.msg) | color(Color::White),
        }));
    }

    return vbox(std::move(lines)) | yflex;
}

bool LogView::on_scroll(int delta) {
    std::lock_guard<std::mutex> lock(mutex_);

    scroll_offset_ += delta;

    int max_offset = static_cast<int>(entries_.size());
    if (scroll_offset_ < 0) scroll_offset_ = 0;
    if (scroll_offset_ > max_offset) scroll_offset_ = max_offset;

    auto_scroll_ = (scroll_offset_ == 0);
    return true;
}

ftxui::Color LogView::color_for_level(const std::string& level) {
    if (level == "debug")   return Color::GrayDark;
    if (level == "info")    return Color::White;
    if (level == "warn" || level == "warning") return Color::Yellow;
    if (level == "error")   return Color::Red;
    return Color::White;
}

} // namespace grotto::tui
