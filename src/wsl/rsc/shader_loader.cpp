#include "shader_loader.hpp"
#include <SDL3/SDL_iostream.h>
#include <SDL3/SDL_stdinc.h>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <spdlog/spdlog.h>
#include <string>

namespace wsl::rsc {

std::shared_ptr<gfx::shader_module> shader_loader::operator()(const std::string& path) const {
    size_t size = 0;
    void* data = SDL_LoadFile(path.c_str(), &size);
    if (data == nullptr) {
        spdlog::error("shader_loader: failed to load shader bytecode from {}", path);
        return nullptr;
    }

    auto module = std::make_shared<gfx::shader_module>();
    module->bytecode.assign(static_cast<uint8_t*>(data), static_cast<uint8_t*>(data) + size);
    SDL_free(data);

    return module;
}

} // namespace wsl::rsc
