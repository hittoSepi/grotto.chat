#pragma once

#include "ui/color_scheme.hpp"

#include <ftxui/dom/elements.hpp>

#include <string>
#include <vector>
#include <algorithm>

namespace grotto::ui {

inline ftxui::Element build_modal_box(const std::string& title,
                                      ftxui::Elements body_rows,
                                      int width) {
    ftxui::Elements rows;
    rows.reserve(body_rows.size() + 2);
    rows.push_back(ftxui::hbox({
        ftxui::text(title) | ftxui::bold | ftxui::color(palette::blue()),
        ftxui::filler(),
    }) | ftxui::bgcolor(palette::bg()));
    rows.push_back(ftxui::separator());
    for (auto& row : body_rows) {
        rows.push_back(std::move(row) | ftxui::bgcolor(palette::bg()));
    }

    return ftxui::vbox(std::move(rows))
           | ftxui::border
           | ftxui::bgcolor(palette::bg())
           | ftxui::clear_under
           | ftxui::size(ftxui::WIDTH, ftxui::EQUAL, width);
}

inline ftxui::Element overlay_centered_modal(ftxui::Element base,
                                             ftxui::Element modal_box,
                                             ftxui::Color scrim_color = ftxui::Color::RGB(0x00, 0x00, 0x00)) {
    auto scrim = ftxui::text("")
                 | ftxui::size(ftxui::WIDTH, ftxui::EQUAL, 10000)
                 | ftxui::size(ftxui::HEIGHT, ftxui::EQUAL, 10000)
                 | ftxui::bgcolor(scrim_color);

    return ftxui::dbox({
        std::move(base),
        std::move(scrim),
        ftxui::vbox({
            ftxui::filler(),
            ftxui::hbox({
                ftxui::filler(),
                std::move(modal_box),
                ftxui::filler(),
            }),
            ftxui::filler(),
        }),
    });
}

inline ftxui::Element build_pattern_scrim(int term_cols,
                                          int term_rows,
                                          const std::string& glyph = "▓",
                                          ftxui::Color fg = ftxui::Color::RGB(0x08, 0x08, 0x12)) {
    const int cols = std::max(1, term_cols);
    const int rows = std::max(1, term_rows);
    const std::string cell = glyph.empty() ? "▓" : glyph;

    ftxui::Elements scrim_rows;
    scrim_rows.reserve(static_cast<size_t>(rows));
    for (int row = 0; row < rows; ++row) {
        std::string line;
        line.reserve(static_cast<size_t>(cols) * cell.size());
        for (int col = 0; col < cols; ++col) {
            line += cell;
        }
        scrim_rows.push_back(ftxui::text(std::move(line)) | ftxui::color(fg));
    }
    return ftxui::vbox(std::move(scrim_rows)) | ftxui::bgcolor(palette::bg()) | ftxui::clear_under;
}

inline ftxui::Element overlay_centered_modal(ftxui::Element base,
                                             ftxui::Element modal_box,
                                             ftxui::Element scrim) {
    return ftxui::dbox({
        std::move(base),
        std::move(scrim),
        ftxui::vbox({
            ftxui::filler(),
            ftxui::hbox({
                ftxui::filler(),
                std::move(modal_box),
                ftxui::filler(),
            }),
            ftxui::filler(),
        }),
    });
}

} // namespace grotto::ui
