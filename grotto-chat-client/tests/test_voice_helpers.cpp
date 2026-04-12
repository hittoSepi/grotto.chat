#include "voice/opus_codec.hpp"
#include "voice/pcm_sample_fifo.hpp"
#include "voice/voice_activity_gate.hpp"
#include "voice/voice_mode.hpp"
#include "voice/voice_peer_role.hpp"

#include <catch2/catch_test_macros.hpp>

#include <vector>

TEST_CASE("capture FIFO emits full opus frames and keeps remainder", "[voice]") {
    grotto::voice::PcmSampleFifo fifo;

    std::vector<float> chunk_a(240, 1.0f);
    std::vector<float> chunk_b(240, 2.0f);
    std::vector<float> chunk_c(240, 3.0f);
    std::vector<float> chunk_d(240, 4.0f);
    std::vector<float> chunk_e(240, 5.0f);

    fifo.push(chunk_a);
    fifo.push(chunk_b);
    fifo.push(chunk_c);
    REQUIRE_FALSE(fifo.pop_exact(grotto::voice::OpusCodec::kFrameSamples).has_value());

    fifo.push(chunk_d);
    auto frame = fifo.pop_exact(grotto::voice::OpusCodec::kFrameSamples);
    REQUIRE(frame.has_value());
    REQUIRE(frame->size() == static_cast<size_t>(grotto::voice::OpusCodec::kFrameSamples));
    REQUIRE((*frame)[0] == 1.0f);
    REQUIRE((*frame)[239] == 1.0f);
    REQUIRE((*frame)[240] == 2.0f);
    REQUIRE((*frame)[479] == 2.0f);
    REQUIRE((*frame)[480] == 3.0f);
    REQUIRE((*frame)[719] == 3.0f);
    REQUIRE((*frame)[720] == 4.0f);
    REQUIRE((*frame)[959] == 4.0f);

    fifo.push(chunk_e);
    REQUIRE(fifo.size() == 240);
}

TEST_CASE("playout FIFO returns four 5ms chunks from one opus frame", "[voice]") {
    grotto::voice::PcmSampleFifo fifo;
    std::vector<float> frame(grotto::voice::OpusCodec::kFrameSamples);
    for (size_t i = 0; i < frame.size(); ++i) {
        frame[i] = static_cast<float>(i);
    }
    fifo.push(frame);

    for (int chunk = 0; chunk < 4; ++chunk) {
        std::vector<float> out(240, 0.0f);
        const size_t mixed = fifo.mix_into(out.data(), static_cast<uint32_t>(out.size()));
        REQUIRE(mixed == 240);
        for (size_t i = 0; i < out.size(); ++i) {
            REQUIRE(out[i] == static_cast<float>(chunk * 240 + static_cast<int>(i)));
        }
    }

    REQUIRE(fifo.empty());
}

TEST_CASE("room offer role is deterministic", "[voice]") {
    REQUIRE(grotto::voice::should_offer_to_peer("alice", "bob"));
    REQUIRE_FALSE(grotto::voice::should_offer_to_peer("bob", "alice"));
}

TEST_CASE("voice activity gate keeps vox open briefly across rnnoise dips", "[voice]") {
    grotto::voice::VoiceActivityGate gate;

    const auto opened = gate.update(0.030f, 0.020f, 1000);
    REQUIRE(opened.gate_open);
    REQUIRE(opened.signal_detected);

    const auto held = gate.update(0.010f, 0.020f, 1120);
    REQUIRE(held.gate_open);
    REQUIRE_FALSE(held.signal_detected);

    const auto closed = gate.update(0.010f, 0.020f, 1185);
    REQUIRE_FALSE(closed.gate_open);
    REQUIRE_FALSE(closed.signal_detected);
}

TEST_CASE("voice activity gate uses lower close threshold after opening", "[voice]") {
    grotto::voice::VoiceActivityGate gate;

    const auto opened = gate.update(0.025f, 0.020f, 1000);
    REQUIRE(opened.gate_open);
    REQUIRE(opened.signal_detected);

    const auto sustained = gate.update(0.015f, 0.020f, 1040);
    REQUIRE(sustained.gate_open);
    REQUIRE(sustained.signal_detected);
}

TEST_CASE("voice mode helper normalizes legacy and cycles through all modes", "[voice]") {
    REQUIRE(grotto::voice::normalize_voice_mode("ptt") == "toggle");
    REQUIRE(grotto::voice::normalize_voice_mode("hold") == "hold");
    REQUIRE(grotto::voice::normalize_voice_mode("vox") == "vox");

    REQUIRE(grotto::voice::next_voice_mode("toggle") == "hold");
    REQUIRE(grotto::voice::next_voice_mode("hold") == "vox");
    REQUIRE(grotto::voice::next_voice_mode("vox") == "toggle");
}
