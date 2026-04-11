#include "voice/noise_suppressor.hpp"

#include <spdlog/spdlog.h>

#include <algorithm>
#include <array>
#include <cmath>

#if defined(GROTTO_HAS_RNNOISE) && GROTTO_HAS_RNNOISE
extern "C" {
#include <rnnoise.h>
}
#endif

namespace grotto::voice {

namespace {

constexpr float kRnnoisePcmScale = 32768.0f;

class NoiseSuppressorBackend {
public:
    virtual ~NoiseSuppressorBackend() = default;
    virtual bool initialize() = 0;
    virtual void reset() = 0;
    virtual void process_frame(const float* input, float* output) = 0;
};

#if defined(GROTTO_HAS_RNNOISE) && GROTTO_HAS_RNNOISE
class RnnoiseBackend final : public NoiseSuppressorBackend {
public:
    ~RnnoiseBackend() override {
        reset();
    }

    bool initialize() override {
        state_ = rnnoise_create(nullptr);
        return state_ != nullptr;
    }

    void reset() override {
        if (state_ != nullptr) {
            rnnoise_destroy(state_);
            state_ = nullptr;
        }
    }

    void process_frame(const float* input, float* output) override {
        if (state_ == nullptr || input == nullptr || output == nullptr) {
            return;
        }

        std::array<float, NoiseSuppressor::kInputFrameSamples> scaled_input {};
        std::array<float, NoiseSuppressor::kInputFrameSamples> scaled_output {};
        for (size_t i = 0; i < scaled_input.size(); ++i) {
            scaled_input[i] = input[i] * kRnnoisePcmScale;
        }

        rnnoise_process_frame(state_, scaled_output.data(), scaled_input.data());

        for (size_t i = 0; i < scaled_output.size(); ++i) {
            output[i] = std::clamp(scaled_output[i] / kRnnoisePcmScale, -1.0f, 1.0f);
        }
    }

private:
    DenoiseState* state_ = nullptr;
};
#endif

} // namespace

struct NoiseSuppressor::Impl {
    std::unique_ptr<NoiseSuppressorBackend> backend;
};

NoiseSuppressor::NoiseSuppressor()
    : impl_(std::make_unique<Impl>()) {}

NoiseSuppressor::~NoiseSuppressor() = default;

bool NoiseSuppressor::configure(bool enabled, const std::string& level) {
    reset();
    enabled_ = enabled;
    level_ = level.empty() ? std::string("moderate") : level;

    if (!enabled_) {
        return true;
    }

#if defined(GROTTO_HAS_RNNOISE) && GROTTO_HAS_RNNOISE
    impl_->backend = std::make_unique<RnnoiseBackend>();
    if (!impl_->backend->initialize()) {
        impl_->backend.reset();
        if (!init_warning_logged_) {
            spdlog::warn("Noise suppression requested but RNNoise init failed; using passthrough audio");
            init_warning_logged_ = true;
        }
        return false;
    }
    initialized_ = true;
    return true;
#else
    if (!init_warning_logged_) {
        spdlog::warn("Noise suppression requested but build has no RNNoise support; using passthrough audio");
        init_warning_logged_ = true;
    }
    return false;
#endif
}

void NoiseSuppressor::reset() {
    input_fifo_.clear();
    enabled_ = false;
    initialized_ = false;
    init_warning_logged_ = false;
    last_frame_change_ratio_.store(0.0f, std::memory_order_relaxed);
    last_frame_modified_.store(false, std::memory_order_relaxed);
    if (impl_ && impl_->backend) {
        impl_->backend->reset();
        impl_->backend.reset();
    }
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
#if defined(GROTTO_HAS_RNNOISE) && GROTTO_HAS_RNNOISE
    return true;
#else
    return false;
#endif
}

std::vector<float> NoiseSuppressor::process_frame_10ms(const std::vector<float>& frame_48k) {
    if (frame_48k.size() != kInputFrameSamples || !enabled_ || !initialized_ || !impl_ || !impl_->backend) {
        last_frame_change_ratio_.store(0.0f, std::memory_order_relaxed);
        last_frame_modified_.store(false, std::memory_order_relaxed);
        return frame_48k;
    }

    std::vector<float> output(frame_48k.size(), 0.0f);
    impl_->backend->process_frame(frame_48k.data(), output.data());

    float input_energy = 0.0f;
    float diff_energy = 0.0f;
    for (size_t i = 0; i < frame_48k.size(); ++i) {
        const float in = frame_48k[i];
        const float out = output[i];
        input_energy += in * in;
        const float diff = out - in;
        diff_energy += diff * diff;
    }

    const float ratio = input_energy > 1e-9f
        ? std::sqrt(diff_energy / input_energy)
        : 0.0f;
    last_frame_change_ratio_.store(std::clamp(ratio, 0.0f, 1.0f), std::memory_order_relaxed);
    last_frame_modified_.store(ratio > 0.001f, std::memory_order_relaxed);

    return output;
}

} // namespace grotto::voice
