#include "ui/message_view.hpp"

#include "i18n/strings.hpp"
#include "ui/color_scheme.hpp"
#include "ui/graphics_layout.hpp"
#include "ui/markdown_renderer.hpp"

#include <algorithm>
#include <ctime>
#include <optional>
#include <regex>
#include <string>
#include <utility>
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

int visible_width(std::string_view text) {
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

size_t byte_offset_for_display_column(std::string_view text, int display_column) {
    if (display_column <= 0) {
        return 0;
    }

    int col = 0;
    size_t i = 0;
    while (i < text.size() && col < display_column) {
        unsigned char c = static_cast<unsigned char>(text[i]);
        size_t cp_len = 1;
        if ((c & 0x80) == 0) cp_len = 1;
        else if ((c & 0xE0) == 0xC0) cp_len = 2;
        else if ((c & 0xF0) == 0xE0) cp_len = 3;
        else if ((c & 0xF8) == 0xF0) cp_len = 4;
        i += cp_len;
        ++col;
    }
    return i;
}

std::string substring_by_display_columns(const std::string& text, int start_col, int end_col) {
    if (end_col <= start_col) {
        return {};
    }
    const int text_cols = visible_width(text);
    const int start = std::clamp(start_col, 0, text_cols);
    const int end = std::clamp(end_col, start, text_cols);
    if (end <= start) {
        return {};
    }
    const size_t start_byte = byte_offset_for_display_column(text, start);
    const size_t end_byte = byte_offset_for_display_column(text, end);
    if (end_byte <= start_byte || start_byte >= text.size()) {
        return {};
    }
    return text.substr(start_byte, end_byte - start_byte);
}

std::optional<std::string> find_url_in_text(const std::string& text) {
    static const std::regex url_re(R"(((?:https?://|www\.)\S+))");
    std::smatch match;
    if (!std::regex_search(text, match, url_re)) {
        return std::nullopt;
    }

    std::string url = match[1].str();
    if (url.starts_with("www.")) {
        url = "https://" + url;
    }
    while (!url.empty() && (url.back() == ')' || url.back() == ']' || url.back() == '.' ||
                            url.back() == ',' || url.back() == ';')) {
        url.pop_back();
    }
    return url.empty() ? std::nullopt : std::optional<std::string>(url);
}

std::vector<MessageRenderPart> fallback_render_parts(const Message& msg) {
    if (!msg.render_parts.empty()) {
        return msg.render_parts;
    }

    std::vector<MessageRenderPart> parts;
    if (!msg.content.empty()) {
        parts.push_back({MessageRenderPart::Kind::Text, msg.content, std::nullopt});
    }
    if (msg.inline_image) {
        parts.push_back({MessageRenderPart::Kind::Image, {}, msg.inline_image});
    }
    return parts;
}

std::vector<LayoutRow> render_text_block(const Message& msg,
                                         int message_index,
                                         int block_index,
                                         const std::string& ts_prefix,
                                         int width,
                                         const std::string& text_content) {
    std::vector<LayoutRow> rows;

    if (msg.type == Message::Type::System || msg.type == Message::Type::VoiceEvent ||
        msg.type == Message::Type::Preview) {
        const std::string first_prefix = (msg.type == Message::Type::Preview) ? "" : "• ";
        const std::string continuation_prefix(visible_width(first_prefix), ' ');
        const int content_width = std::max(1, width - visible_width(ts_prefix) - visible_width(first_prefix));
        auto content_lines = render_markdown_lines(text_content, content_width);
        auto plain_lines = render_markdown_plain_lines(text_content, content_width);

        if (content_lines.empty()) {
            content_lines.push_back(text(""));
            plain_lines.push_back("");
        }

        for (size_t i = 0; i < content_lines.size(); ++i) {
            const std::string current_ts = (i == 0 ? ts_prefix : std::string(ts_prefix.size(), ' '));
            const std::string bullet_prefix = (i == 0 ? first_prefix : continuation_prefix);
            rows.push_back({
                message_index,
                block_index,
                LayoutBlockKind::Text,
                current_ts + bullet_prefix + plain_lines[i],
                find_url_in_text(plain_lines[i]),
                false,
                hbox({
                    text(current_ts) | color(palette::comment()),
                    text(bullet_prefix) | color(msg.type == Message::Type::Preview ? palette::blue1() : palette::yellow()),
                    std::move(content_lines[i]) |
                        color(msg.type == Message::Type::Preview ? palette::blue1() : palette::yellow()) | flex,
                }),
            });
        }
        return rows;
    }

    const std::string nick = "<" + msg.sender_id + "> ";
    const std::string continuation_prefix(visible_width(nick), ' ');
    const int content_width = std::max(1, width - visible_width(ts_prefix) - visible_width(nick));
    auto content_lines = render_markdown_lines(text_content, content_width);
    auto plain_lines = render_markdown_plain_lines(text_content, content_width);

    if (content_lines.empty()) {
        content_lines.push_back(text(""));
        plain_lines.push_back("");
    }

    for (size_t i = 0; i < content_lines.size(); ++i) {
        const std::string current_ts = (i == 0 ? ts_prefix : std::string(ts_prefix.size(), ' '));
        const std::string nick_prefix = (i == 0 ? nick : continuation_prefix);
        rows.push_back({
            message_index,
            block_index,
            LayoutBlockKind::Text,
            current_ts + nick_prefix + plain_lines[i],
            find_url_in_text(plain_lines[i]),
            false,
            hbox({
                text(current_ts) | color(palette::comment()),
                text(nick_prefix) |
                    (i == 0 ? ftxui::color(nick_color(msg.sender_id)) : color(palette::fg())),
                std::move(content_lines[i]) | flex,
            }),
        });
    }
    return rows;
}

std::vector<LayoutRow> render_one_message(const Message& msg,
                                          int message_index,
                                          const std::string& ts_fmt,
                                          int width) {
    std::vector<LayoutRow> rows;
    const std::string ts = format_ts(msg.timestamp_ms, ts_fmt) + " ";
    const auto parts = fallback_render_parts(msg);

    bool first_block = true;
    int block_index = 0;
    for (const auto& part : parts) {
        const std::string ts_prefix = first_block ? ts : std::string(ts.size(), ' ');
        first_block = false;

        if (part.kind == MessageRenderPart::Kind::Image && part.image) {
            auto image_rows = render_graphics_rows(
                *part.image,
                message_index,
                block_index,
                ts_prefix,
                std::max(1, width),
                {std::max(8, width - visible_width(ts_prefix)), 20});
            rows.insert(rows.end(),
                        std::make_move_iterator(image_rows.begin()),
                        std::make_move_iterator(image_rows.end()));
            ++block_index;
            continue;
        }

        if (part.kind == MessageRenderPart::Kind::Spacer) {
            rows.push_back({
                message_index,
                block_index++,
                LayoutBlockKind::Spacer,
                "",
                std::nullopt,
                false,
                text(""),
            });
            continue;
        }

        auto text_rows = render_text_block(
            msg, message_index, block_index, ts_prefix, std::max(1, width), part.text);
        rows.insert(rows.end(),
                    std::make_move_iterator(text_rows.begin()),
                    std::make_move_iterator(text_rows.end()));
        ++block_index;
    }

    if (msg.type == Message::Type::Chat && msg.read_by_remote) {
        const std::string ts_prefix(ts.size(), ' ');
        const std::string nick_prefix(visible_width("<" + msg.sender_id + "> "), ' ');
        const std::string label = i18n::tr(i18n::I18nKey::READ_RECEIPT_READ);
        rows.push_back({
            message_index,
            block_index,
            LayoutBlockKind::Text,
            ts_prefix + nick_prefix + label,
            std::nullopt,
            false,
            hbox({
                text(ts_prefix) | color(palette::comment()),
                text(nick_prefix),
                text(label) | color(palette::comment()) | dim,
            }),
        });
    }

    return rows;
}

std::vector<LayoutRow> flatten_message_rows(const ChannelState& state,
                                            const std::string& timestamp_format,
                                            int width) {
    std::vector<LayoutRow> all_rows;
    for (size_t i = 0; i < state.messages.size(); ++i) {
        auto rows = render_one_message(state.messages[i], static_cast<int>(i), timestamp_format, width);
        all_rows.insert(all_rows.end(),
                        std::make_move_iterator(rows.begin()),
                        std::make_move_iterator(rows.end()));
    }
    return all_rows;
}

std::pair<int, int> visible_row_window(int total, int visible_rows, int scroll_offset) {
    const int bottom_idx = std::clamp(total - 1 - scroll_offset, 0, std::max(0, total - 1));
    const int top_idx = std::max(0, bottom_idx - visible_rows + 1);
    return {top_idx, bottom_idx};
}

} // anonymous namespace

Element render_messages(const ChannelState& state,
                        const std::string& timestamp_format,
                        int visible_rows,
                        int width,
                        int selected_start_row,
                        int selected_start_col,
                        int selected_end_row,
                        int selected_end_col) {
    if (state.messages.empty()) {
        return text(i18n::tr(i18n::I18nKey::NO_MESSAGES)) | color(palette::comment()) | center;
    }

    auto all_rows = flatten_message_rows(state, timestamp_format, width);
    if (all_rows.empty()) {
        return text(i18n::tr(i18n::I18nKey::NO_MESSAGES)) | color(palette::comment()) | center;
    }

    const auto [top_idx, bottom_idx] = visible_row_window(
        static_cast<int>(all_rows.size()), visible_rows, state.scroll_offset);

    Elements visible;
    visible.reserve(bottom_idx - top_idx + 1);
    for (int i = top_idx; i <= bottom_idx; ++i) {
        const auto& row = all_rows[i];
        auto row_el = std::move(all_rows[i].element);
        const int visible_row = i - top_idx;
        if (selected_start_row >= 0 &&
            selected_end_row >= selected_start_row &&
            visible_row >= selected_start_row &&
            visible_row <= selected_end_row) {
            const int line_cols = visible_width(row.plain_text);
            int from_col = 0;
            int to_col = line_cols;
            if (selected_start_row == selected_end_row) {
                from_col = selected_start_col;
                to_col = selected_end_col + 1;
            } else if (visible_row == selected_start_row) {
                from_col = selected_start_col;
            } else if (visible_row == selected_end_row) {
                to_col = selected_end_col + 1;
            }

            from_col = std::clamp(from_col, 0, line_cols);
            to_col = std::clamp(to_col, from_col, line_cols);

            if (to_col > from_col && line_cols > 0) {
                const std::string left = substring_by_display_columns(row.plain_text, 0, from_col);
                const std::string mid = substring_by_display_columns(row.plain_text, from_col, to_col);
                const std::string right = substring_by_display_columns(row.plain_text, to_col, line_cols);
                row_el = hbox({
                    text(left),
                    text(mid) | bgcolor(palette::bg_highlight()),
                    text(right),
                });
            } else {
                row_el = std::move(row_el) | bgcolor(palette::bg_highlight());
            }
        }
        visible.push_back(std::move(row_el));
    }
    return vbox(std::move(visible));
}

std::vector<VisibleLayoutHit> collect_visible_layout_hits(
    const ChannelState& state,
    const std::string& timestamp_format,
    int visible_rows,
    int width) {
    std::vector<VisibleLayoutHit> hits;
    if (state.messages.empty()) {
        return hits;
    }

    auto all_rows = flatten_message_rows(state, timestamp_format, width);
    if (all_rows.empty()) {
        return hits;
    }

    const auto [top_idx, bottom_idx] = visible_row_window(
        static_cast<int>(all_rows.size()), visible_rows, state.scroll_offset);
    hits.reserve(bottom_idx - top_idx + 1);
    for (int i = top_idx; i <= bottom_idx; ++i) {
        hits.push_back({
            all_rows[i].message_index,
            all_rows[i].block_index,
            all_rows[i].block_kind,
            all_rows[i].plain_text,
            all_rows[i].url,
            all_rows[i].has_inline_image,
        });
    }
    return hits;
}

std::vector<GraphicsDrawCommand> collect_visible_draw_commands(
    const ChannelState& state,
    const std::string& timestamp_format,
    int visible_rows,
    int width,
    int viewport_x,
    int viewport_y) {
    std::vector<GraphicsDrawCommand> commands;
    if (state.messages.empty()) {
        return commands;
    }

    auto all_rows = flatten_message_rows(state, timestamp_format, width);
    if (all_rows.empty()) {
        return commands;
    }

    const auto [top_idx, bottom_idx] = visible_row_window(
        static_cast<int>(all_rows.size()), visible_rows, state.scroll_offset);

    for (int i = top_idx; i <= bottom_idx; ++i) {
        const auto& row = all_rows[i];
        if (!row.starts_graphics_block ||
            row.graphics_backend == GraphicsBackendKind::ColorBlocks ||
            !row.graphics_image ||
            row.graphics_columns <= 0 ||
            row.graphics_rows <= 0) {
            continue;
        }

        commands.push_back({
            row.graphics_backend,
            row.message_index,
            row.block_index,
            viewport_x + row.graphics_offset_x,
            viewport_y + (i - top_idx),
            row.graphics_columns,
            row.graphics_rows,
            0,
            row.graphics_image,
        });
    }

    return commands;
}

} // namespace grotto::ui
