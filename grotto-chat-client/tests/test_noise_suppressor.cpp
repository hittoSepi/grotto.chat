#include "voice/noise_suppressor.hpp"

#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

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
