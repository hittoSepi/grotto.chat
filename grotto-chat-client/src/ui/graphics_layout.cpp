#include "ui/graphics_layout.hpp"

#include "ui/color_scheme.hpp"
#include "ui/terminal_image.hpp"

#include <algorithm>

namespace grotto::ui {

namespace {

int visible_width(std::string_view text) {
    int width = 0;
    for (size_t i = 0; i < text.size();) {
        unsigned char c = static_cast<unsigned char>(text[i]);
        if ((c & 0x80) == 0) i += 1;
        else if ((c & 0xE0) == 0xC0) i += 2;
        else if ((c & 0xF0) == 0xE0) i += 3;
        else if ((c & 0xF8) == 0xF0) i += 4;
        else i += 1;
        ++width;
    }
    return width;
}

struct GraphicsMeasure {
    int columns = 1;
    int pixel_rows = 2;
};

class GraphicsBackend {
public:
    virtual ~GraphicsBackend() = default;
    virtual GraphicsBackendKind kind() const = 0;
    virtual bool supports_terminal_protocol() const = 0;
    virtual GraphicsMeasure measure(const InlineImageThumbnail& thumbnail,
                                    const GraphicsConstraints& constraints) const = 0;
};

class ColorBlockBackend : public GraphicsBackend {
public:
    GraphicsBackendKind kind() const override { return GraphicsBackendKind::ColorBlocks; }
    bool supports_terminal_protocol() const override { return false; }

    GraphicsMeasure measure(const InlineImageThumbnail& thumbnail,
                            const GraphicsConstraints& constraints) const override {
        GraphicsMeasure out;
        out.columns = std::clamp(thumbnail.width, 1, std::max(1, constraints.max_columns));

        const float image_aspect = thumbnail.width > 0
            ? static_cast<float>(thumbnail.height) / static_cast<float>(thumbnail.width)
            : 1.0f;
        // Half-block rendering folds 2 bitmap rows into one text row, and terminal
        // cells are taller than they are wide. A factor around 1.0f keeps wide images
        // visually closer to their source proportions than the earlier 0.5f heuristic.
        const int target_pixel_rows = std::clamp(
            static_cast<int>(image_aspect * static_cast<float>(out.columns) * 1.0f),
            2,
            std::max(2, constraints.max_rows * 2));
        out.pixel_rows = std::max(2, target_pixel_rows);
        return out;
    }
};

class SixelBackend final : public ColorBlockBackend {
public:
    GraphicsBackendKind kind() const override { return GraphicsBackendKind::Sixel; }
    bool supports_terminal_protocol() const override { return true; }
};

class KittyBackend final : public ColorBlockBackend {
public:
    GraphicsBackendKind kind() const override { return GraphicsBackendKind::Kitty; }
    bool supports_terminal_protocol() const override { return true; }
};

const GraphicsBackend& active_backend() {
    static const ColorBlockBackend color_backend;
    static const SixelBackend sixel_backend;
    static const KittyBackend kitty_backend;

    if (!terminal_inline_native_graphics_enabled()) {
        return color_backend;
    }

    if (terminal_uses_compact_image_preview()) {
        return sixel_backend;
    }
    if (terminal_inline_images_supported()) {
        return kitty_backend;
    }
    return color_backend;
}

InlineImageThumbnail resample_thumbnail(const InlineImageThumbnail& thumbnail,
                                        int out_cols,
                                        int out_rows) {
    InlineImageThumbnail out;
    if (thumbnail.rgba.empty() || thumbnail.width <= 0 || thumbnail.height <= 0 ||
        out_cols <= 0 || out_rows <= 0) {
        return out;
    }

    out.width = out_cols;
    out.height = out_rows;
    out.rgba.resize(static_cast<size_t>(out_cols * out_rows * 4), 0);

    for (int y = 0; y < out_rows; ++y) {
        const int src_y = std::min(thumbnail.height - 1, y * thumbnail.height / out_rows);
        for (int x = 0; x < out_cols; ++x) {
            const int src_x = std::min(thumbnail.width - 1, x * thumbnail.width / out_cols);
            const size_t src_idx = static_cast<size_t>((src_y * thumbnail.width + src_x) * 4);
            const size_t dst_idx = static_cast<size_t>((y * out_cols + x) * 4);
            for (size_t c = 0; c < 4; ++c) {
                out.rgba[dst_idx + c] = thumbnail.rgba[src_idx + c];
            }
        }
    }

    return out;
}

} // namespace

GraphicsBackendKind active_graphics_backend_kind() {
    return active_backend().kind();
}

std::vector<LayoutRow> render_graphics_rows(const InlineImageThumbnail& thumbnail,
                                            int message_index,
                                            int block_index,
                                            const std::string& ts_prefix,
                                            int width,
                                            const GraphicsConstraints& constraints) {
    std::vector<LayoutRow> rows;
    if (thumbnail.rgba.empty() || thumbnail.width <= 0 || thumbnail.height <= 0) {
        return rows;
    }

    const int content_width = std::max(1, width - visible_width(ts_prefix));
    const GraphicsMeasure measure = active_backend().measure(thumbnail, {
        std::min(content_width, constraints.max_columns),
        constraints.max_rows,
    });

    const InlineImageThumbnail scaled = resample_thumbnail(
        thumbnail,
        std::max(1, measure.columns),
        std::max(2, measure.pixel_rows));
    if (scaled.rgba.empty()) {
        return rows;
    }

    const GraphicsBackendKind backend_kind = active_backend().kind();
    const bool native_backend = backend_kind != GraphicsBackendKind::ColorBlocks;
    const int text_offset_x = visible_width(ts_prefix);
    const auto source_ptr = native_backend
        ? std::make_shared<InlineImageThumbnail>(thumbnail)
        : nullptr;

    for (int y = 0; y < scaled.height; y += 2) {
        const std::string current_ts = (y == 0) ? ts_prefix : std::string(ts_prefix.size(), ' ');
        ftxui::Element row_element;
        if (native_backend) {
            row_element = ftxui::hbox({
                ftxui::text(current_ts) | ftxui::color(palette::comment()),
                ftxui::text(std::string(static_cast<size_t>(scaled.width), ' ')),
            });
        } else {
            ftxui::Elements cells;
            cells.push_back(ftxui::text(current_ts) | ftxui::color(palette::comment()));
            for (int x = 0; x < scaled.width; ++x) {
                const size_t top = static_cast<size_t>((y * scaled.width + x) * 4);
                const size_t bottom_row = static_cast<size_t>(std::min(y + 1, scaled.height - 1));
                const size_t bottom = static_cast<size_t>((bottom_row * scaled.width + x) * 4);
                ftxui::Color fg = ftxui::Color::RGB(
                    scaled.rgba[top + 0], scaled.rgba[top + 1], scaled.rgba[top + 2]);
                ftxui::Color bg = ftxui::Color::RGB(
                    scaled.rgba[bottom + 0], scaled.rgba[bottom + 1], scaled.rgba[bottom + 2]);
                cells.push_back(ftxui::text("▀") | ftxui::color(fg) | ftxui::bgcolor(bg));
            }
            row_element = ftxui::hbox(std::move(cells));
        }

        rows.push_back({
            message_index,
            block_index,
            LayoutBlockKind::Image,
            current_ts + std::string(static_cast<size_t>(scaled.width), '#'),
            std::nullopt,
            true,
            std::move(row_element),
            native_backend && y == 0,
            backend_kind,
            source_ptr,
            scaled.width,
            (scaled.height + 1) / 2,
            text_offset_x,
        });
    }

    return rows;
}

} // namespace grotto::ui
