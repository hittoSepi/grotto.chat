#include "voice/noise_suppressor.hpp"

#include <spdlog/spdlog.h>

#if defined(GROTTO_HAS_RNNOISE) && GROTTO_HAS_RNNOISE
extern "C" {
#include <rnnoise.h>
}
#endif

namespace grotto::voice {

namespace {

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
        rnnoise_process_frame(state_, output, input);
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
        return frame_48k;
    }

    std::vector<float> output(frame_48k.size(), 0.0f);
    impl_->backend->process_frame(frame_48k.data(), output.data());
    return output;
}

} // namespace grotto::voice
