#pragma once
#include <cstdint>
#include <map>
#include <mutex>
#include <optional>
#include <vector>

namespace grotto::voice {

struct AudioFrame {
    uint16_t             seq;
    std::vector<float>   pcm;    // kFrameSamples floats
};

// Reorder buffer keyed by RTP sequence number.
// Holds up to kWindowSize frames; pops in-order with PLC on gap.
class JitterBuffer {
public:
    explicit JitterBuffer(int target_delay_frames = 4);

    // Push a received (possibly out-of-order) audio frame.
    void push(uint16_t seq, std::vector<float> pcm);

    // Pop the next in-sequence frame for playback.
    // Returns nullopt if buffer not yet primed or gap detected (caller should PLC).
    std::optional<std::vector<float>> pop();

    void reset();

    int buffered_count() const;
    bool is_primed() const;

private:
    mutable std::mutex   mu_;
    std::map<uint16_t, std::vector<float>> frames_;
    uint16_t next_seq_   = 0;
    bool     primed_     = false;
    int      target_delay_;

    static constexpr int kWindowSize = 16;
};

} // namespace grotto::voice
