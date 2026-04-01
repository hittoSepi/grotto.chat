#pragma once

#include <ftxui/component/event.hpp>

#include <cctype>
#include <optional>
#include <string>

namespace grotto::ui {

inline std::optional<std::string> key_name_from_event(const ftxui::Event& event) {
    using ftxui::Event;

    if (event == Event::F1) return "F1";
    if (event == Event::F2) return "F2";
    if (event == Event::F3) return "F3";
    if (event == Event::F4) return "F4";
    if (event == Event::F5) return "F5";
    if (event == Event::F6) return "F6";
    if (event == Event::F7) return "F7";
    if (event == Event::F8) return "F8";
    if (event == Event::F9) return "F9";
    if (event == Event::F10) return "F10";
    if (event == Event::F11) return "F11";
    if (event == Event::F12) return "F12";
    if (event == Event::Tab) return "Tab";
    if (event == Event::Return) return "Enter";
    if (event == Event::Backspace) return "Backspace";
    if (event == Event::Delete) return "Delete";
    if (event == Event::Insert) return "Insert";
    if (event == Event::Home) return "Home";
    if (event == Event::End) return "End";
    if (event == Event::PageUp) return "PageUp";
    if (event == Event::PageDown) return "PageDown";
    if (event == Event::ArrowUp) return "ArrowUp";
    if (event == Event::ArrowDown) return "ArrowDown";
    if (event == Event::ArrowLeft) return "ArrowLeft";
    if (event == Event::ArrowRight) return "ArrowRight";

    if (event == Event::CtrlA) return "Ctrl+A";
    if (event == Event::CtrlB) return "Ctrl+B";
    if (event == Event::CtrlC) return "Ctrl+C";
    if (event == Event::CtrlD) return "Ctrl+D";
    if (event == Event::CtrlE) return "Ctrl+E";
    if (event == Event::CtrlF) return "Ctrl+F";
    if (event == Event::CtrlG) return "Ctrl+G";
    if (event == Event::CtrlH) return "Ctrl+H";
    if (event == Event::CtrlI) return "Ctrl+I";
    if (event == Event::CtrlJ) return "Ctrl+J";
    if (event == Event::CtrlK) return "Ctrl+K";
    if (event == Event::CtrlL) return "Ctrl+L";
    if (event == Event::CtrlM) return "Ctrl+M";
    if (event == Event::CtrlN) return "Ctrl+N";
    if (event == Event::CtrlO) return "Ctrl+O";
    if (event == Event::CtrlP) return "Ctrl+P";
    if (event == Event::CtrlQ) return "Ctrl+Q";
    if (event == Event::CtrlR) return "Ctrl+R";
    if (event == Event::CtrlS) return "Ctrl+S";
    if (event == Event::CtrlT) return "Ctrl+T";
    if (event == Event::CtrlU) return "Ctrl+U";
    if (event == Event::CtrlV) return "Ctrl+V";
    if (event == Event::CtrlW) return "Ctrl+W";
    if (event == Event::CtrlX) return "Ctrl+X";
    if (event == Event::CtrlY) return "Ctrl+Y";
    if (event == Event::CtrlZ) return "Ctrl+Z";

    if (event.is_character()) {
        std::string value = event.character();
        if (value == " ") return "Space";
        if (value.size() == 1) {
            const unsigned char c = static_cast<unsigned char>(value[0]);
            if (std::isalpha(c)) {
                return std::string(1, static_cast<char>(std::toupper(c)));
            }
        }
        return value;
    }

    return std::nullopt;
}

} // namespace grotto::ui
