#include "ui/graphics_compositor.hpp"

#include "ui/terminal_image.hpp"

namespace grotto::ui {

namespace {

GraphicsBackendKind backend_for_protocol(TerminalInlineProtocol protocol) {
    if (protocol == TerminalInlineProtocol::Kitty) {
        return GraphicsBackendKind::Kitty;
    }
    if (protocol == TerminalInlineProtocol::Sixel) {
        return GraphicsBackendKind::Sixel;
    }
    return GraphicsBackendKind::ColorBlocks;
}

bool has_backend_commands(const GraphicsFrame& frame, GraphicsBackendKind backend) {
    for (const auto& command : frame.commands) {
        if (command.backend == backend) {
            return true;
        }
    }
    return false;
}

std::vector<GraphicsDrawCommand> filter_commands(const GraphicsFrame& frame,
                                                 GraphicsBackendKind backend) {
    std::vector<GraphicsDrawCommand> filtered;
    for (const auto& command : frame.commands) {
        if (command.backend == backend) {
            filtered.push_back(command);
        }
    }
    return filtered;
}

GraphicsBackendKind first_native_backend(const GraphicsFrame& frame) {
    for (const auto& command : frame.commands) {
        if (command.backend != GraphicsBackendKind::ColorBlocks) {
            return command.backend;
        }
    }
    return GraphicsBackendKind::ColorBlocks;
}

bool same_backend_commands(const GraphicsFrame& a,
                           const GraphicsFrame& b,
                           GraphicsBackendKind backend) {
    if (a.viewport_x != b.viewport_x ||
        a.viewport_y != b.viewport_y ||
        a.viewport_width != b.viewport_width ||
        a.viewport_height != b.viewport_height) {
        return false;
    }

    size_t ia = 0;
    size_t ib = 0;
    while (true) {
        while (ia < a.commands.size() &&
               a.commands[ia].backend != backend) {
            ++ia;
        }
        while (ib < b.commands.size() &&
               b.commands[ib].backend != backend) {
            ++ib;
        }

        const bool done_a = ia >= a.commands.size();
        const bool done_b = ib >= b.commands.size();
        if (done_a || done_b) {
            return done_a == done_b;
        }

        const auto& ca = a.commands[ia];
        const auto& cb = b.commands[ib];
        if (ca.message_index != cb.message_index ||
            ca.block_index != cb.block_index ||
            ca.viewport_x != cb.viewport_x ||
            ca.viewport_y != cb.viewport_y ||
            ca.width != cb.width ||
            ca.height != cb.height ||
            ca.z_index != cb.z_index) {
            return false;
        }

        ++ia;
        ++ib;
    }
}

} // namespace

bool GraphicsCompositor::needs_full_clear(const GraphicsFrame& frame) const {
    return frame.viewport_x != last_frame_.viewport_x ||
           frame.viewport_y != last_frame_.viewport_y ||
           frame.viewport_width != last_frame_.viewport_width ||
           frame.viewport_height != last_frame_.viewport_height;
}

void GraphicsCompositor::prepare_for_frame(const GraphicsFrame& frame) {
    const bool had_sixel = had_native_graphics_ &&
                           first_native_backend(last_frame_) == GraphicsBackendKind::Sixel;
    if (!had_sixel) {
        return;
    }

    const bool frame_changed = needs_full_clear(frame) ||
                               !same_backend_commands(frame, last_frame_, GraphicsBackendKind::Sixel);
    if (!frame_changed) {
        return;
    }

    clear_inline_graphics_commands(filter_commands(last_frame_, GraphicsBackendKind::Sixel));
}

void GraphicsCompositor::commit(GraphicsFrame frame) {
    const TerminalInlineProtocol protocol = terminal_inline_protocol_for_compositor();
    const GraphicsBackendKind active_backend = backend_for_protocol(protocol);
    const bool native_enabled = active_backend != GraphicsBackendKind::ColorBlocks;
    const bool has_active_commands = has_backend_commands(frame, active_backend);
    const bool backend_changed = had_native_graphics_ &&
                                 first_native_backend(last_frame_) != active_backend;

    if (needs_full_clear(frame)) {
        requires_full_clear_ = true;
    }

    if (!native_enabled) {
        if (had_native_graphics_) {
            clear_inline_graphics_layer();
        }
        last_frame_ = std::move(frame);
        had_native_graphics_ = false;
        requires_full_clear_ = false;
        return;
    }

    if (active_backend == GraphicsBackendKind::Sixel) {
        if (backend_changed) {
            clear_inline_graphics_layer();
        }
        if (has_active_commands) {
            draw_inline_graphics_commands(filter_commands(frame, active_backend));
        }
        last_frame_ = std::move(frame);
        had_native_graphics_ = has_active_commands;
        requires_full_clear_ = false;
        return;
    }

    if (!has_active_commands && !had_native_graphics_) {
        last_frame_ = std::move(frame);
        requires_full_clear_ = false;
        return;
    }

    if (!has_active_commands && had_native_graphics_) {
        clear_inline_graphics_layer();
        had_native_graphics_ = false;
        last_frame_ = std::move(frame);
        requires_full_clear_ = false;
        return;
    }

    if (backend_changed || requires_full_clear_) {
        clear_inline_graphics_layer();
    }

    bool should_draw = requires_full_clear_ || !had_native_graphics_;
    if (!should_draw) {
        should_draw = !same_backend_commands(frame, last_frame_, active_backend);
    }
    if (should_draw) {
        draw_inline_graphics_commands(filter_commands(frame, active_backend));
    }

    last_frame_ = std::move(frame);
    had_native_graphics_ = true;
    requires_full_clear_ = false;
}

void GraphicsCompositor::invalidate_all() {
    requires_full_clear_ = true;
}

} // namespace grotto::ui
