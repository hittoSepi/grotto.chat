#pragma once

#include <ftxui/dom/elements.hpp>

#include <string>
#include <vector>

namespace grotto::ui::settings_layout {

ftxui::Element gap(int height = 1);
ftxui::Element hint(const std::string& text);
ftxui::Element page(const std::string& title,
                    std::vector<ftxui::Element> sections,
                    const std::string& subtitle = {});
ftxui::Element section(const std::string& title,
                       std::vector<ftxui::Element> rows,
                       const std::string& hint_text = {});
ftxui::Element row(const std::string& label, ftxui::Element control, bool active = true);
ftxui::Element row(const std::string& label,
                   ftxui::Element control,
                   ftxui::Element value,
                   bool active = true);
ftxui::Element toggle_row(ftxui::Element toggle,
                          const std::string& hint_text = {},
                          bool active = true);
ftxui::Element labeled_block(const std::string& label,
                             ftxui::Element content,
                             bool active = true);
ftxui::Element action_row(ftxui::Element content, bool active = true);

} // namespace grotto::ui::settings_layout
