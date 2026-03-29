#include "ui/graphics_compositor.hpp"

#include "ui/terminal_image.hpp"

namespace grotto::ui {

namespace {

bool has_native_commands(const GraphicsFrame& frame) {
    for (const auto& command : frame.commands) {
        if (command.backend != GraphicsBackendKind::ColorBlocks) {
            return true;
        }
    }
    return false;
}

} // namespace

bool GraphicsCompositor::needs_full_clear(const GraphicsFrame& frame) const {
    return frame.viewport_x != last_frame_.viewport_x ||
           frame.viewport_y != last_frame_.viewport_y ||
           frame.viewport_width != last_frame_.viewport_width ||
           frame.viewport_height != last_frame_.viewport_height;
}

void GraphicsCompositor::commit(GraphicsFrame frame) {
    const bool native_enabled = terminal_inline_native_graphics_enabled();
    const bool has_native = has_native_commands(frame);

    if (needs_full_clear(frame)) {
        requires_full_clear_ = true;
    }

    if (!native_enabled) {
        last_frame_ = std::move(frame);
        had_native_graphics_ = has_native;
        requires_full_clear_ = false;
        return;
    }

    if (!has_native && !had_native_graphics_) {
        last_frame_ = std::move(frame);
        requires_full_clear_ = false;
        return;
    }

    if (!has_native && had_native_graphics_) {
        clear_inline_graphics_layer();
        had_native_graphics_ = false;
        last_frame_ = std::move(frame);
        requires_full_clear_ = false;
        return;
    }

    if (requires_full_clear_) {
        clear_inline_graphics_layer();
    }

    // Native inline commit stays disabled for now until a proven safe
    // main-thread draw path exists for the active terminal backend.
    last_frame_ = std::move(frame);
    had_native_graphics_ = has_native;
    requires_full_clear_ = false;
}

void GraphicsCompositor::invalidate_all() {
    requires_full_clear_ = true;
}

} // namespace grotto::ui
