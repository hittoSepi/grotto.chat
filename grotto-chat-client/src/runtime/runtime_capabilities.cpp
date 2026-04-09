#include "runtime/runtime_capabilities.hpp"

#include "ui/mouse_support.hpp"
#include "voice/audio_device.hpp"

namespace grotto {

RuntimeCapabilities detect_runtime_capabilities(const ClientConfig& cfg) {
    RuntimeCapabilities capabilities;

    capabilities.configured_terminal_graphics_mode =
        ui::parse_terminal_graphics_mode(cfg.preview.terminal_graphics);

    ui::configure_terminal_graphics(capabilities.configured_terminal_graphics_mode);
    capabilities.terminal_protocol_detected = ui::terminal_inline_protocol();
    capabilities.compositor_protocol = ui::terminal_inline_protocol_for_compositor();
    capabilities.inline_native_enabled = ui::terminal_inline_native_graphics_enabled();

    ui::initialize_clipboard_backend();
    capabilities.clipboard_backend = ui::clipboard_backend_name();
    capabilities.clipboard_available = capabilities.clipboard_backend != "unknown";

    // Do not enumerate audio devices during process startup. On some Windows
    // machines the host audio stack or drivers can fault inside device
    // enumeration before the UI even appears. Voice settings and actual voice
    // startup still probe devices later through the normal path.
    capabilities.audio_input_device_count = 0;
    capabilities.audio_output_device_count = 0;
    capabilities.audio_capture_available = false;
    capabilities.audio_playback_available = false;

    return capabilities;
}

std::string terminal_graphics_mode_name(ui::TerminalGraphicsMode mode) {
    switch (mode) {
        case ui::TerminalGraphicsMode::Auto:
            return "auto";
        case ui::TerminalGraphicsMode::Off:
            return "off";
        case ui::TerminalGraphicsMode::ViewerOnly:
            return "viewer-only";
    }
    return "auto";
}

std::string terminal_inline_protocol_name(ui::TerminalInlineProtocol protocol) {
    switch (protocol) {
        case ui::TerminalInlineProtocol::Kitty:
            return "kitty";
        case ui::TerminalInlineProtocol::Sixel:
            return "sixel";
        case ui::TerminalInlineProtocol::ITerm2:
            return "iterm2";
        case ui::TerminalInlineProtocol::None:
        default:
            return "none";
    }
}

}  // namespace grotto
