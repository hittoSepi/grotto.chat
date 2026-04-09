#pragma once

#include "ui/graphics_layout.hpp"

#include <vector>

namespace grotto::ui {

struct GraphicsFrame {
    int viewport_x = 0;
    int viewport_y = 0;
    int viewport_width = 0;
    int viewport_height = 0;
    std::vector<GraphicsDrawCommand> commands;
};

class GraphicsCompositor {
public:
    void prepare_for_frame(const GraphicsFrame& frame);
    void commit(GraphicsFrame frame);
    void invalidate_all();

private:
    bool needs_full_clear(const GraphicsFrame& frame) const;

    GraphicsFrame last_frame_;
    bool had_native_graphics_ = false;
    bool requires_full_clear_ = false;
};

} // namespace grotto::ui
