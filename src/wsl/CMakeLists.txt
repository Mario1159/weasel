file(GLOB_RECURSE WSL_SOURCES
    "*.cpp"
    "*.c"
    "log/*.cpp"
)

list(APPEND WSL_SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/editor_app.cpp"
)

# Editor-specific sources are always compiled into wsl (they are guarded
# internally by the WEASEL_BUILD_EDITOR define).
list(APPEND WSL_SOURCES
    "${CMAKE_SOURCE_DIR}/src/editor/renderer_imgui.cpp"
    "${CMAKE_SOURCE_DIR}/src/editor/physics_debug_drawer.cpp"
    "${CMAKE_SOURCE_DIR}/src/editor/physics_debug_renderer.cpp"
)

# RenderDoc integration is opt-in. When OFF, drop the renderdoc_*.cpp
# sources that the recursive GLOB picked up so the vendored header is
# not pulled into the build at all.
if(NOT WEASEL_ENABLE_RENDERDOC)
    list(FILTER WSL_SOURCES EXCLUDE REGEX "renderdoc")
endif()

add_library(wsl SHARED ${WSL_SOURCES})
add_library(Weasel::wsl ALIAS wsl)

set_target_properties(wsl PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
)

if(APPLE)
    # On macOS the editor executable lives in
    # Weasel.app/Contents/MacOS/ and loads this dylib from
    # Weasel.app/Contents/Frameworks/. Use @rpath so the editor's
    # rpath (@executable_path/../Frameworks) resolves to this dylib,
    # and give the dylib itself a loader_path rpath so any chained
    # load (e.g. libTracyClient.dylib, which is colocated with it)
    # is also found.
    set_target_properties(wsl PROPERTIES
        INSTALL_NAME_DIR "@rpath"
        INSTALL_RPATH "@loader_path"
        BUILD_WITH_INSTALL_RPATH TRUE
    )
endif()

target_include_directories(wsl PUBLIC ${CMAKE_SOURCE_DIR}/src)
target_include_directories(wsl PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

target_link_libraries(wsl PUBLIC
    rmlui_backend_sdl_gl3
    SDL3::SDL3
    SDL3_image::SDL3_image
    SDL3_mixer::SDL3_mixer
    RmlUi::RmlUi
    Jolt
    fastgltf
    meshoptimizer
    EnTT::EnTT
    spdlog
    reflectcpp
    msgpack-c
    imguizmo
    imguitextselect
    imsearch
    imviewguizmo::imviewguizmo
    ImGuiColorTextEdit::imguicolortextedit
    glm_dep
    slang::slang
)

if(TARGET weasel_daslang)
    target_link_libraries(wsl PUBLIC weasel_daslang)
endif()

target_link_libraries(wsl PRIVATE
    $<$<BOOL:${WEASEL_ENABLE_MULTIPLAYER}>:GameNetworkingSockets_s>
    LibArchive::LibArchive
    ${CMAKE_DL_LIBS}
)

# Used by the runtime shader compiler to parse slangc's reflection JSON.
if(TARGET nlohmann_json::nlohmann_json)
    target_link_libraries(wsl PRIVATE nlohmann_json::nlohmann_json)
endif()

# A2A protocol support
if(TARGET simdjson::simdjson)
    target_sources(wsl PRIVATE
        ai/a2a/types.cpp
        ai/a2a/agent_card.cpp
        ai/a2a/errors.cpp
        ai/a2a/json_util.cpp
        ai/a2a/client.cpp
        ai/a2a/server.cpp
        ai/a2a/discovery.cpp
        ai/a2a/streaming.cpp
        ai/a2a/http/http_client.cpp
        ai/a2a/http/http_server.cpp
        ai/a2a/http/json_rpc_server.cpp
    )
    target_link_libraries(wsl PRIVATE simdjson::simdjson CURL::libcurl)
endif()

# ACP (Agent Client Protocol) support
if(TARGET simdjson::simdjson)
    target_sources(wsl PRIVATE
        ai/acp/acp_types.cpp
        ai/acp/acp_client.cpp
        ai/acp/acp_session.cpp
        ai/acp/acp_agent_manager.cpp
    )
    target_link_libraries(wsl PRIVATE simdjson::simdjson)
endif()

# Absolute path to the slangc executable deployed next to the editor, so the
# runtime shader compiler can find it regardless of the working directory.
if(TARGET slangc)
    target_compile_definitions(wsl PRIVATE
        WEASEL_SLANGC_PATH="${CMAKE_BINARY_DIR}/slangc")
endif()

# Tracy is PUBLIC because wsl's public headers (system.hpp in
# particular) now use ZoneScopedN / ZoneText / FrameMark — anything
# that includes wsl public headers needs the Tracy include path.
target_link_libraries(wsl PUBLIC TracyClient)

if(TARGET imgui)
    target_link_libraries(wsl PUBLIC imgui)
endif()

target_compile_definitions(wsl PUBLIC JPH_DEBUG_RENDERER CPP_RTTI_ENABLED)
target_compile_definitions(wsl PRIVATE WEASEL_BUILD_EDITOR)
if(WEASEL_ENABLE_MULTIPLAYER)
    target_compile_definitions(wsl PUBLIC WEASEL_ENABLE_MULTIPLAYER)
endif()
target_compile_definitions(wsl PRIVATE
    WEASEL_SOURCE_DIR=\"${CMAKE_SOURCE_DIR}\"
    WEASEL_BUILD_DIR=\"${CMAKE_BINARY_DIR}\"
)

if(WEASEL_ENABLE_RENDERDOC)
    # Header-only dependency: the .so/.dll is loaded at runtime.
    target_include_directories(wsl PRIVATE
        "${CMAKE_SOURCE_DIR}/cmake/renderdoc")
    target_compile_definitions(wsl PRIVATE WEASEL_ENABLE_RENDERDOC)
endif()

# Tracy instrumentation. The TracyClient library is already linked
# unconditionally (see PRIVATE link block above). The TRACY_ENABLE
# define activates the client macros (FrameMark, FrameImage,
# TracyAllocN/FreeN, TracyPlot, TracyParameterSetup, TracyAppInfo,
# TracyMessageLC, ...) so they actually call into the library.
# Without this define, every Tracy macro compiles to a no-op and
# the entire integration is silent.
#
# Always on by default — when no profiler is attached, the
# per-frame work is gated on TracyIsConnected (see the frame_image
# helpers in render_window.cpp) so the overhead is only a single
# atomic load per frame. The Tracy client is also built with
# TRACY_ON_DEMAND, so the network thread stays quiescent until a
# profiler connects.
target_compile_definitions(wsl PRIVATE TRACY_ENABLE TRACY_HAS_CALLGRIND=0)

if(MSVC)
    target_compile_options(wsl PRIVATE /W4 /permissive-)
else()
    target_compile_options(wsl PRIVATE -Wall -Wextra -Wpedantic -rdynamic)
endif()

# --- C++20 modules (opt-in, see WEASEL_ENABLE_MODULES in root CMakeLists) ---
# Real named modules, promoted per namespace as the include graph is
# untangled (see CXX_MODULES_PLAN_V2.md). `wsl.math` is the first: it is
# compiled by a custom command (no -fmodule-mapper, so GCC resolves the CMI
# from gcm.cache) and the resulting object is linked into consumers that
# `import wsl.math;`.
if(WEASEL_ENABLE_MODULES)
  # Response file carrying the wsl target's include directories. GCC splits
  # the response file on whitespace; empty entries must be filtered (an empty
  # -I adds CWD to the search path and breaks CMI key matching).
  set(_math_rsp "${CMAKE_BINARY_DIR}/gcm.cache/math_incs.rsp")
  file(GENERATE OUTPUT "${_math_rsp}"
       CONTENT "$<JOIN:$<LIST:TRANSFORM,$<LIST:FILTER,$<TARGET_PROPERTY:wsl,INCLUDE_DIRECTORIES>,EXCLUDE,^$>,PREPEND,-I>,
>")

  add_custom_command(
    OUTPUT "${CMAKE_BINARY_DIR}/gcm.cache/wsl_math.stamp"
           "${CMAKE_BINARY_DIR}/wsl_math.o"
    COMMAND ${CMAKE_COMMAND} -E make_directory
            "${CMAKE_BINARY_DIR}/gcm.cache"
    COMMAND ${CMAKE_CXX_COMPILER} -std=c++20 -fmodules-ts
            @${_math_rsp}
            -c "${CMAKE_CURRENT_SOURCE_DIR}/math/math.cppm"
            -o "${CMAKE_BINARY_DIR}/wsl_math.o"
    COMMAND ${CMAKE_COMMAND} -E touch
            "${CMAKE_BINARY_DIR}/gcm.cache/wsl_math.stamp"
    WORKING_DIRECTORY "${CMAKE_BINARY_DIR}"
    COMMENT "Building wsl.math module interface"
    VERBATIM
    DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/math/math.cppm"
  )
  add_custom_target(wsl_math_module
      DEPENDS "${CMAKE_BINARY_DIR}/gcm.cache/wsl_math.stamp")
endif()

