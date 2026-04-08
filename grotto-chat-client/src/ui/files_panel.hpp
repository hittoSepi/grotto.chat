#pragma once

#include "state/app_state.hpp"

#include <ftxui/dom/elements.hpp>

#include <optional>
#include <string>
#include <vector>

namespace grotto::ui {

struct FileHitRegion {
    std::string file_id;
    int x = 0;
    int y = 0;
    int width = 0;
    int height = 1;
};

ftxui::Element render_files_panel(
    const std::vector<RemoteFileEntry>& files,
    int width,
    const std::optional<std::string>& selected_file_id,
    const std::string& quota_summary,
    std::vector<FileHitRegion>& out_file_positions,
    int base_x = 0,
    int base_y = 1);

} // namespace grotto::ui
