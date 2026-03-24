#pragma once
#include <ftxui/dom/elements.hpp>
#include <string>

namespace grotto::ui {

// Parse markdown text and return an FTXUI Element tree.
// Supports: **bold**, *italic*, `inline code`, ```code blocks```,
// # headings, - bullet lists, > blockquotes
ftxui::Element render_markdown(const std::string& text);

} // namespace grotto::ui
