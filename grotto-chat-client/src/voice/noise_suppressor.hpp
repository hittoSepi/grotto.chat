#pragma once

#include "voice/pcm_sample_fifo.hpp"

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace grotto::voice {

class NoiseSuppressor {
public:
    static constexpr uint32_t kInputSampleRate = 48000;
    static constexpr uint32_t kInputFrameSamples = 480; // 10 ms @ 48 kHz
    static constexpr uint32_t kProcessingSampleRate = 16000;
    static constexpr uint32_t kProcessingFrameSamples = 160; // 10 ms @ 16 kHz

    NoiseSuppressor();
    ~NoiseSuppressor();

    bool configure(bool enabled, const std::string& level);
    void reset();
    void clear_pending();

    std::vector<std::vector<float>> process_capture_chunk(const float* pcm, uint32_t frames);

    [[nodiscard]] bool enabled() const { return enabled_; }
    [[nodiscard]] bool operational() const { return enabled_ && initialized_; }
    [[nodiscard]] bool build_available() const;
    [[nodiscard]] const std::string& level() const { return level_; }

private:
    std::vector<float> process_frame_10ms(const std::vector<float>& frame_48k);
    static int policy_for_level(const std::string& level);
    static float compute_rms(const std::vector<float>& samples);
    static std::vector<int16_t> downsample_48k_to_16k(const std::vector<float>& frame_48k);
    static std::vector<float> upsample_16k_to_48k(const std::vector<int16_t>& frame_16k);
    static int16_t float_to_s16(float sample);
    static float s16_to_float(int16_t sample);

    void* handle_ = nullptr;
    PcmSampleFifo input_fifo_;
    bool enabled_ = false;
    bool initialized_ = false;
    bool init_warning_logged_ = false;
    std::string level_ = "moderate";
};

} // namespace grotto::voice
