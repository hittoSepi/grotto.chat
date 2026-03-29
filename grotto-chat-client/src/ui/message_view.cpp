#include "ui/message_view.hpp"
#include "ui/color_scheme.hpp"
#include "ui/markdown_renderer.hpp"
#include "i18n/strings.hpp"
#include <ftxui/dom/elements.hpp>
#include <algorithm>
#include <cstdlib>
#include <ctime>
#include <string>
#include <vector>

using namespace ftxui;

namespace grotto::ui {

namespace {

struct RenderedLine {
    int message_index = -1;
    std::string plain_text;
    Element element;
};

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

bool supports_truecolor_preview() {
#ifdef _WIN32
    return true;
#else
    const char* color_term = std::getenv("COLORTERM");
    if (color_term) {
        std::string value = color_term;
        if (value.find("truecolor") != std::string::npos ||
            value.find("24bit") != std::string::npos) {
            return true;
        }
    }

    const char* term_program = std::getenv("TERM_PROGRAM");
    if (term_program) {
        std::string value = term_program;
        if (value == "iTerm.app" || value == "WezTerm" || value == "vscode") {
            return true;
        }
    }

    const char* term = std::getenv("TERM");
    if (term) {
        std::string value = term;
        if (value.find("direct") != std::string::npos || value.find("kitty") != std::string::npos) {
            return true;
        }
    }
    return false;
#endif
}

std::vector<RenderedLine> render_color_preview(const Message& msg,
                                               int message_index,
                                               const std::string& ts,
                                               int width) {
    std::vector<RenderedLine> rows;

    std::vector<std::string> header_lines;
    size_t start = 0;
    while (start <= msg.content.size()) {
        size_t end = msg.content.find('\n', start);
        std::string line = (end == std::string::npos)
            ? msg.content.substr(start)
            : msg.content.substr(start, end - start);
        if (line.empty()) {
            break;
        }
        header_lines.push_back(line);
        if (end == std::string::npos) {
            break;
        }
        start = end + 1;
    }

    const int content_width = std::max(1, width - visible_width(ts));
    for (size_t i = 0; i < header_lines.size(); ++i) {
        std::string line = header_lines[i];
        if (visible_width(line) > content_width) {
            line.resize(static_cast<size_t>(content_width));
        }
        const std::string ts_prefix = (i == 0 ? ts : std::string(ts.size(), ' '));
        rows.push_back({
            message_index,
            ts_prefix + line,
            hbox({
                text(ts_prefix) | color(palette::comment()),
                text(line) | color(palette::blue1()) | flex,
            })
        });
    }

    const auto& thumbnail = *msg.inline_image;
    for (int y = 0; y < thumbnail.height; y += 2) {
        Elements cells;
        const std::string ts_prefix = rows.empty() && y == 0 ? ts : std::string(ts.size(), ' ');
        cells.push_back(text(ts_prefix) | color(palette::comment()));

        for (int x = 0; x < thumbnail.width; ++x) {
            size_t top = static_cast<size_t>((y * thumbnail.width + x) * 4);
            size_t bottom = static_cast<size_t>(((std::min(y + 1, thumbnail.height - 1)) * thumbnail.width + x) * 4);
            ftxui::Color fg = Color::RGB(thumbnail.rgba[top + 0], thumbnail.rgba[top + 1], thumbnail.rgba[top + 2]);
            ftxui::Color bg = Color::RGB(thumbnail.rgba[bottom + 0], thumbnail.rgba[bottom + 1], thumbnail.rgba[bottom + 2]);
            cells.push_back(text("▀") | color(fg) | bgcolor(bg));
        }

        rows.push_back({
            message_index,
            ts_prefix + std::string(static_cast<size_t>(thumbnail.width), '#'),
            hbox(std::move(cells))
        });
    }

    return rows;
}

std::vector<RenderedLine> render_one_lines(const Message& msg,
                                           int message_index,
                                           const std::string& ts_fmt,
                                           int width) {
    const std::string ts = format_ts(msg.timestamp_ms, ts_fmt) + " ";
    width = std::max(1, width);

    if (msg.type == Message::Type::Preview) {
        if (msg.inline_image && supports_truecolor_preview()) {
            return render_color_preview(msg, message_index, ts, width);
        }

        std::vector<RenderedLine> rows;
        std::vector<std::string> preview_lines;
        size_t start = 0;
        while (start <= msg.content.size()) {
            size_t end = msg.content.find('\n', start);
            if (end == std::string::npos) {
                preview_lines.push_back(msg.content.substr(start));
                break;
            }
            preview_lines.push_back(msg.content.substr(start, end - start));
            start = end + 1;
        }
        if (preview_lines.empty()) {
            preview_lines.push_back("");
        }

        const int content_width = std::max(1, width - visible_width(ts));
        for (size_t i = 0; i < preview_lines.size(); ++i) {
            std::string line = preview_lines[i];
            if (visible_width(line) > content_width) {
                line.resize(static_cast<size_t>(content_width));
            }
            const std::string ts_prefix = (i == 0 ? ts : std::string(ts.size(), ' '));
            rows.push_back({
                message_index,
                ts_prefix + line,
                hbox({
                    text(ts_prefix) | color(palette::comment()),
                    text(line) | color(palette::blue1()) | flex,
                })
            });
        }
        return rows;
    }

    if (msg.type == Message::Type::System || msg.type == Message::Type::VoiceEvent) {
        const std::string first_prefix = "• ";
        const std::string continuation_prefix(visible_width(first_prefix), ' ');
        const int content_width = std::max(1, width - visible_width(ts) - visible_width(first_prefix));
        auto content_lines = render_markdown_lines(msg.content, content_width);
        auto plain_lines = render_markdown_plain_lines(msg.content, content_width);

        std::vector<RenderedLine> rows;
        for (size_t i = 0; i < content_lines.size(); ++i) {
            const std::string ts_prefix = (i == 0 ? ts : std::string(ts.size(), ' '));
            const std::string bullet_prefix = (i == 0 ? first_prefix : continuation_prefix);
            rows.push_back({
                message_index,
                ts_prefix + bullet_prefix + plain_lines[i],
                hbox({
                    text(ts_prefix) | color(palette::comment()),
                    text(bullet_prefix) | color(palette::yellow()),
                    std::move(content_lines[i]) | color(palette::yellow()) | flex,
                })
            });
        }
        return rows;
    }

    const std::string nick = "<" + msg.sender_id + "> ";
    const std::string continuation_prefix(visible_width(nick), ' ');
    const int content_width = std::max(1, width - visible_width(ts) - visible_width(nick));
    auto content_lines = render_markdown_lines(msg.content, content_width);
    auto plain_lines = render_markdown_plain_lines(msg.content, content_width);

    std::vector<RenderedLine> rows;
    for (size_t i = 0; i < content_lines.size(); ++i) {
        const std::string ts_prefix = (i == 0 ? ts : std::string(ts.size(), ' '));
        const std::string nick_prefix = (i == 0 ? nick : continuation_prefix);
        rows.push_back({
            message_index,
            ts_prefix + nick_prefix + plain_lines[i],
            hbox({
                text(ts_prefix) | color(palette::comment()),
                text(nick_prefix) |
                    (i == 0 ? ftxui::color(nick_color(msg.sender_id)) : color(palette::fg())),
                std::move(content_lines[i]) | flex,
            })
        });
    }
    return rows;
}

std::vector<RenderedLine> flatten_message_lines(const ChannelState& state,
                                                const std::string& timestamp_format,
                                                int width) {
    std::vector<RenderedLine> all_lines;
    for (size_t i = 0; i < state.messages.size(); ++i) {
        auto rows = render_one_lines(state.messages[i], static_cast<int>(i), timestamp_format, width);
        all_lines.insert(all_lines.end(),
                         std::make_move_iterator(rows.begin()),
                         std::make_move_iterator(rows.end()));
    }
    return all_lines;
}

} // anonymous namespace

Element render_messages(const ChannelState& state,
                        const std::string& timestamp_format,
                        int visible_rows,
                        int width) {
    if (state.messages.empty()) {
        return text(i18n::tr(i18n::I18nKey::NO_MESSAGES)) | color(palette::comment()) | center;
    }

    auto all_lines = flatten_message_lines(state, timestamp_format, width);

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
        visible.push_back(std::move(all_lines[i].element));
    }

    return vbox(std::move(visible));
}

std::vector<VisibleMessageLine> collect_visible_message_lines(
    const ChannelState& state,
    const std::string& timestamp_format,
    int visible_rows,
    int width) {
    std::vector<VisibleMessageLine> visible_lines;
    if (state.messages.empty()) {
        return visible_lines;
    }

    auto all_lines = flatten_message_lines(state, timestamp_format, width);
    if (all_lines.empty()) {
        return visible_lines;
    }

    const int total = static_cast<int>(all_lines.size());
    int bottom_idx = total - 1 - state.scroll_offset;
    bottom_idx = std::clamp(bottom_idx, 0, total - 1);
    int top_idx = std::max(0, bottom_idx - visible_rows + 1);

    visible_lines.reserve(bottom_idx - top_idx + 1);
    for (int i = top_idx; i <= bottom_idx; ++i) {
        visible_lines.push_back({all_lines[i].message_index, all_lines[i].plain_text});
    }

    return visible_lines;
}

} // namespace grotto::ui
