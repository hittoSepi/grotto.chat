#pragma once

#include "state/channel_state.hpp"
#include <ftxui/component/screen_interactive.hpp>
#include <string>
#include <string_view>

namespace grotto::ui {

enum class TerminalGraphicsMode {
    Auto,
    Off,
    ViewerOnly,
};

TerminalGraphicsMode parse_terminal_graphics_mode(std::string_view value);
void configure_terminal_graphics(TerminalGraphicsMode mode);
bool terminal_inline_images_supported();
bool terminal_uses_compact_image_preview();
bool display_inline_image_from_url(ftxui::ScreenInteractive& screen,
                                   const std::string& url);
bool display_inline_image(ftxui::ScreenInteractive& screen,
                          const InlineImageThumbnail& thumbnail,
                          const std::string& title = {});

} // namespace grotto::ui
