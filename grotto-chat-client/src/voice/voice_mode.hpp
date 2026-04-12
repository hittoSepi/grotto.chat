#pragma once

#include <algorithm>
#include <cctype>
#include <string>
#include <string_view>

namespace grotto::voice {

inline std::string normalize_voice_mode(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });

    if (value == "vox") {
        return "vox";
    }
    if (value == "hold" || value == "push-to-talk" || value == "push_to_talk" ||
        value == "push to talk") {
        return "hold";
    }
    if (value == "toggle" || value == "toggle-to-talk" || value == "toggle_to_talk" ||
        value == "toggle to talk" || value == "ttt" || value == "ptt") {
        return "toggle";
    }
    return "toggle";
}

inline bool is_vox_mode(std::string_view mode) {
    return normalize_voice_mode(std::string(mode)) == "vox";
}

inline bool is_hold_mode(std::string_view mode) {
    return normalize_voice_mode(std::string(mode)) == "hold";
}

inline bool is_toggle_mode(std::string_view mode) {
    return normalize_voice_mode(std::string(mode)) == "toggle";
}

inline std::string next_voice_mode(std::string_view mode) {
    const auto normalized = normalize_voice_mode(std::string(mode));
    if (normalized == "toggle") {
        return "hold";
    }
    if (normalized == "hold") {
        return "vox";
    }
    return "toggle";
}

} // namespace grotto::voice
