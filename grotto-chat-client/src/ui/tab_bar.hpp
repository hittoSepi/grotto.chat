#pragma once
#include <ftxui/dom/elements.hpp>
#include <string>
#include <vector>

namespace grotto::ui {

struct TabHitRegion {
    std::string channel;
    int x = 0;
    int width = 0;
};

// Render tab bar and return positions for mouse hit testing
// Returns vector of exact tab hit regions for each rendered tab.
ftxui::Element render_tab_bar(const std::vector<std::string>& channels,
                               const std::string& active_channel,
                               const std::vector<int>& unread_counts,
                               const std::vector<std::size_t>& voice_counts,
                               std::vector<TabHitRegion>& out_positions);

// Simple version without position tracking (backward compatible)
inline ftxui::Element render_tab_bar(const std::vector<std::string>& channels,
                                      const std::string& active_channel,
                                      const std::vector<int>& unread_counts) {
    std::vector<TabHitRegion> dummy;
    std::vector<std::size_t> no_voice;
    return render_tab_bar(channels, active_channel, unread_counts, no_voice, dummy);
}

} // namespace grotto::ui
