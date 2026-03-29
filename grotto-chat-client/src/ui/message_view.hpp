#pragma once

#include "state/channel_state.hpp"
#include "ui/graphics_layout.hpp"

#include <ftxui/dom/elements.hpp>
#include <string>
#include <vector>

namespace grotto::ui {

ftxui::Element render_messages(const ChannelState& state,
                               const std::string& timestamp_format,
                               int visible_rows,
                               int width);

std::vector<GraphicsDrawCommand> collect_visible_draw_commands(
    const ChannelState& state,
    const std::string& timestamp_format,
    int visible_rows,
    int width,
    int viewport_x,
    int viewport_y);

std::vector<VisibleLayoutHit> collect_visible_layout_hits(
    const ChannelState& state,
    const std::string& timestamp_format,
    int visible_rows,
    int width);

} // namespace grotto::ui
