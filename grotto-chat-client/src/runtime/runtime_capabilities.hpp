#pragma once

#include "config.hpp"
#include "ui/terminal_image.hpp"

#include <cstddef>
#include <string>

namespace grotto {

struct RuntimeCapabilities {
    ui::TerminalGraphicsMode configured_terminal_graphics_mode =
        ui::TerminalGraphicsMode::Auto;
    ui::TerminalInlineProtocol terminal_protocol_detected =
        ui::TerminalInlineProtocol::None;
    ui::TerminalInlineProtocol compositor_protocol =
        ui::TerminalInlineProtocol::None;
    bool inline_native_enabled = false;

    std::string clipboard_backend = "unknown";
    bool clipboard_available = false;

    std::size_t audio_input_device_count = 0;
    std::size_t audio_output_device_count = 0;
    bool audio_capture_available = false;
    bool audio_playback_available = false;
};

RuntimeCapabilities detect_runtime_capabilities(const ClientConfig& cfg);

std::string terminal_graphics_mode_name(ui::TerminalGraphicsMode mode);
std::string terminal_inline_protocol_name(ui::TerminalInlineProtocol protocol);

}  // namespace grotto
