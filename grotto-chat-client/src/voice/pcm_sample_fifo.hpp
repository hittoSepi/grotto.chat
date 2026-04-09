#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <optional>
#include <vector>

namespace grotto::voice {

class PcmSampleFifo {
public:
    void clear() { samples_.clear(); }

    void push(const float* pcm, uint32_t frames) {
        if (!pcm || frames == 0) {
            return;
        }
        samples_.insert(samples_.end(), pcm, pcm + frames);
    }

    void push(const std::vector<float>& pcm) {
        if (pcm.empty()) {
            return;
        }
        samples_.insert(samples_.end(), pcm.begin(), pcm.end());
    }

    [[nodiscard]] size_t size() const {
        return samples_.size();
    }

    [[nodiscard]] bool empty() const {
        return samples_.empty();
    }

    std::optional<std::vector<float>> pop_exact(size_t frames) {
        if (samples_.size() < frames) {
            return std::nullopt;
        }

        std::vector<float> out;
        out.reserve(frames);
        for (size_t i = 0; i < frames; ++i) {
            out.push_back(samples_.front());
            samples_.pop_front();
        }
        return out;
    }

    size_t discard_front(size_t frames) {
        const size_t count = std::min(frames, samples_.size());
        for (size_t i = 0; i < count; ++i) {
            samples_.pop_front();
        }
        return count;
    }

    size_t mix_into(float* out, uint32_t frames) {
        if (!out || frames == 0 || samples_.empty()) {
            return 0;
        }

        const size_t count = std::min<size_t>(frames, samples_.size());
        for (size_t i = 0; i < count; ++i) {
            out[i] += samples_.front();
            samples_.pop_front();
        }
        return count;
    }

private:
    std::deque<float> samples_;
};

} // namespace grotto::voice
