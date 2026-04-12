#include "voice/noise_suppressor.hpp"

#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

#include <cmath>

namespace grotto::voice {

TEST_CASE("noise suppressor batches 5 ms capture chunks into 10 ms frames", "[voice][ns]") {
    NoiseSuppressor suppressor;
    REQUIRE(suppressor.configure(false, "moderate"));

    std::vector<float> first(240, 0.25f);
    std::vector<float> second(240, -0.25f);

    auto out_first = suppressor.process_capture_chunk(first.data(), static_cast<uint32_t>(first.size()));
    auto out_second = suppressor.process_capture_chunk(second.data(), static_cast<uint32_t>(second.size()));

    REQUIRE(out_first.empty());
    REQUIRE(out_second.size() == 1);
    REQUIRE(out_second.front().size() == NoiseSuppressor::kInputFrameSamples);
    REQUIRE(out_second.front().front() == Catch::Approx(0.25f));
    REQUIRE(out_second.front().back() == Catch::Approx(-0.25f));
}

TEST_CASE("noise suppressor passthrough preserves frame size when enabled", "[voice][ns]") {
    NoiseSuppressor suppressor;
    suppressor.configure(true, "moderate");

    std::vector<float> frame(NoiseSuppressor::kInputFrameSamples, 0.1f);
    auto out = suppressor.process_capture_chunk(frame.data(), static_cast<uint32_t>(frame.size()));

    REQUIRE(out.size() == 1);
    REQUIRE(out.front().size() == frame.size());
}

TEST_CASE("noise suppressor preserves voiced output over repeated frames", "[voice][ns]") {
    NoiseSuppressor suppressor;
    REQUIRE(suppressor.configure(true, "moderate"));

    constexpr float kAmplitude = 0.2f;
    constexpr float kFrequencyHz = 220.0f;
    constexpr size_t kFrameCount = 12;

    const auto rms = [](const std::vector<float>& samples) {
        float energy = 0.0f;
        for (float sample : samples) {
            energy += sample * sample;
        }
        return std::sqrt(energy / static_cast<float>(samples.size()));
    };

    float input_rms = 0.0f;
    float max_output_rms = 0.0f;
    float trailing_output_rms_sum = 0.0f;
    size_t trailing_output_frames = 0;

    for (size_t frame_index = 0; frame_index < kFrameCount; ++frame_index) {
        std::vector<float> frame(NoiseSuppressor::kInputFrameSamples);
        const size_t sample_offset = frame_index * NoiseSuppressor::kInputFrameSamples;
        for (size_t i = 0; i < frame.size(); ++i) {
            const float t = static_cast<float>(sample_offset + i) /
                static_cast<float>(NoiseSuppressor::kInputSampleRate);
            frame[i] = kAmplitude * std::sin(2.0f * 3.14159265358979323846f * kFrequencyHz * t);
        }

        const auto out = suppressor.process_capture_chunk(frame.data(), static_cast<uint32_t>(frame.size()));
        REQUIRE(out.size() == 1);
        REQUIRE(out.front().size() == frame.size());

        input_rms = rms(frame);
        const float output_rms = rms(out.front());
        max_output_rms = std::max(max_output_rms, output_rms);

        // Skip the first few frames so the test tolerates model warm-up/state adaptation.
        if (frame_index >= 4) {
            trailing_output_rms_sum += output_rms;
            ++trailing_output_frames;
        }
    }

    REQUIRE(input_rms > 0.01f);
    REQUIRE(trailing_output_frames > 0);
    REQUIRE(max_output_rms > input_rms * 0.01f);
    REQUIRE((trailing_output_rms_sum / static_cast<float>(trailing_output_frames)) > input_rms * 0.001f);
}

TEST_CASE("noise suppressor clear_pending drops partial capture frames", "[voice][ns]") {
    NoiseSuppressor suppressor;
    REQUIRE(suppressor.configure(true, "moderate"));

    std::vector<float> partial(240, 0.1f);
    auto out_partial = suppressor.process_capture_chunk(partial.data(), static_cast<uint32_t>(partial.size()));
    REQUIRE(out_partial.empty());

    suppressor.clear_pending();

    std::vector<float> frame(NoiseSuppressor::kInputFrameSamples, -0.2f);
    auto out = suppressor.process_capture_chunk(frame.data(), static_cast<uint32_t>(frame.size()));

    REQUIRE(out.size() == 1);
    REQUIRE(out.front().size() == NoiseSuppressor::kInputFrameSamples);
}

TEST_CASE("noise suppressor reset returns to passthrough-disabled state", "[voice][ns]") {
    NoiseSuppressor suppressor;
    REQUIRE(suppressor.configure(true, "moderate"));
    REQUIRE(suppressor.enabled());

    suppressor.reset();

    REQUIRE_FALSE(suppressor.enabled());
    REQUIRE_FALSE(suppressor.operational());

    std::vector<float> frame(NoiseSuppressor::kInputFrameSamples, 0.05f);
    auto out = suppressor.process_capture_chunk(frame.data(), static_cast<uint32_t>(frame.size()));

    REQUIRE(out.size() == 1);
    REQUIRE(out.front() == frame);
}

} // namespace grotto::voice
