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
                        int width) {
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
        visible.push_back(std::move(all_rows[i].element));
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
            row.graphics_image,
        });
    }

    return commands;
}

} // namespace grotto::ui
