#include "voice/noise_suppressor.hpp"

#include <spdlog/spdlog.h>

#include <algorithm>
#include <cmath>

#if defined(GROTTO_HAS_WEBRTC_NS) && GROTTO_HAS_WEBRTC_NS
extern "C" {
#include <noise_suppression.h>
}
#endif

namespace grotto::voice {

NoiseSuppressor::NoiseSuppressor() = default;

NoiseSuppressor::~NoiseSuppressor() {
#if defined(GROTTO_HAS_WEBRTC_NS) && GROTTO_HAS_WEBRTC_NS
    if (handle_) {
        WebRtcNs_Free(static_cast<NsHandle*>(handle_));
        handle_ = nullptr;
    }
#endif
}

bool NoiseSuppressor::configure(bool enabled, const std::string& level) {
    reset();
    enabled_ = enabled;
    level_ = level.empty() ? std::string("moderate") : level;

    if (!enabled_) {
        return true;
    }

#if defined(GROTTO_HAS_WEBRTC_NS) && GROTTO_HAS_WEBRTC_NS
    handle_ = WebRtcNs_Create();
    if (!handle_) {
        if (!init_warning_logged_) {
            spdlog::warn("Noise suppression requested but WebRTC NS handle creation failed; using passthrough audio");
            init_warning_logged_ = true;
        }
        return false;
    }
    if (WebRtcNs_Init(static_cast<NsHandle*>(handle_), kProcessingSampleRate) != 0) {
        spdlog::warn("Noise suppression requested but WebRTC NS init failed; using passthrough audio");
        WebRtcNs_Free(static_cast<NsHandle*>(handle_));
        handle_ = nullptr;
        init_warning_logged_ = true;
        return false;
    }
    if (WebRtcNs_set_policy(static_cast<NsHandle*>(handle_), policy_for_level(level_)) != 0) {
        spdlog::warn("Noise suppression policy '{}' failed; using passthrough audio", level_);
        WebRtcNs_Free(static_cast<NsHandle*>(handle_));
        handle_ = nullptr;
        init_warning_logged_ = true;
        return false;
    }
    initialized_ = true;
    return true;
#else
    if (!init_warning_logged_) {
        spdlog::warn("Noise suppression requested but build has no WebRTC NS support; using passthrough audio");
        init_warning_logged_ = true;
    }
    return false;
#endif
}

void NoiseSuppressor::reset() {
    input_fifo_.clear();
    initialized_ = false;
    init_warning_logged_ = false;
#if defined(GROTTO_HAS_WEBRTC_NS) && GROTTO_HAS_WEBRTC_NS
    if (handle_) {
        WebRtcNs_Free(static_cast<NsHandle*>(handle_));
        handle_ = nullptr;
    }
#else
    handle_ = nullptr;
#endif
}

void NoiseSuppressor::clear_pending() {
    input_fifo_.clear();
}

std::vector<std::vector<float>> NoiseSuppressor::process_capture_chunk(const float* pcm, uint32_t frames) {
    std::vector<std::vector<float>> out;
    if (!pcm || frames == 0) {
        return out;
    }

    input_fifo_.push(pcm, frames);
    while (auto frame = input_fifo_.pop_exact(kInputFrameSamples)) {
        out.push_back(process_frame_10ms(*frame));
    }
    return out;
}

bool NoiseSuppressor::build_available() const {
#if defined(GROTTO_HAS_WEBRTC_NS) && GROTTO_HAS_WEBRTC_NS
    return true;
#else
    return false;
#endif
}

std::vector<float> NoiseSuppressor::process_frame_10ms(const std::vector<float>& frame_48k) {
    if (frame_48k.size() != kInputFrameSamples || !enabled_ || !initialized_ || !handle_) {
        return frame_48k;
    }

#if defined(GROTTO_HAS_WEBRTC_NS) && GROTTO_HAS_WEBRTC_NS
    auto frame_16k = downsample_48k_to_16k(frame_48k);
    std::vector<int16_t> output_16k(kProcessingFrameSamples);
    const int16_t* input_bands[1] = {frame_16k.data()};
    int16_t* output_bands[1] = {output_16k.data()};
    WebRtcNs_Analyze(static_cast<NsHandle*>(handle_), frame_16k.data());
    WebRtcNs_Process(static_cast<NsHandle*>(handle_), input_bands, 1, output_bands);
    return upsample_16k_to_48k(output_16k);
#else
    return frame_48k;
#endif
}

int NoiseSuppressor::policy_for_level(const std::string& level) {
    if (level == "low") {
        return 0;
    }
    if (level == "high" || level == "very_high") {
        return 2;
    }
    return 1;
}

std::vector<int16_t> NoiseSuppressor::downsample_48k_to_16k(const std::vector<float>& frame_48k) {
    std::vector<int16_t> out(kProcessingFrameSamples, 0);
    for (size_t i = 0; i < out.size(); ++i) {
        const size_t base = i * 3;
        const float avg = (frame_48k[base] + frame_48k[base + 1] + frame_48k[base + 2]) / 3.0f;
        out[i] = float_to_s16(avg);
    }
    return out;
}

std::vector<float> NoiseSuppressor::upsample_16k_to_48k(const std::vector<int16_t>& frame_16k) {
    std::vector<float> out(kInputFrameSamples, 0.0f);
    for (size_t i = 0; i < out.size(); ++i) {
        const float src_pos = static_cast<float>(i) / 3.0f;
        const auto index = static_cast<size_t>(src_pos);
        const size_t next = std::min(index + 1, frame_16k.size() - 1);
        const float frac = src_pos - static_cast<float>(index);
        const float a = s16_to_float(frame_16k[index]);
        const float b = s16_to_float(frame_16k[next]);
        out[i] = a + ((b - a) * frac);
    }
    return out;
}

int16_t NoiseSuppressor::float_to_s16(float sample) {
    const float clamped = std::clamp(sample, -1.0f, 1.0f);
    return static_cast<int16_t>(std::lrint(clamped * 32767.0f));
}

float NoiseSuppressor::s16_to_float(int16_t sample) {
    return static_cast<float>(sample) / 32768.0f;
}

} // namespace grotto::voice
