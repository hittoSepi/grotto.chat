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

} // namespace grotto::voice
