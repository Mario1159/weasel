#pragma once

#include <vector>
#include <cstdint>
#include <string>
#include <memory>

namespace wsl::gfx {
    /** Represents a compiled shader module containing its bytecode. */
    struct shader_module {
        /** The compiled shader bytecode. */
        std::vector<uint8_t> bytecode;
    };
}

namespace wsl::rsc {
    class resource_manager;

    /**
 * Resource loader for shader modules.
 *
 * This class handles loading shader bytecode from the filesystem
 * and creating a gfx::shader_module from it.
 */
    class shader_loader {
    public:
        using result_type = std::shared_ptr<gfx::shader_module>;

        /**
 * Loads a shader module from the specified path.
 * :param path: The path to the shader bytecode file.
 * :return: A shared pointer to the loaded shader module.
 */
        std::shared_ptr<gfx::shader_module> operator()(const std::string& path) const;
    };
}
