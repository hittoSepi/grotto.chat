#pragma once

#include "voice/pcm_sample_fifo.hpp"

#include <cstdint>
#include <string>
#include <memory>
#include <vector>

namespace grotto::voice {

class NoiseSuppressor {
public:
    static constexpr uint32_t kInputSampleRate = 48000;
    static constexpr uint32_t kInputFrameSamples = 480; // 10 ms @ 48 kHz
    static constexpr uint32_t kProcessingSampleRate = 48000;
    static constexpr uint32_t kProcessingFrameSamples = 480; // 10 ms @ 48 kHz

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
    struct Impl;

    std::vector<float> process_frame_10ms(const std::vector<float>& frame_48k);

    std::unique_ptr<Impl> impl_;
    PcmSampleFifo input_fifo_;
    bool enabled_ = false;
    bool initialized_ = false;
    bool init_warning_logged_ = false;
    std::string level_ = "moderate";
};

} // namespace grotto::voice
