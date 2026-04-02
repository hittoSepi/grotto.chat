#include "voice/limiter.hpp"

#include <algorithm>
#include <cmath>

namespace grotto::voice {

void Limiter::configure(bool enabled, float threshold) {
    enabled_ = enabled;
    threshold_ = std::clamp(threshold, 0.2f, 0.99f);
    current_gain_ = 1.0f;
}

void Limiter::reset() {
    current_gain_ = 1.0f;
}

void Limiter::process(std::vector<float>& frame) {
    if (!enabled_ || frame.empty()) {
        return;
    }

    float peak = 0.0f;
    for (float sample : frame) {
        peak = std::max(peak, std::abs(sample));
    }
    if (peak <= 0.0f) {
        return;
    }

    const float safety_threshold = threshold_ * 0.98f;
    const float target_gain = (peak > safety_threshold) ? (safety_threshold / peak) : 1.0f;
    const float release = 0.12f;
    const float frame_gain = std::min(current_gain_, target_gain);

    if (target_gain < current_gain_) {
        current_gain_ = target_gain;
    } else {
        current_gain_ += (target_gain - current_gain_) * release;
    }

    for (float& sample : frame) {
        sample = std::clamp(sample * frame_gain, -0.999f, 0.999f);
    }
}

} // namespace grotto::voice
