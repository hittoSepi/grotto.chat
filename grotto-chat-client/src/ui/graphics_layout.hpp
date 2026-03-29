#pragma once

#include "state/channel_state.hpp"

#include <ftxui/dom/elements.hpp>
#include <optional>
#include <memory>
#include <string>
#include <vector>

namespace grotto::ui {

enum class LayoutBlockKind {
    Text,
    Image,
    Spacer,
};

enum class GraphicsBackendKind {
    ColorBlocks,
    Sixel,
    Kitty,
};

struct GraphicsConstraints {
    int max_columns = 1;
    int max_rows = 1;
};

struct VisibleLayoutHit {
    int message_index = -1;
    int block_index = -1;
    LayoutBlockKind block_kind = LayoutBlockKind::Text;
    std::string plain_text;
    std::optional<std::string> url;
    bool has_inline_image = false;
};

struct LayoutRow {
    int message_index = -1;
    int block_index = -1;
    LayoutBlockKind block_kind = LayoutBlockKind::Text;
    std::string plain_text;
    std::optional<std::string> url;
    bool has_inline_image = false;
    ftxui::Element element;
    bool starts_graphics_block = false;
    GraphicsBackendKind graphics_backend = GraphicsBackendKind::ColorBlocks;
    std::shared_ptr<InlineImageThumbnail> graphics_image;
    int graphics_columns = 0;
    int graphics_rows = 0;
    int graphics_offset_x = 0;
};

struct GraphicsDrawCommand {
    GraphicsBackendKind backend = GraphicsBackendKind::ColorBlocks;
    int message_index = -1;
    int block_index = -1;
    int viewport_x = 0;
    int viewport_y = 0;
    int width = 0;
    int height = 0;
    std::shared_ptr<InlineImageThumbnail> image;
};

GraphicsBackendKind active_graphics_backend_kind();
std::vector<LayoutRow> render_graphics_rows(const InlineImageThumbnail& thumbnail,
                                            int message_index,
                                            int block_index,
                                            const std::string& ts_prefix,
                                            int width,
                                            const GraphicsConstraints& constraints);

} // namespace grotto::ui
