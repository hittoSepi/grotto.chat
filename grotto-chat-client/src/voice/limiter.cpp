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

    const float target_gain = (peak > threshold_) ? (threshold_ / peak) : 1.0f;
    const float attack = 0.35f;
    const float release = 0.08f;
    const float smoothing = (target_gain < current_gain_) ? attack : release;
    current_gain_ += (target_gain - current_gain_) * smoothing;

    for (float& sample : frame) {
        const float limited = sample * current_gain_;
        const float normalized = limited / std::max(threshold_, 0.0001f);
        sample = std::tanh(normalized) * threshold_;
    }
}

} // namespace grotto::voice
