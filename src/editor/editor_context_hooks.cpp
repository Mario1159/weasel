#include "wsl/comp/singl/editor_context.hpp"
#include "renderer_imgui.hpp"
#include "physics_debug_renderer.hpp"
#include <memory>
#include <spdlog/spdlog.h>

namespace editor {

static void editor_context_on_init(wsl::comp::singl::editor_context* ctx) {
    spdlog::debug("editor_context_hooks: initializing editor-specific renderers");
    ctx->imgui_renderer = std::make_unique<editor::renderer_imgui>(ctx->runtime_ctx.window, &ctx->runtime_ctx.render_ctx);

    // make_physics_debug_renderer returns unique_ptr which can be assigned to the interface-typed unique_ptr
    ctx->debug_renderer = editor::make_physics_debug_renderer(ctx->runtime_ctx.window, &ctx->runtime_ctx.render_ctx);
}

static void editor_context_on_deinit(wsl::comp::singl::editor_context* ctx) {
    spdlog::debug("editor_context_hooks: deinitializing editor-specific renderers");
    // unique_ptr members will be cleaned up automatically
    ctx->imgui_renderer.reset();
    ctx->debug_renderer.reset();
}

} // namespace editor

namespace wsl::comp::singl {

editor::renderer_imgui* editor_context::get_imgui_renderer() const {
    return imgui_renderer ? imgui_renderer.get() : nullptr;
}

wsl::debug::debug_renderer_interface* editor_context::get_debug_renderer() const {
    return debug_renderer ? debug_renderer.get() : nullptr;
}

} // namespace wsl::comp::singl

namespace editor {

struct hook_registrator {
    hook_registrator() {
        wsl::comp::singl::editor_context::on_init_hook = editor_context_on_init;
        wsl::comp::singl::editor_context::on_deinit_hook = editor_context_on_deinit;
    }
};

static hook_registrator& get_hooks() {
    static hook_registrator instance;
    return instance;
}

} // namespace editor

// Force initialization at program startup via dynamic initializer
static bool g_force_init = (editor::get_hooks(), true);
