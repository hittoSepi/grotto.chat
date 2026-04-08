#pragma once
#include <ftxui/screen/color.hpp>
#include <cstdint>
#include <string_view>
namespace grotto::ui {

// Grotto palette
namespace palette {
    inline ftxui::Color bg()           { return ftxui::Color::RGB(0x14,0x16,0x1b); } // #14161b
    inline ftxui::Color bg_dark()      { return ftxui::Color::RGB(0x0b,0x0c,0x0f); } // #0b0c0f
    inline ftxui::Color bg_highlight() { return ftxui::Color::RGB(0x2a,0x2f,0x36); } // #2a2f36
    inline ftxui::Color fg()           { return ftxui::Color::RGB(0xd6,0xcf,0xb8); } // #d6cfb8
    inline ftxui::Color fg_dark()      { return ftxui::Color::RGB(0x9a,0x93,0x80); } // #9a9380
    inline ftxui::Color comment()      { return ftxui::Color::RGB(0x6b,0x70,0x76); } // #6b7076

    inline ftxui::Color blue()         { return ftxui::Color::RGB(0x3a,0x3f,0x46); } // #3a3f46
    inline ftxui::Color blue1()        { return ftxui::Color::RGB(0x4a,0x4f,0x55); } // #4a4f55
    inline ftxui::Color cyan()         { return ftxui::Color::RGB(0x1f,0x7a,0x5a); } // #1f7a5a
    inline ftxui::Color magenta()      { return ftxui::Color::RGB(0x8a,0x5a,0x2b); } // #8a5a2b
    inline ftxui::Color orange()       { return ftxui::Color::RGB(0xc0,0x7a,0x2c); } // #c07a2c
    inline ftxui::Color yellow()       { return ftxui::Color::RGB(0xf0,0xd9,0x4a); } // #f0d94a
    inline ftxui::Color green()        { return ftxui::Color::RGB(0x3c,0xff,0x9b); } // #3cff9b
    inline ftxui::Color red()          { return ftxui::Color::RGB(0xff,0xb3,0x47); } // #ffb347
    inline ftxui::Color purple()       { return ftxui::Color::RGB(0x5a,0x3b,0x1e); } // #5a3b1e
    inline ftxui::Color teal()         { return ftxui::Color::RGB(0x0f,0x3d,0x2e); } // #0f3d2e

    inline ftxui::Color online()       { return green(); }
    inline ftxui::Color away_c()       { return yellow(); }
    inline ftxui::Color dnd_c()        { return orange(); }
    inline ftxui::Color offline_c()    { return comment(); }
    inline ftxui::Color unread_badge() { return yellow(); }
    inline ftxui::Color error_c()      { return orange(); }
} // namespace palette

inline ftxui::Color nick_color(std::string_view nick) {
    static const ftxui::Color pool[] = {
        palette::cyan(), palette::green(), palette::yellow(), palette::orange(),
        palette::magenta(), palette::red(), palette::blue(), palette::blue1(),
        palette::purple(), palette::teal(),
    };
    static constexpr int N = static_cast<int>(std::size(pool));

    uint32_t hash = 2166136261u;
    for (unsigned char c : nick) { hash ^= c; hash *= 16777619u; }
    return pool[hash % N];
}

} // namespace grotto::ui
