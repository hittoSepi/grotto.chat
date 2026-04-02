#include "voice/limiter.hpp"

#include <algorithm>
#include <cmath>

namespace grotto::voice {

namespace {

float soft_clip(float sample, float threshold) {
    const float abs_sample = std::abs(sample);
    if (abs_sample <= threshold) {
        return sample;
    }

    const float sign = (sample < 0.0f) ? -1.0f : 1.0f;
    const float over = abs_sample - threshold;
    const float knee = std::max(1.0f - threshold, 0.001f);
    const float normalized = std::clamp(over / knee, 0.0f, 1.0f);
    const float compressed = threshold + knee * std::tanh(normalized);
    return sign * std::min(compressed, 0.999f);
}

} // namespace

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
    const float release = 0.06f;
    if (target_gain < current_gain_) {
        current_gain_ = target_gain;
    } else {
        current_gain_ += (target_gain - current_gain_) * release;
    }

    for (float& sample : frame) {
        sample = soft_clip(sample * current_gain_, threshold_);
    }
}

} // namespace grotto::voice
