#pragma once
#include "state/channel_state.hpp"
#include <ftxui/dom/elements.hpp>
#include <string>

namespace grotto::ui {

// Renders the message history as an FTXUI Element.
ftxui::Element render_messages(const ChannelState& state,
                                const std::string& timestamp_format,
                                int visible_rows,
                                int width);

} // namespace grotto::ui
