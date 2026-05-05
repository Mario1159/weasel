#pragma once

#include "wsl/debug/debug_renderer.hpp"

namespace wsl::phys {
class engine;
}

namespace editor {

void draw_physics_debug(wsl::phys::engine& engine, wsl::debug::debug_renderer_interface& renderer);
} // namespace editor
