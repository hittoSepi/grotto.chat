#pragma once

#include <algorithm>
#include <cstdint>

namespace grotto::voice {

struct VoiceActivityDecision {
    bool gate_open = false;
    bool signal_detected = false;
};

class VoiceActivityGate {
public:
    static constexpr float   kCloseThresholdFactor = 0.70f;
    static constexpr int64_t kHangoverMs = 180;

    void reset() {
        open_ = false;
        last_signal_ms_ = 0;
    }

    [[nodiscard]] VoiceActivityDecision update(float rms,
                                               float threshold,
                                               int64_t now_ms) {
        const float open_threshold = std::max(threshold, 0.0f);
        const float close_threshold = open_threshold * kCloseThresholdFactor;
        const float active_threshold = open_ ? close_threshold : open_threshold;
        const bool signal_detected = rms >= active_threshold;

        if (signal_detected) {
            open_ = true;
            last_signal_ms_ = now_ms;
            return {.gate_open = true, .signal_detected = true};
        }

        if (open_ && last_signal_ms_ > 0 &&
            (now_ms - last_signal_ms_) < kHangoverMs) {
            return {.gate_open = true, .signal_detected = false};
        }

        open_ = false;
        return {.gate_open = false, .signal_detected = false};
    }

    [[nodiscard]] bool is_open() const {
        return open_;
    }

private:
    bool    open_ = false;
    int64_t last_signal_ms_ = 0;
};

} // namespace grotto::voice
