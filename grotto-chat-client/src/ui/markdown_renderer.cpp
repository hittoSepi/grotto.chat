#include "ui/markdown_renderer.hpp"
#include "ui/color_scheme.hpp"
#include <ftxui/dom/elements.hpp>
#include <sstream>
#include <string>
#include <vector>

using namespace ftxui;

namespace grotto::ui {

namespace {

enum class InlineStyle {
    Normal,
    Bold,
    Italic,
    Code,
    Url,
    Heading,
    Quote,
    Bullet,
};

struct InlineToken {
    std::string text;
    InlineStyle style = InlineStyle::Normal;
};

Element apply_style(Element element, InlineStyle style) {
    switch (style) {
    case InlineStyle::Bold:
        return std::move(element) | bold | color(Color::White);
    case InlineStyle::Italic:
        return std::move(element) | italic | color(palette::fg_dark());
    case InlineStyle::Code:
        return std::move(element) | color(palette::cyan()) | bgcolor(palette::bg_highlight());
    case InlineStyle::Url:
        return std::move(element) | color(palette::cyan()) | underlined;
    case InlineStyle::Heading:
        return std::move(element) | bold | underlined | color(palette::blue());
    case InlineStyle::Quote:
        return std::move(element) | dim | color(palette::comment());
    case InlineStyle::Bullet:
        return std::move(element) | color(palette::yellow());
    case InlineStyle::Normal:
    default:
        return std::move(element) | color(palette::fg());
    }
}

int visual_width(const std::string& text) {
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

void append_segment_tokens(std::vector<InlineToken>& tokens,
                           const std::string& text,
                           InlineStyle style) {
    std::string part;
    for (char c : text) {
        if (c == ' ') {
            if (!part.empty()) {
                tokens.push_back({part, style});
                part.clear();
            }
            tokens.push_back({" ", style});
        } else {
            part.push_back(c);
        }
    }
    if (!part.empty()) {
        tokens.push_back({part, style});
    }
}

std::vector<InlineToken> parse_inline_tokens(const std::string& line, InlineStyle base_style = InlineStyle::Normal) {
    std::vector<InlineToken> tokens;
    size_t i = 0;
    std::string buf;

    auto flush_buf = [&]() {
        if (!buf.empty()) {
            append_segment_tokens(tokens, buf, base_style);
            buf.clear();
        }
    };

    while (i < line.size()) {
        if (i + 7 < line.size() &&
            (line.substr(i, 8) == "https://" || line.substr(i, 7) == "http://")) {
            flush_buf();
            size_t end = i;
            while (end < line.size() && line[end] != ' ' && line[end] != '\t') ++end;
            std::string url = line.substr(i, end - i);
            while (!url.empty() && (url.back() == '.' || url.back() == ',' ||
                                    url.back() == ')' || url.back() == ']')) {
                url.pop_back();
                --end;
            }
            append_segment_tokens(tokens, url, InlineStyle::Url);
            i = end;
            continue;
        }
        if (i + 1 < line.size() && line[i] == '*' && line[i + 1] == '*') {
            flush_buf();
            size_t end = line.find("**", i + 2);
            if (end != std::string::npos) {
                append_segment_tokens(tokens, line.substr(i + 2, end - i - 2), InlineStyle::Bold);
                i = end + 2;
                continue;
            }
        }
        if (line[i] == '*' && (i + 1 >= line.size() || line[i + 1] != '*')) {
            flush_buf();
            size_t end = line.find('*', i + 1);
            if (end != std::string::npos) {
                append_segment_tokens(tokens, line.substr(i + 1, end - i - 1), InlineStyle::Italic);
                i = end + 1;
                continue;
            }
        }
        if (line[i] == '`') {
            flush_buf();
            size_t end = line.find('`', i + 1);
            if (end != std::string::npos) {
                append_segment_tokens(tokens, line.substr(i + 1, end - i - 1), InlineStyle::Code);
                i = end + 1;
                continue;
            }
        }
        buf += line[i];
        ++i;
    }
    flush_buf();
    return tokens;
}

Element line_from_tokens(const std::vector<InlineToken>& tokens) {
    Elements parts;
    for (const auto& token : tokens) {
        parts.push_back(apply_style(text(token.text), token.style));
    }
    if (parts.empty()) {
        return text("");
    }
    return hbox(std::move(parts));
}

std::vector<Element> wrap_tokens(const std::vector<InlineToken>& tokens, int width) {
    std::vector<Element> lines;
    std::vector<InlineToken> current;
    int current_width = 0;
    width = std::max(1, width);

    auto flush_line = [&]() {
        lines.push_back(line_from_tokens(current));
        current.clear();
        current_width = 0;
    };

    for (const auto& token : tokens) {
        const int token_width = visual_width(token.text);
        const bool is_space = token.text == " ";

        if (is_space && current.empty()) {
            continue;
        }

        if (!current.empty() && current_width + token_width > width && !is_space) {
            flush_line();
        }

        if (is_space && current.empty()) {
            continue;
        }

        current.push_back(token);
        current_width += token_width;
    }

    if (!current.empty() || lines.empty()) {
        flush_line();
    }

    return lines;
}

std::vector<Element> prefixed_lines(std::vector<Element> content_lines,
                                    const std::string& prefix,
                                    InlineStyle prefix_style = InlineStyle::Normal) {
    std::vector<Element> out;
    if (content_lines.empty()) {
        out.push_back(hbox({apply_style(text(prefix), prefix_style), text("")}));
        return out;
    }

    for (auto& line : content_lines) {
        out.push_back(hbox({
            apply_style(text(prefix), prefix_style),
            std::move(line),
        }));
    }
    return out;
}

std::vector<Element> wrap_prefixed_text(const std::string& text,
                                        int width,
                                        const std::string& prefix,
                                        InlineStyle content_style,
                                        InlineStyle prefix_style = InlineStyle::Normal) {
    const int content_width = std::max(1, width - visual_width(prefix));
    auto lines = wrap_tokens(parse_inline_tokens(text, content_style), content_width);
    return prefixed_lines(std::move(lines), prefix, prefix_style);
}

} // anonymous namespace

std::vector<Element> render_markdown_lines(const std::string& md, int width) {
    std::istringstream stream(md);
    std::string line;
    std::vector<Element> blocks;
    bool in_code_block = false;

    while (std::getline(stream, line)) {
        if (line.size() >= 3 && line.substr(0, 3) == "```") {
            in_code_block = !in_code_block;
            continue;
        }

        if (in_code_block) {
            auto code_lines = wrap_prefixed_text(line, width, "", InlineStyle::Code);
            blocks.insert(blocks.end(),
                          std::make_move_iterator(code_lines.begin()),
                          std::make_move_iterator(code_lines.end()));
            continue;
        }

        if (!line.empty() && line[0] == '#') {
            size_t level = 0;
            while (level < line.size() && line[level] == '#') ++level;
            std::string heading = line.substr(level);
            if (!heading.empty() && heading[0] == ' ') heading = heading.substr(1);
            auto heading_lines = wrap_prefixed_text(heading, width, "", InlineStyle::Heading);
            blocks.insert(blocks.end(),
                          std::make_move_iterator(heading_lines.begin()),
                          std::make_move_iterator(heading_lines.end()));
            continue;
        }

        if (!line.empty() && line[0] == '>') {
            std::string content = line.substr(1);
            if (!content.empty() && content[0] == ' ') content = content.substr(1);
            auto quote_lines = wrap_prefixed_text(content, width, "│ ", InlineStyle::Quote, InlineStyle::Quote);
            blocks.insert(blocks.end(),
                          std::make_move_iterator(quote_lines.begin()),
                          std::make_move_iterator(quote_lines.end()));
            continue;
        }

        if (line.size() >= 2 &&
            (line[0] == '-' || line[0] == '*') && line[1] == ' ') {
            std::string content = line.substr(2);
            auto bullet_lines = wrap_prefixed_text(content, width, "  • ", InlineStyle::Normal, InlineStyle::Bullet);
            blocks.insert(blocks.end(),
                          std::make_move_iterator(bullet_lines.begin()),
                          std::make_move_iterator(bullet_lines.end()));
            continue;
        }

        if (line.empty()) {
            blocks.push_back(text(""));
        } else {
            auto normal_lines = wrap_prefixed_text(line, width, "", InlineStyle::Normal);
            blocks.insert(blocks.end(),
                          std::make_move_iterator(normal_lines.begin()),
                          std::make_move_iterator(normal_lines.end()));
        }
    }

    if (blocks.empty()) {
        blocks.push_back(text(""));
    }
    return blocks;
}

Element render_markdown(const std::string& md) {
    return vbox(render_markdown_lines(md, 80));
}

} // namespace grotto::ui
