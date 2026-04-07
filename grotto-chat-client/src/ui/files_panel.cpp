#include "ui/files_panel.hpp"

#include "ui/color_scheme.hpp"

#include <ftxui/dom/elements.hpp>

#include <algorithm>
#include <iomanip>
#include <sstream>

using namespace ftxui;

namespace grotto::ui {

namespace {

std::string human_bytes(uint64_t bytes) {
    static constexpr const char* kUnits[] = {"B", "KB", "MB", "GB", "TB"};
    double value = static_cast<double>(bytes);
    size_t unit_index = 0;
    while (value >= 1024.0 && unit_index + 1 < std::size(kUnits)) {
        value /= 1024.0;
        ++unit_index;
    }

    std::ostringstream oss;
    if (unit_index == 0) {
        oss << bytes << ' ' << kUnits[unit_index];
    } else if (value >= 10.0) {
        oss << std::fixed << std::setprecision(0) << value << ' ' << kUnits[unit_index];
    } else {
        oss << std::fixed << std::setprecision(1) << value << ' ' << kUnits[unit_index];
    }
    return oss.str();
}

std::string truncate_with_ellipsis(std::string text, int max_width) {
    if (max_width <= 0 || static_cast<int>(text.size()) <= max_width) {
        return text;
    }
    if (max_width <= 3) {
        return text.substr(0, static_cast<size_t>(max_width));
    }
    return text.substr(0, static_cast<size_t>(max_width - 3)) + "...";
}

Element render_file_entry(const RemoteFileEntry& file, int width, bool selected) {
    std::string label = file.filename.empty() ? file.file_id : file.filename;
    std::string meta = human_bytes(file.file_size);
    if (!file.sender_id.empty()) {
        meta += " <" + file.sender_id + ">";
    }

    const std::string marker = selected ? "> " : "  ";
    const int content_width = std::max(8, width - static_cast<int>(marker.size()) - 1);
    label = truncate_with_ellipsis(label, content_width);
    meta = truncate_with_ellipsis(meta, content_width);

    auto body = vbox({
        text(marker + label) | color(selected ? palette::bg() : palette::fg()),
        text(marker + meta) | color(selected ? palette::bg() : palette::comment()),
    });

    if (selected) {
        return body | bgcolor(palette::blue());
    }
    return body;
}

} // namespace

Element render_files_panel(const std::vector<RemoteFileEntry>& files,
                           int width,
                           const std::optional<std::string>& selected_file_id,
                           std::vector<FileHitRegion>& out_file_positions,
                           int base_x,
                           int base_y) {
    out_file_positions.clear();

    Elements content;
    content.push_back(
        hbox({
            text("FILES " + std::to_string(files.size())) | bold | color(palette::fg_dark()),
            filler(),
            text("Enter download") | color(palette::comment()),
        }));
    content.push_back(separator() | color(palette::bg_highlight()));

    int current_y = base_y + 2;
    if (files.empty()) {
        content.push_back(text(" No files yet") | color(palette::comment()));
        content.push_back(text(" Press /files to refresh") | color(palette::comment()));
    } else {
        for (const auto& file : files) {
            const bool selected = selected_file_id && *selected_file_id == file.file_id;
            out_file_positions.push_back({file.file_id, base_x, current_y, width, 2});
            content.push_back(render_file_entry(file, width, selected));
            current_y += 2;
        }
        content.push_back(separator() | color(palette::bg_highlight()));
        content.push_back(text(" Up/Down move, double-click downloads") | color(palette::comment()));
    }

    return vbox(std::move(content)) | bgcolor(palette::bg_dark());
}

} // namespace grotto::ui
