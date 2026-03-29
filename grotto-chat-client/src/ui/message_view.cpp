#include "ui/message_view.hpp"
#include "ui/color_scheme.hpp"
#include "ui/markdown_renderer.hpp"
#include "i18n/strings.hpp"
#include <ftxui/dom/elements.hpp>
#include <algorithm>
#include <ctime>
#include <string>
#include <vector>

using namespace ftxui;

namespace grotto::ui {

namespace {

static std::string format_ts(int64_t ms, const std::string& fmt) {
    if (ms == 0) return "[--:--]";
    time_t secs = static_cast<time_t>(ms / 1000);
    struct tm tm_info{};
#ifdef _WIN32
    localtime_s(&tm_info, &secs);
#else
    localtime_r(&secs, &tm_info);
#endif
    char buf[32];
    strftime(buf, sizeof(buf), fmt.c_str(), &tm_info);
    return std::string("[") + buf + "]";
}

int visible_width(const std::string& text) {
    int width = 0;
    for (size_t i = 0; i < text.size();) {
        unsigned char c = static_cast<unsigned char>(text[i]);
        if ((c & 0x80) == 0) i += 1;
        else if ((c & 0xE0) == 0xC0) i += 2;
        else if ((c & 0xF0) == 0xE0) i += 3;
        else if ((c & 0xF8) == 0xF0) i += 4;
        else i += 1;
        ++width;
    }
    return width;
}

std::vector<Element> render_one_lines(const Message& msg,
                                      const std::string& ts_fmt,
                                      int width) {
    const std::string ts = format_ts(msg.timestamp_ms, ts_fmt) + " ";
    width = std::max(1, width);

    if (msg.type == Message::Type::System || msg.type == Message::Type::VoiceEvent) {
        const std::string first_prefix = "• ";
        const std::string continuation_prefix(visible_width(first_prefix), ' ');
        const int content_width = std::max(1, width - visible_width(ts) - visible_width(first_prefix));
        auto content_lines = render_markdown_lines(msg.content, content_width);

        std::vector<Element> rows;
        for (size_t i = 0; i < content_lines.size(); ++i) {
            rows.push_back(hbox({
                text(i == 0 ? ts : std::string(ts.size(), ' ')) | color(palette::comment()),
                text(i == 0 ? first_prefix : continuation_prefix) | color(palette::yellow()),
                std::move(content_lines[i]) | color(palette::yellow()) | flex,
            }));
        }
        return rows;
    }

    const std::string nick = "<" + msg.sender_id + "> ";
    const std::string continuation_prefix(visible_width(nick), ' ');
    const int content_width = std::max(1, width - visible_width(ts) - visible_width(nick));
    auto content_lines = render_markdown_lines(msg.content, content_width);

    std::vector<Element> rows;
    for (size_t i = 0; i < content_lines.size(); ++i) {
        rows.push_back(hbox({
            text(i == 0 ? ts : std::string(ts.size(), ' ')) | color(palette::comment()),
            text(i == 0 ? nick : continuation_prefix) |
                (i == 0 ? ftxui::color(nick_color(msg.sender_id)) : color(palette::fg())),
            std::move(content_lines[i]) | flex,
        }));
    }
    return rows;
}

} // anonymous namespace

Element render_messages(const ChannelState& state,
                        const std::string& timestamp_format,
                        int visible_rows,
                        int width) {
    if (state.messages.empty()) {
        return text(i18n::tr(i18n::I18nKey::NO_MESSAGES)) | color(palette::comment()) | center;
    }

    std::vector<Element> all_lines;
    for (const auto& msg : state.messages) {
        auto rows = render_one_lines(msg, timestamp_format, width);
        all_lines.insert(all_lines.end(),
                         std::make_move_iterator(rows.begin()),
                         std::make_move_iterator(rows.end()));
    }

    if (all_lines.empty()) {
        return text(i18n::tr(i18n::I18nKey::NO_MESSAGES)) | color(palette::comment()) | center;
    }

    const int total = static_cast<int>(all_lines.size());
    int bottom_idx = total - 1 - state.scroll_offset;
    bottom_idx = std::clamp(bottom_idx, 0, total - 1);
    int top_idx = std::max(0, bottom_idx - visible_rows + 1);

    Elements visible;
    visible.reserve(bottom_idx - top_idx + 1);
    for (int i = top_idx; i <= bottom_idx; ++i) {
        visible.push_back(std::move(all_lines[i]));
    }

    return vbox(std::move(visible));
}

} // namespace grotto::ui
