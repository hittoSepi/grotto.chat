#pragma once
#include "state/channel_state.hpp"
#include <ftxui/dom/elements.hpp>
#include <string>

namespace grotto::ui {

struct VisibleMessageLine {
    int message_index = -1;
    std::string plain_text;
};

// Renders the message history as an FTXUI Element.
ftxui::Element render_messages(const ChannelState& state,
                                const std::string& timestamp_format,
                                int visible_rows,
                                int width);

std::vector<VisibleMessageLine> collect_visible_message_lines(
    const ChannelState& state,
    const std::string& timestamp_format,
    int visible_rows,
    int width);

} // namespace grotto::ui
