#include "ui/settings_layout.hpp"

#include "ui/color_scheme.hpp"

#include <algorithm>
#include <utility>

using namespace ftxui;

namespace grotto::ui::settings_layout {

namespace {

constexpr int kLabelWidth = 24;
constexpr int kPageInset = 1;

Element label_cell(const std::string& label, bool active) {
    return paragraphAlignLeft(label) |
           color(active ? palette::fg_dark() : palette::comment()) |
           size(WIDTH, EQUAL, kLabelWidth) |
           vcenter;
}

Element inset(Element child) {
    return hbox({
        text(std::string(kPageInset, ' ')),
        std::move(child) | flex,
        text(std::string(kPageInset, ' ')),
    });
}

Element separator_inset() {
    return inset(separator() | color(palette::blue()));
}

Element apply_active_state(Element element, bool active) {
    if (active) {
        return element;
    }
    return std::move(element) | dim;
}

} // namespace

Element gap(int height) {
    return text("") | size(HEIGHT, EQUAL, std::max(height, 0));
}

Element hint(const std::string& text_value) {
    if (text_value.empty()) {
        return text("");
    }
    return inset(text(text_value) | color(palette::comment()) | dim);
}

Element row(const std::string& label, Element control, bool active) {
    auto row_element = hbox({
        label_cell(label, active),
        std::move(control) | flex,
    });
    return inset(apply_active_state(std::move(row_element), active));
}

Element row(const std::string& label, Element control, Element value, bool active) {
    auto row_element = hbox({
        label_cell(label, active),
        std::move(control) | flex,
        text(" "),
        std::move(value) | vcenter,
    });
    return inset(apply_active_state(std::move(row_element), active));
}

Element toggle_row(Element toggle, const std::string& hint_text, bool active) {
    Elements rows;
    rows.push_back(inset(apply_active_state(std::move(toggle), active)));
    if (!hint_text.empty()) {
        rows.push_back(gap());
        rows.push_back(hint(hint_text));
    }
    return vbox(std::move(rows));
}

Element labeled_block(const std::string& label, Element content, bool active) {
    Elements rows;
    rows.push_back(inset(text(label) | color(active ? palette::fg_dark() : palette::comment())));
    rows.push_back(gap());
    rows.push_back(inset(apply_active_state(std::move(content), active)));
    return vbox(std::move(rows));
}

Element action_row(Element content, bool active) {
    return inset(apply_active_state(std::move(content), active));
}

Element section(const std::string& title, std::vector<Element> rows, const std::string& hint_text) {
    Elements children;
    children.push_back(inset(text(title) | bold | color(palette::blue())));
    children.push_back(separator_inset());
    children.push_back(gap());

    for (size_t i = 0; i < rows.size(); ++i) {
        children.push_back(std::move(rows[i]));
        if (i + 1 < rows.size()) {
            children.push_back(gap());
        }
    }

    if (!hint_text.empty()) {
        children.push_back(gap());
        children.push_back(hint(hint_text));
    }

    return vbox(std::move(children));
}

Element page(const std::string& title, std::vector<Element> sections, const std::string& subtitle) {
    Elements children;
    children.push_back(gap());
    children.push_back(inset(text(title) | bold | color(palette::blue())));
    if (!subtitle.empty()) {
        children.push_back(gap());
        children.push_back(hint(subtitle));
    }
    children.push_back(gap(2));

    for (size_t i = 0; i < sections.size(); ++i) {
        children.push_back(std::move(sections[i]));
        if (i + 1 < sections.size()) {
            children.push_back(gap(2));
        }
    }

    children.push_back(gap());
    return vbox(std::move(children)) | flex;
}

} // namespace grotto::ui::settings_layout
