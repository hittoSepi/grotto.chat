#pragma once

#include <ftxui/component/screen_interactive.hpp>
#include <string>

namespace grotto::ui {

bool terminal_inline_images_supported();
bool display_inline_image_from_url(ftxui::ScreenInteractive& screen,
                                   const std::string& url);

} // namespace grotto::ui
