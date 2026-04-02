#pragma once

#include <vector>

namespace grotto::voice {

class Limiter {
public:
    void configure(bool enabled, float threshold);
    void reset();
    void process(std::vector<float>& frame);

private:
    bool enabled_ = false;
    float threshold_ = 0.85f;
    float current_gain_ = 1.0f;
};

} // namespace grotto::voice
