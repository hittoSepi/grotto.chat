#pragma once
#include <ftxui/dom/elements.hpp>
#include <string>
#include <vector>

namespace grotto::ui {

// Parse markdown text and return an FTXUI Element tree.
// Supports: **bold**, *italic*, `inline code`, ```code blocks```,
// # headings, - bullet lists, > blockquotes
ftxui::Element render_markdown(const std::string& text);
std::vector<ftxui::Element> render_markdown_lines(const std::string& text, int width);
std::vector<std::string> render_markdown_plain_lines(const std::string& text, int width);

} // namespace grotto::ui
