cmake_minimum_required(VERSION 3.22)
project(weasel LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Export all symbols from shared libraries on Windows so that runtime-loaded
# .dll files can resolve symbols from the engine (wsl.dll).
if(WIN32)
  set(CMAKE_WINDOWS_EXPORT_ALL_SYMBOLS ON)
endif()

# Pass the C++ compiler path to C++ code so the runtime shared library cache
# can invoke the same compiler that built the engine.
add_compile_definitions(
  WEASEL_CXX_COMPILER="${CMAKE_CXX_COMPILER}"
)

option(WEASEL_DISCONNECTED_DEPENDENCY_UPDATES
    "Skip CPM/FetchContent git update steps for already-populated dependencies"
    ON)
option(WEASEL_LIMIT_NINJA_RESOURCES
    "Limit Ninja compile/link parallelism to keep the editor responsive"
    ON)
option(WEASEL_ENABLE_MULTIPLAYER
    "Enable multiplayer support via GameNetworkingSockets"
    ON)
# RenderDoc in-application API. The module is loaded at runtime via
# dlopen / GetModuleHandle, so there is no link-time dependency. Default
# ON for Debug, OFF for Release so shipping builds do not pull the
# header in. Capture options (vsync, callstacks, file template) and
# RENDERDOC_OPTION_* environment variables are honoured when ON.
option(WEASEL_ENABLE_RENDERDOC
    "Enable RenderDoc in-application API (programmatic capture + annotation)"
    $<IF:$<CONFIG:Debug>,ON,OFF>)

# Temporarily disable GameNetworkingSockets on Windows until abseil-cpp is fixed.
# See: https://github.com/abseil/abseil-cpp/pull/2054
if(WIN32 AND WEASEL_ENABLE_MULTIPLAYER)
    message(STATUS
        "Disabling WEASEL_ENABLE_MULTIPLAYER on Windows "
        "(temporarily until abseil-cpp PR #2054 is merged and published in vcpkg)"
    )
    set(WEASEL_ENABLE_MULTIPLAYER OFF CACHE BOOL
        "Enable multiplayer support via GameNetworkingSockets" FORCE)
endif()
set(WEASEL_NINJA_COMPILE_JOBS "4"
    CACHE STRING "Maximum number of concurrent compile jobs when using Ninja.")
set(WEASEL_NINJA_LINK_JOBS "1"
    CACHE STRING "Maximum number of concurrent link jobs when using Ninja.")

if(WEASEL_DISCONNECTED_DEPENDENCY_UPDATES)
    set(FETCHCONTENT_UPDATES_DISCONNECTED ON CACHE BOOL
        "Disable FetchContent update steps during reconfigure" FORCE)
endif()

if(CMAKE_GENERATOR MATCHES "Ninja" AND WEASEL_LIMIT_NINJA_RESOURCES)
    if(WEASEL_NINJA_COMPILE_JOBS GREATER 0)
        set_property(GLOBAL APPEND PROPERTY JOB_POOLS
            weasel_compile_pool=${WEASEL_NINJA_COMPILE_JOBS})
        set(CMAKE_JOB_POOL_COMPILE weasel_compile_pool)
    endif()

    if(WEASEL_NINJA_LINK_JOBS GREATER 0)
        set_property(GLOBAL APPEND PROPERTY JOB_POOLS
            weasel_link_pool=${WEASEL_NINJA_LINK_JOBS})
        set(CMAKE_JOB_POOL_LINK weasel_link_pool)
    endif()

    message(STATUS
        "Limiting Ninja job pools: compile=${WEASEL_NINJA_COMPILE_JOBS}, "
        "link=${WEASEL_NINJA_LINK_JOBS}"
    )
endif()

set(CMAKE_CXX_FLAGS_DEBUG "-O0 -g")

# --- Dependencies ---

# On Apple Silicon macOS, Homebrew installs libarchive headers under
# /opt/homebrew/opt/libarchive/include (and the lib at
# /opt/homebrew/opt/libarchive/lib). The find_path() inside CMake's
# LibArchive config will not always look there, so set the variables
# manually. On Intel macOS the prefix is /usr/local, which CMake
# already searches.
if(APPLE)
    set(LibArchive_INCLUDE_DIR "/opt/homebrew/opt/libarchive/include")
endif()

find_package(LibArchive REQUIRED)
find_package(PkgConfig REQUIRED)



pkg_check_modules(GLM QUIET glm)

if(GLM_FOUND)
    message(STATUS "Found glm via pkg-config: ${GLM_VERSION}")
    add_library(glm_dep INTERFACE)
    target_include_directories(glm_dep INTERFACE ${GLM_INCLUDE_DIRS})
    target_link_libraries(glm_dep INTERFACE ${GLM_LIBRARIES})
else()
    message(STATUS "glm not found via pkg-config, trying CMake package...")
    find_package(glm CONFIG REQUIRED)
    add_library(glm_dep ALIAS glm::glm)
endif()

include(cmake/CPM.cmake)
include(cmake/Daslang.cmake)

# SDL_STATIC_PIC=ON forces the static archive to be compiled with
# -fPIC. Required because wsl/editor are .so targets that link
# SDL3 statically; without PIC, ld.lld refuses with
# "R_X86_64_PC32 cannot be used against symbol ..." when resolving
# SDL_waylanddyn.c and friends.
set(SDL_STATIC_PIC ON CACHE BOOL "Build SDL3 static lib with -fPIC" FORCE)
CPMAddPackage(
    NAME SDL3
    GITHUB_REPOSITORY libsdl-org/SDL
    GIT_TAG release-3.4.10
    SYSTEM ON
)

CPMAddPackage(
    NAME stb
    GITHUB_REPOSITORY nothings/stb
    GIT_TAG master
    DOWNLOAD_ONLY TRUE
)

set(SLANG_ENABLE_GFX OFF CACHE BOOL "Disable slang GFX")
set(SLANG_ENABLE_SLANG_RHI OFF CACHE BOOL "Disable slang RHI")
set(SLANG_ENABLE_TESTS OFF CACHE BOOL "Disable slang tests")
set(SLANG_ENABLE_EXAMPLES OFF CACHE BOOL "Disable slang examples")
set(SLANG_ENABLE_CUDA OFF CACHE BOOL "Disable CUDA")
set(SLANG_ENABLE_NVAPI OFF CACHE BOOL "Disable NVAPI")
set(SLANG_ENABLE_OPTIX OFF CACHE BOOL "Disable OptiX")
set(SLANG_ENABLE_AFTERMATH OFF CACHE BOOL "Disable Aftermath")
set(SLANG_ENABLE_DXIL OFF CACHE BOOL "Disable DXIL")
set(SLANG_ENABLE_SLANGD OFF CACHE BOOL "Disable slangd")
set(SLANG_ENABLE_SLANGI OFF CACHE BOOL "Disable slangi")

CPMAddPackage(
    NAME imgui
    GITHUB_REPOSITORY Mario1159/imgui
    GIT_TAG docking-feature-cmake
    SYSTEM ON
    OPTIONS "CMAKE_POSITION_INDEPENDENT_CODE ON"
)

CPMAddPackage(
    NAME cli11
    GITHUB_REPOSITORY CLIUtils/CLI11
    GIT_TAG v2.6.2
    SYSTEM ON
)

CPMAddPackage(
    NAME cpp-mcp
    GITHUB_REPOSITORY hkr04/cpp-mcp
    GIT_TAG main
    SYSTEM ON
)

CPMAddPackage(
    NAME slang
    GITHUB_REPOSITORY shader-slang/slang
    GIT_TAG v2026.8.1
    OPTIONS
        "SLANG_LIB_TYPE STATIC"
    SYSTEM ON
)

if(NOT TARGET slang::slang)
    if(TARGET slang)
        # Slang built from source - wrap the raw target in the expected namespace
        add_library(slang::slang INTERFACE IMPORTED)
        set_property(TARGET slang::slang PROPERTY INTERFACE_LINK_LIBRARIES slang)
    else()
        file(GLOB SLANG_BUILD_DIRS "${CMAKE_BINARY_DIR}/_deps/slang-build/slang-*")
        if(SLANG_BUILD_DIRS)
            list(GET SLANG_BUILD_DIRS 0 SLANG_BUILD_DIR)
            find_library(SLANG_COMPILER_LIBRARY
                NAMES "slang-compiler"
                PATHS "${SLANG_BUILD_DIR}/lib"
            )

            if(SLANG_COMPILER_LIBRARY)
                add_library(slang::slang SHARED IMPORTED)
                set_target_properties(slang::slang PROPERTIES
                    IMPORTED_LOCATION "${SLANG_COMPILER_LIBRARY}"
                    INTERFACE_INCLUDE_DIRECTORIES "${SLANG_BUILD_DIR}/include"
                )
            endif()
        endif()
    endif()
endif()

CPMAddPackage(
    NAME cereal
    GITHUB_REPOSITORY USCiLab/cereal
    GIT_TAG v1.3.2
    OPTIONS "BUILD_SANDBOX OFF" "BUILD_TESTS OFF"
    SYSTEM ON
)

CPMAddPackage(
    NAME ImGuizmo
    GITHUB_REPOSITORY Mario1159/ImGuizmo
    GIT_TAG feature-cmake
    SYSTEM ON
)

CPMAddPackage(
    NAME ImSearch
    GITHUB_REPOSITORY Mario1159/imsearch
    GIT_TAG main
    OPTIONS "CMAKE_POSITION_INDEPENDENT_CODE ON"
    SYSTEM ON
)

CPMAddPackage(
    NAME ImViewGuizmo
    GITHUB_REPOSITORY Mario1159/ImViewGuizmo
    GIT_TAG main
    SYSTEM ON
)

CPMAddPackage(
    NAME ImNodeFlow
    GITHUB_REPOSITORY Fattorino/ImNodeFlow
    GIT_TAG master
    DOWNLOAD_ONLY TRUE
)

if(DEFINED ImNodeFlow_SOURCE_DIR)
    add_library(imnodeflow INTERFACE)
    target_include_directories(imnodeflow INTERFACE ${ImNodeFlow_SOURCE_DIR}/include)
    target_sources(imnodeflow INTERFACE ${ImNodeFlow_SOURCE_DIR}/src/ImNodeFlow.cpp)
endif()

CPMAddPackage(
    NAME ImGizmo2D
    GITHUB_REPOSITORY Miisan-png/ImGizmo2D
    GIT_TAG main
    DOWNLOAD_ONLY TRUE
)

if(DEFINED ImGizmo2D_SOURCE_DIR)
    add_library(imgizmo2d INTERFACE)
    target_include_directories(imgizmo2d INTERFACE ${ImGizmo2D_SOURCE_DIR})
endif()

CPMAddPackage(
    NAME ImGuiTextSelect
    GITHUB_REPOSITORY Mario1159/ImGuiTextSelect
    GIT_TAG main
    SYSTEM ON
)

CPMAddPackage(
    NAME ImGuiColorTextEdit
    GITHUB_REPOSITORY goossens/ImGuiColorTextEdit
    GIT_TAG future
    DOWNLOAD_ONLY TRUE
)

CPMAddPackage(
    NAME Tracy
    GITHUB_REPOSITORY wolfpld/tracy
    GIT_TAG v0.13.1
    OPTIONS
        "TRACY_ENABLE ON"
        "TRACY_ON_DEMAND ON"
        # Tracy must be a SHARED library (TRACY_STATIC OFF). As a static
        # lib, both `wsl.dll` and the editor executable would carry their
        # own `Profiler` instance and the `s_instance` assertion would
        # fire on the second construction. With a shared lib there is
        # exactly one `Profiler` global for the whole process.
        "TRACY_STATIC OFF"
    SYSTEM ON
)

CPMAddPackage(
    NAME SpdLog
    GITHUB_REPOSITORY gabime/spdlog
    GIT_TAG v1.17.0
    SYSTEM ON
)
set_target_properties(spdlog PROPERTIES POSITION_INDEPENDENT_CODE ON)

CPMAddPackage(
    NAME SDL3_mixer
    GITHUB_REPOSITORY libsdl-org/SDL_mixer
    GIT_TAG main
    SYSTEM ON
)

set(SDLIMAGE_AVIF OFF CACHE BOOL "Disable AVIF in SDL3_image" FORCE)
set(SDLIMAGE_JXL OFF CACHE BOOL "Disable JXL in SDL3_image" FORCE)
CPMAddPackage(
    NAME SDL3_image
    GITHUB_REPOSITORY libsdl-org/SDL_image
    GIT_TAG release-3.2.4
    SYSTEM ON
)

CPMAddPackage(
    NAME EnTT
    GITHUB_REPOSITORY skypjack/entt
    GIT_TAG v3.15.0
    SYSTEM ON
)

CPMAddPackage(
    NAME fastgltf
    GITHUB_REPOSITORY spnda/fastgltf
    GIT_TAG main
    SYSTEM ON
)

# meshoptimizer is amalgamated (single .cpp + header). Used by the model
# loader to reorder indices for GPU vertex cache efficiency before upload.
CPMAddPackage(
    NAME meshoptimizer
    GITHUB_REPOSITORY zeux/meshoptimizer
    GIT_TAG v0.24
    SYSTEM ON
)

if(WEASEL_ENABLE_MULTIPLAYER)
    CPMAddPackage(
        NAME GameNetworkingSockets
        GITHUB_REPOSITORY ValveSoftware/GameNetworkingSockets
        GIT_TAG master
        OPTIONS "BUILD_TESTS OFF"
        SYSTEM ON
    )
endif()

CPMAddPackage(
    NAME RmlUi
    GITHUB_REPOSITORY Mario1159/RmlUi
    GIT_TAG master
    SYSTEM ON
)

set(RMLUI_BACKEND_PATH ${RmlUi_SOURCE_DIR}/Backends)
add_library(rmlui_backend_sdl_gl3 INTERFACE)
target_sources(rmlui_backend_sdl_gl3 INTERFACE
    ${RMLUI_BACKEND_PATH}/RmlUi_Platform_SDL.cpp
    ${RMLUI_BACKEND_PATH}/RmlUi_Renderer_SDL_GPU.cpp
)
target_include_directories(rmlui_backend_sdl_gl3 INTERFACE ${RMLUI_BACKEND_PATH})
target_compile_definitions(rmlui_backend_sdl_gl3 INTERFACE
    RMLUI_SDL_VERSION_MAJOR=3
    RMLUI_NUM_MSAA_SAMPLES=4
)

CPMAddPackage(
    NAME JoltPhysics
    GITHUB_REPOSITORY jrouwe/JoltPhysics
    GIT_TAG v5.4.0
    SOURCE_SUBDIR Build
    OPTIONS
        "CPP_RTTI_ENABLED"
        # JoltPhysics defaults INTERPROCEDURAL_OPTIMIZATION to ON, which
        # builds libJolt.a as LLVM bitcode. The system linker can only
        # consume that when the consumer also enables LTO, which the
        # engine does not. Without this override the build fails with:
        #   /usr/bin/ld: libJolt.a: error adding symbols: file format
        #   not recognized
        "INTERPROCEDURAL_OPTIMIZATION OFF"
    SYSTEM ON
)
target_compile_definitions(Jolt PUBLIC JPH_DEBUG_RENDERER CPP_RTTI_ENABLED)

# ai-sdk-cpp is fetched manually so we can patch the missing natvis file
# before add_subdirectory runs.  CPM's PATCH_COMMAND is not reliable here.
include(FetchContent)
FetchContent_Declare(
    ai-sdk-cpp
    GIT_REPOSITORY https://github.com/ClickHouse/ai-sdk-cpp.git
    GIT_TAG main
    GIT_SUBMODULES_RECURSE YES
)
FetchContent_GetProperties(ai-sdk-cpp)
if(NOT ai-sdk-cpp_POPULATED)
    FetchContent_Populate(ai-sdk-cpp)
    # MSVC: nlohmann_json_patched expects a natvis file that doesn't exist.
    set(_AI_SDK_NATVIS "${ai-sdk-cpp_SOURCE_DIR}/third_party/nlohmann_json_patched/nlohmann_json.natvis")
    if(NOT EXISTS "${_AI_SDK_NATVIS}")
        file(TOUCH "${_AI_SDK_NATVIS}")
    endif()
endif()
add_subdirectory(${ai-sdk-cpp_SOURCE_DIR} ${ai-sdk-cpp_BINARY_DIR} EXCLUDE_FROM_ALL SYSTEM)

# A2A protocol: simdjson for JSON parsing
find_package(simdjson REQUIRED)

# A2A protocol: libcurl for HTTP transport
find_package(CURL REQUIRED)

# --- Asset Compilation ---
set(SHADER_SOURCE_DIR "${CMAKE_SOURCE_DIR}/rsc/shaders")

# Find slangc executable robustly across platforms.
if(WIN32)
    set(_SLANGC_NAME "slangc.exe")
else()
    set(_SLANGC_NAME "slangc")
endif()

# Priority 1: prebuilt slangc downloaded by CPM (slang-* directories)
set(SLANG_EXE "")
file(GLOB _SLANG_PREBUILT_CANDIDATES LIST_DIRECTORIES true "${CMAKE_BINARY_DIR}/_deps/slang-build/slang-*")
foreach(_dir IN LISTS _SLANG_PREBUILT_CANDIDATES)
    if(IS_DIRECTORY "${_dir}")
        set(_candidate "${_dir}/bin/${_SLANGC_NAME}")
        if(EXISTS "${_candidate}")
            set(SLANG_EXE "${_candidate}")
            break()
        endif()
    endif()
endforeach()

# Priority 2: find_program fallback in known build directories
if(NOT SLANG_EXE)
    find_program(SLANG_EXE slangc
        PATHS
            "${CMAKE_BINARY_DIR}/_deps/slang-build/bin"
            "${CMAKE_BINARY_DIR}/_deps/slang-build/source/slangc"
            "${CMAKE_BINARY_DIR}/_deps/slang-build/slangc"
    )
endif()

# Priority 3: last resort – search in PATH / system directories
if(NOT SLANG_EXE)
    find_program(SLANG_EXE slangc)
endif()

# If slangc is built from source as a target, add a dependency on it but
# keep using the prebuilt slangc for shader compilation (it's known to work
# with -dxc-path; the freshly built executable has path-resolution issues).
set(SLANGC_IS_TARGET FALSE)
if(TARGET slangc)
    set(SLANGC_IS_TARGET TRUE)
endif()

if(NOT SLANG_EXE)
    message(FATAL_ERROR "Could not find slangc executable or target. "
        "Set SLANG_EXE manually or ensure slang is built.")
endif()

message(STATUS "Using slangc: ${SLANG_EXE}")

set(VERTEX_SHADER_SOURCES
    "${SHADER_SOURCE_DIR}/cube.vert.slang"
    "${SHADER_SOURCE_DIR}/flat.vert.slang"
    "${SHADER_SOURCE_DIR}/ui.vert.slang"
    "${SHADER_SOURCE_DIR}/fullscreen.vert.slang"
    "${SHADER_SOURCE_DIR}/skybox.vert.slang"
    "${SHADER_SOURCE_DIR}/shadow_depth.vert.slang"
    "${SHADER_SOURCE_DIR}/point_shadow.vert.slang"
    "${SHADER_SOURCE_DIR}/ssao_prepass.vert.slang"
    "${SHADER_SOURCE_DIR}/outline.vert.slang"
    "${SHADER_SOURCE_DIR}/grid.vert.slang"
    "${SHADER_SOURCE_DIR}/sprite_2d.vert.slang"
)

set(FRAGMENT_SHADER_SOURCES
    "${SHADER_SOURCE_DIR}/cube.frag.slang"
    "${SHADER_SOURCE_DIR}/unlit.frag.slang"
    "${SHADER_SOURCE_DIR}/preview_bg.frag.slang"
    "${SHADER_SOURCE_DIR}/flat.frag.slang"
    "${SHADER_SOURCE_DIR}/ui.frag.slang"
    "${SHADER_SOURCE_DIR}/ui_texture.frag.slang"
    "${SHADER_SOURCE_DIR}/sprite_2d.frag.slang"
    "${SHADER_SOURCE_DIR}/skybox.frag.slang"
    "${SHADER_SOURCE_DIR}/procedural_skybox.frag.slang"
    "${SHADER_SOURCE_DIR}/brdf_lut.frag.slang"
    "${SHADER_SOURCE_DIR}/ibl_prefilter.frag.slang"
    "${SHADER_SOURCE_DIR}/ibl_irradiance.frag.slang"
    "${SHADER_SOURCE_DIR}/bloom_blur.frag.slang"
    "${SHADER_SOURCE_DIR}/bloom_downsample.frag.slang"
    "${SHADER_SOURCE_DIR}/composite_tonemap.frag.slang"
    "${SHADER_SOURCE_DIR}/shadow_depth.frag.slang"
    "${SHADER_SOURCE_DIR}/point_shadow.frag.slang"
    "${SHADER_SOURCE_DIR}/ssao_prepass.frag.slang"
    "${SHADER_SOURCE_DIR}/ssao.frag.slang"
    "${SHADER_SOURCE_DIR}/ssao_blur.frag.slang"
    "${SHADER_SOURCE_DIR}/outline.frag.slang"
    "${SHADER_SOURCE_DIR}/equirect_to_cube.frag.slang"
    "${SHADER_SOURCE_DIR}/grid.frag.slang"
)

set(COMPUTE_SHADER_SOURCES
    "${SHADER_SOURCE_DIR}/cluster_build.slang"
    "${SHADER_SOURCE_DIR}/light_cull.slang"
)

set(SHADER_OUTPUT_DIR "${CMAKE_BINARY_DIR}/compiled_shaders")
file(MAKE_DIRECTORY "${SHADER_OUTPUT_DIR}")

# Copy the shared PBR include module next to the compiled shaders so the
# runtime shader-graph codegen can inline it (cube.frag.slang includes it at
# build time from the same source directory).
add_custom_command(
    OUTPUT "${SHADER_OUTPUT_DIR}/pbr_common.slang"
    COMMAND ${CMAKE_COMMAND} -E copy
            "${SHADER_SOURCE_DIR}/pbr_common.slang"
            "${SHADER_OUTPUT_DIR}/pbr_common.slang"
    DEPENDS "${SHADER_SOURCE_DIR}/pbr_common.slang"
    COMMENT "Copying pbr_common.slang"
    VERBATIM
)
list(APPEND COMPILED_SHADERS "${SHADER_OUTPUT_DIR}/pbr_common.slang")

if(WIN32)
    set(SLANG_SHADER_TARGET dxil)
    set(SHADER_EXT ".dxil")
    # DXC 1.9 rejects Slang-generated DXIL with validation enabled for some targets.
    set(SLANG_DXC_FLAGS "-Xdxc" "-Vd")
elseif(APPLE)
    set(SLANG_SHADER_TARGET metal)
    set(SHADER_EXT ".metal")
    set(SLANG_DXC_FLAGS "")
else()
    set(SLANG_SHADER_TARGET spirv)
    set(SHADER_EXT ".spv")
    set(SLANG_DXC_FLAGS "")
    # Slang's direct SPIR-V emitter produces generic SPIR-V 1.5 that
    # pulls in Vulkan 1.1+ capabilities (DrawParameters,
    # SPV_KHR_storage_buffer_storage_class) without declaring a
    # matching target environment. SDL_GPU's Vulkan backend then
    # reports the device as Vulkan 1.0 to the validation layer, the
    # validator rejects the SPIR-V, and the AMDVK driver ends up
    # parsing the rest as Vulkan 1.0 — the fragment-stage storage
    # buffer descriptor comes back as `VkBuffer 0x0`
    # (validation: "buffer is VK_NULL_HANDLE ... invalid or has been
    # destroyed") and the next `SDL_DispatchGPUCompute` segfaults.
    # Routing through glslang (which is on PATH) emits Vulkan 1.0
    # compatible SPIR-V by default and the driver parses it correctly.
    set(SLANG_SPIRV_FLAGS "-emit-spirv-via-glsl" "-fvk-use-entrypoint-name")
endif()

if(WIN32)
    set(SLANG_VERT_PROFILE "-profile" "vs_6_0")
    set(SLANG_FRAG_PROFILE "-profile" "ps_6_0")
else()
    set(SLANG_VERT_PROFILE "")
    set(SLANG_FRAG_PROFILE "")
endif()

set(COMPILED_SHADERS "")
foreach(SHADER_FILE IN LISTS VERTEX_SHADER_SOURCES)
    get_filename_component(SHADER_BASE "${SHADER_FILE}" NAME_WE)
    set(OUTPUT_FILE "${SHADER_OUTPUT_DIR}/${SHADER_BASE}.vert.slang${SHADER_EXT}")
    add_custom_command(
        OUTPUT "${OUTPUT_FILE}"
        COMMAND "${SLANG_EXE}" "${SHADER_FILE}" -target ${SLANG_SHADER_TARGET} ${SLANG_VERT_PROFILE} -entry vsMain -o "${OUTPUT_FILE}" -fvk-t-shift 0 0 -fvk-s-shift 0 0 -fvk-b-shift 0 1 ${SLANG_DXC_FLAGS} ${SLANG_SPIRV_FLAGS}
        DEPENDS "${SHADER_FILE}"
        COMMENT "Compiling shader: ${SHADER_FILE}"
        VERBATIM
    )
    list(APPEND COMPILED_SHADERS "${OUTPUT_FILE}")
endforeach()

foreach(SHADER_FILE IN LISTS FRAGMENT_SHADER_SOURCES)
    get_filename_component(SHADER_BASE "${SHADER_FILE}" NAME_WE)
    set(OUTPUT_FILE "${SHADER_OUTPUT_DIR}/${SHADER_BASE}.frag.slang${SHADER_EXT}")
    add_custom_command(
        OUTPUT "${OUTPUT_FILE}"
        COMMAND "${SLANG_EXE}" "${SHADER_FILE}" -target ${SLANG_SHADER_TARGET} ${SLANG_FRAG_PROFILE} -entry fsMain -o "${OUTPUT_FILE}" -fvk-t-shift 0 2 -fvk-s-shift 0 2 -fvk-b-shift 0 3 -fvk-u-shift 0 2 ${SLANG_DXC_FLAGS} ${SLANG_SPIRV_FLAGS}
        DEPENDS "${SHADER_FILE}"
        COMMENT "Compiling shader: ${SHADER_FILE}"
        VERBATIM
    )
    list(APPEND COMPILED_SHADERS "${OUTPUT_FILE}")
endforeach()

foreach(SHADER_FILE IN LISTS COMPUTE_SHADER_SOURCES)
    get_filename_component(SHADER_BASE "${SHADER_FILE}" NAME_WE)
    set(OUTPUT_FILE "${SHADER_OUTPUT_DIR}/${SHADER_BASE}.slang${SHADER_EXT}")
    add_custom_command(
        OUTPUT "${OUTPUT_FILE}"
        # Compute shaders use explicit [[vk::binding(set, binding)]]
        # attributes for all resources. The -fvk-*-shift flags would
        # override those bindings, so they are intentionally omitted.
        COMMAND "${SLANG_EXE}" "${SHADER_FILE}" -target ${SLANG_SHADER_TARGET} -entry csMain -o "${OUTPUT_FILE}" ${SLANG_DXC_FLAGS} ${SLANG_SPIRV_FLAGS}
        DEPENDS "${SHADER_FILE}"
        COMMENT "Compiling compute shader: ${SHADER_FILE}"
        VERBATIM
    )
    list(APPEND COMPILED_SHADERS "${OUTPUT_FILE}")
endforeach()

add_custom_target(compile_shaders ALL DEPENDS ${COMPILED_SHADERS} "${SHADER_OUTPUT_DIR}/pbr_common.slang")
if(SLANGC_IS_TARGET)
    add_dependencies(compile_shaders slangc)
endif()

# --- Asset Copying ---
macro(add_copy_target TARGET_NAME SOURCE_FILES DEST_DIR COMMENT_MSG)
    file(MAKE_DIRECTORY "${DEST_DIR}")
    add_custom_target(${TARGET_NAME} ALL)
    foreach(SRC_FILE IN LISTS ${SOURCE_FILES})
        get_filename_component(FILE_NAME "${SRC_FILE}" NAME)
        set(DEST_FILE "${DEST_DIR}/${FILE_NAME}")
        add_custom_command(
            TARGET ${TARGET_NAME}
            POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different "${SRC_FILE}" "${DEST_FILE}"
            COMMENT "${COMMENT_MSG}: ${FILE_NAME}"
        )
    endforeach()
endmacro()

file(GLOB OTF_SOURCES "${CMAKE_SOURCE_DIR}/rsc/otf/*.otf")
add_copy_target(copy_otf OTF_SOURCES "${CMAKE_BINARY_DIR}/otf" "Copying OTF")

file(GLOB ICON_SVG_SOURCES "${CMAKE_SOURCE_DIR}/rsc/icons/*.svg")
add_copy_target(copy_icons ICON_SVG_SOURCES "${CMAKE_BINARY_DIR}/icons" "Copying SVG icon")

# --- ImGuiColorTextEdit integration ---
if(DEFINED ImGuiColorTextEdit_SOURCE_DIR)
    file(COPY
        "${CMAKE_SOURCE_DIR}/cmake/imguicolortextedit/CMakeLists.txt"
        DESTINATION "${ImGuiColorTextEdit_SOURCE_DIR}")
    add_subdirectory(${ImGuiColorTextEdit_SOURCE_DIR} ${ImGuiColorTextEdit_BINARY_DIR} EXCLUDE_FROM_ALL SYSTEM)
endif()

# --- Subprojects ---
add_subdirectory(src/wsl)
add_subdirectory(src/cli)
add_subdirectory(src/editor)
add_subdirectory(src/mcp-server)

# --- stb integration (needs wsl target) ---
if(DEFINED stb_SOURCE_DIR)
    file(COPY
        "${CMAKE_SOURCE_DIR}/cmake/stb/CMakeLists.txt"
        "${CMAKE_SOURCE_DIR}/cmake/stb/stb_image_impl.c"
        DESTINATION "${stb_SOURCE_DIR}")
    add_subdirectory(${stb_SOURCE_DIR} ${stb_BINARY_DIR} EXCLUDE_FROM_ALL SYSTEM)
    if(TARGET stb::stb_image)
        target_link_libraries(wsl PRIVATE stb::stb_image)
    endif()
endif()

# --- Documentation ---
# Unified Sphinx build: daslang API (auto-generated) + C++ API (via hawkmoth)
find_package(Python3 COMPONENTS Interpreter)
if(Python3_FOUND)
    add_custom_target(docs
        COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/doc/source/das_api_gen.py
            --source ${CMAKE_CURRENT_SOURCE_DIR}/src
            --output ${CMAKE_CURRENT_SOURCE_DIR}/doc/source/stdlib
        COMMAND sphinx-build --keep-going -b html
            ${CMAKE_CURRENT_SOURCE_DIR}/doc/source
            ${CMAKE_CURRENT_BINARY_DIR}/docs
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/doc
        COMMENT "Generating documentation (Sphinx + hawkmoth + das_api_gen)"
        VERBATIM
    )
endif()


# --- Testing ---
enable_testing()
# Use doctest for weasel-cli unit tests (tests/weasel-cli/*.cpp)
include(FetchContent)
FetchContent_Declare(
  doctest
  GIT_REPOSITORY https://github.com/onqtam/doctest.git
  GIT_TAG master
)
FetchContent_GetProperties(doctest)
if(NOT doctest_POPULATED)
  FetchContent_Populate(doctest)
endif()

# Create an interface target for doctest headers (if not already defined by daslang)
if(NOT TARGET doctest)
  add_library(doctest INTERFACE)
  target_include_directories(doctest INTERFACE ${doctest_SOURCE_DIR})
endif()

# Add doctest-based tests for the CLI handler
add_executable(weasel_cli_tests
    "${CMAKE_SOURCE_DIR}/tests/weasel-cli/test_weasel_cli.cpp"
    "${CMAKE_SOURCE_DIR}/tests/weasel-cli/test_command_executor.cpp"
)
target_link_libraries(weasel_cli_tests PRIVATE cli doctest)

include(${doctest_SOURCE_DIR}/scripts/cmake/doctest.cmake)
doctest_discover_tests(weasel_cli_tests)

# Add doctest-based tests for the MCP server
add_executable(weasel_mcp_server_tests
    "${CMAKE_SOURCE_DIR}/tests/mcp-server/test_mcp_server.cpp"
)
target_link_libraries(weasel_mcp_server_tests PRIVATE mcp_server_lib doctest)
doctest_discover_tests(weasel_mcp_server_tests)

# --- Installation ---
# Only install our own targets/resources.  CPM dependencies (slang, ai-sdk-cpp,
# etc.) have their own install() rules that break in CI (wrong paths, man pages,
# missing files).  Tagging installs with COMPONENT Engine/CLI/McpServer and
# limiting CPACK_COMPONENTS_ALL prevents CPack from touching them.
#
# On Apple the editor is packaged as a proper .app bundle under /Applications/.
# The dylibs (libwsl, libTracyClient) and the runtime resources
# (compiled_shaders, otf, icons) are nested inside the bundle. CLI tools
# (weasel-cli, weasel-mcp-server) are still installed as plain binaries.
if(APPLE)
    # The .app bundle is the install destination. CMake's MACOSX_BUNDLE
    # install places the whole bundle at <prefix>/Applications/Weasel.app/,
    # so with the productbuild install prefix of `/` this resolves to
    # /Applications/Weasel.app/. The dylibs and resources are installed
    # at the matching bundle-relative paths so they end up inside the
    # bundle.
    install(TARGETS weasel MACOSX_BUNDLE
        DESTINATION Applications
        COMPONENT Engine
    )
    install(TARGETS wsl
        LIBRARY DESTINATION Applications/Weasel.app/Contents/Frameworks
        COMPONENT Engine
    )
    install(TARGETS weasel-cli
        RUNTIME DESTINATION bin
        COMPONENT CLI
    )
    install(TARGETS weasel-mcp-server
        RUNTIME DESTINATION bin
        COMPONENT McpServer
    )
else()
    install(TARGETS wsl weasel
        RUNTIME DESTINATION bin COMPONENT Engine
        LIBRARY DESTINATION lib COMPONENT Engine
        ARCHIVE DESTINATION lib COMPONENT Engine
    )
    install(TARGETS weasel-cli
        RUNTIME DESTINATION bin COMPONENT CLI
    )
    install(TARGETS weasel-mcp-server
        RUNTIME DESTINATION bin COMPONENT McpServer
    )
endif()

# Tracy must ship with the engine: it's a SHARED library and weasel/wsl
# dynamically link against libTracyClient.so. Without it, `weasel` and
# `libwsl.so` would each carry their own Profiler globals and the
# `s_instance` assertion would fire on the second construction.
if(TARGET TracyClient)
    if(APPLE)
        install(TARGETS TracyClient
            RUNTIME DESTINATION Applications/Weasel.app/Contents/Frameworks
            LIBRARY DESTINATION Applications/Weasel.app/Contents/Frameworks
            COMPONENT Engine
        )
    else()
        install(TARGETS TracyClient
            RUNTIME DESTINATION bin COMPONENT Engine
            LIBRARY DESTINATION lib COMPONENT Engine
            ARCHIVE DESTINATION lib COMPONENT Engine
            COMPONENT Engine
        )
    endif()
endif()

# Generate a manual WeaselTargets.cmake with an imported target for wsl.
# This avoids CMake export issues with wsl's many CPM-based PUBLIC dependencies.
set(_WEASEL_IMPORTED_DEFS "JPH_DEBUG_RENDERER;CPP_RTTI_ENABLED;RMLUI_SDL_VERSION_MAJOR=3;RMLUI_NUM_MSAA_SAMPLES=4")
if(WEASEL_ENABLE_MULTIPLAYER)
    set(_WEASEL_IMPORTED_DEFS "${_WEASEL_IMPORTED_DEFS};WEASEL_ENABLE_MULTIPLAYER")
endif()

if(WIN32)
    set(_WSL_IMPORTED_LOCATION "wsl.dll")
    set(_WSL_IMPORTED_IMPLIB "wsl.lib")
elseif(APPLE)
    set(_WSL_IMPORTED_LOCATION "libwsl.dylib")
else()
    set(_WSL_IMPORTED_LOCATION "libwsl.so")
endif()

file(WRITE "${CMAKE_BINARY_DIR}/WeaselTargets.cmake" "# Auto-generated WeaselTargets.cmake
get_filename_component(_IMPORT_PREFIX \"\${CMAKE_CURRENT_LIST_DIR}/../../..\" ABSOLUTE)

add_library(Weasel::wsl SHARED IMPORTED)
set_target_properties(Weasel::wsl PROPERTIES
    IMPORTED_LOCATION \"\${_IMPORT_PREFIX}/lib/${_WSL_IMPORTED_LOCATION}\"")

if(WIN32)
    file(APPEND "${CMAKE_BINARY_DIR}/WeaselTargets.cmake" "
    IMPORTED_IMPLIB \"\${_IMPORT_PREFIX}/lib/${_WSL_IMPORTED_IMPLIB}\"")
endif()

file(APPEND "${CMAKE_BINARY_DIR}/WeaselTargets.cmake" "
    INTERFACE_INCLUDE_DIRECTORIES \"\${_IMPORT_PREFIX}/include\"")

if(_WEASEL_IMPORTED_DEFS)
    file(APPEND "${CMAKE_BINARY_DIR}/WeaselTargets.cmake" "
    INTERFACE_COMPILE_DEFINITIONS \"${_WEASEL_IMPORTED_DEFS}\"")
endif()

# Collect shared library dependencies that consumers must be able to find
# at link-time. Static libraries (daScript, imgui, etc.) are embedded in
# libwsl.so and do not need to be listed here.
set(_WSL_LINK_LIBS "")
if(TARGET SDL3::SDL3)
    get_target_property(_SDL3_loc SDL3::SDL3 IMPORTED_LOCATION)
    if(_SDL3_loc)
        list(APPEND _WSL_LINK_LIBS "${_SDL3_loc}")
    endif()
endif()
if(TARGET TracyClient)
    get_target_property(_TRACY_loc TracyClient IMPORTED_LOCATION)
    if(_TRACY_loc)
        list(APPEND _WSL_LINK_LIBS "${_TRACY_loc}")
    endif()
endif()
if(_WSL_LINK_LIBS)
    list(REMOVE_DUPLICATES _WSL_LINK_LIBS)
    file(APPEND "${CMAKE_BINARY_DIR}/WeaselTargets.cmake" "
    INTERFACE_LINK_LIBRARIES \"${_WSL_LINK_LIBS}\"")
endif()

file(APPEND "${CMAKE_BINARY_DIR}/WeaselTargets.cmake" "
)
add_library(wsl ALIAS Weasel::wsl)
")

install(FILES "${CMAKE_BINARY_DIR}/WeaselTargets.cmake"
    DESTINATION lib/cmake/Weasel
    COMPONENT Engine
)

install(FILES WeaselConfig.cmake
    DESTINATION lib/cmake/Weasel
    COMPONENT Engine
)

install(FILES cmake/WeaselDasAOT.cmake
    DESTINATION lib/cmake/Weasel
    COMPONENT Engine
)

# Install weasel_api.das declarations for AOT compilation.
# Downstream projects need this file in their daslib path.
# Install to share/weasel/ so packaging/linux/Makefile picks it up.
install(FILES src/wsl/das/modules/weasel_api/weasel_api.das
    DESTINATION share/weasel/daslib
    COMPONENT Engine
)
install(FILES src/wsl/das/modules/weasel_helpers.das
    DESTINATION share/weasel/daslib
    COMPONENT Engine
)
install(FILES src/wsl/das/modules/weasel_ecs.das
    DESTINATION share/weasel/daslib
    COMPONENT Engine
)

# Copy weasel_ecs.das to the build daslib directory for runtime module resolution.
# The engine's setDasRoot points to ${CMAKE_BINARY_DIR}/_deps/daslang-src, so
# scripts that `require weasel_ecs` can find it at compile/build time.
add_custom_target(copy_weasel_ecs ALL
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CMAKE_SOURCE_DIR}/src/wsl/das/modules/weasel_ecs.das"
            "${CMAKE_BINARY_DIR}/_deps/daslang-src/daslib/weasel_ecs.das"
    COMMENT "Copying weasel_ecs.das to build daslib"
)

# Copy weasel_helpers.das to the build daslib directory for runtime module resolution.
add_custom_target(copy_weasel_helpers ALL
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CMAKE_SOURCE_DIR}/src/wsl/das/modules/weasel_helpers.das"
            "${CMAKE_BINARY_DIR}/_deps/daslang-src/daslib/weasel_helpers.das"
    COMMENT "Copying weasel_helpers.das to build daslib"
)

set(_WEASEL_HEADER_PATTERNS PATTERN "*.h" PATTERN "*.hpp" PATTERN "*.inl")

install(DIRECTORY src/
    DESTINATION include
    COMPONENT Engine
    FILES_MATCHING ${_WEASEL_HEADER_PATTERNS}
)

# Install dependency headers needed by wsl public API
if(DEFINED EnTT_SOURCE_DIR)
    install(DIRECTORY ${EnTT_SOURCE_DIR}/src/
        DESTINATION include
        COMPONENT Engine
        FILES_MATCHING ${_WEASEL_HEADER_PATTERNS}
    )
endif()

if(DEFINED cereal_SOURCE_DIR)
    install(DIRECTORY ${cereal_SOURCE_DIR}/include/
        DESTINATION include
        COMPONENT Engine
        FILES_MATCHING ${_WEASEL_HEADER_PATTERNS}
    )
endif()

if(DEFINED JoltPhysics_SOURCE_DIR)
    install(DIRECTORY ${JoltPhysics_SOURCE_DIR}/
        DESTINATION include
        COMPONENT Engine
        FILES_MATCHING ${_WEASEL_HEADER_PATTERNS}
    )
endif()

if(DEFINED fastgltf_SOURCE_DIR)
    install(DIRECTORY ${fastgltf_SOURCE_DIR}/include/
        DESTINATION include
        COMPONENT Engine
        FILES_MATCHING ${_WEASEL_HEADER_PATTERNS}
    )
endif()

if(DEFINED spdlog_SOURCE_DIR)
    install(DIRECTORY ${spdlog_SOURCE_DIR}/include/
        DESTINATION include
        COMPONENT Engine
        FILES_MATCHING ${_WEASEL_HEADER_PATTERNS}
    )
endif()

if(DEFINED RmlUi_SOURCE_DIR)
    install(DIRECTORY ${RmlUi_SOURCE_DIR}/Include/
        DESTINATION include
        COMPONENT Engine
        FILES_MATCHING ${_WEASEL_HEADER_PATTERNS}
    )
    install(DIRECTORY ${RmlUi_SOURCE_DIR}/Backends/
        DESTINATION include
        COMPONENT Engine
        FILES_MATCHING PATTERN "RmlUi_*.h"
    )
endif()

if(DEFINED slang_SOURCE_DIR)
    install(DIRECTORY ${slang_SOURCE_DIR}/include/
        DESTINATION include
        COMPONENT Engine
        FILES_MATCHING ${_WEASEL_HEADER_PATTERNS}
    )
endif()

# Tracy: the engine's public headers (wsl/sys/system.hpp) use
# ZoneScopedN / ZoneText / FrameMark, so consumers must be able to
# #include <tracy/Tracy.hpp>. Tracy.hpp itself pulls in siblings from
# the sibling `common/` and `client/` directories via `#include
# "../common/..."` / `#include "../client/..."`, so those have to be
# installed at matching relative paths under <prefix>/include/ as well.
# Without these, any project that links wsl fails to find
# <tracy/Tracy.hpp> at compile time.
if(DEFINED Tracy_SOURCE_DIR)
    install(DIRECTORY ${Tracy_SOURCE_DIR}/public/tracy/
        DESTINATION include/tracy
        COMPONENT Engine
        FILES_MATCHING ${_WEASEL_HEADER_PATTERNS}
    )
    install(DIRECTORY ${Tracy_SOURCE_DIR}/public/common/
        DESTINATION include/common
        COMPONENT Engine
        FILES_MATCHING ${_WEASEL_HEADER_PATTERNS}
    )
    install(DIRECTORY ${Tracy_SOURCE_DIR}/public/client/
        DESTINATION include/client
        COMPONENT Engine
        FILES_MATCHING ${_WEASEL_HEADER_PATTERNS}
    )
endif()

if(DEFINED SDL3_image_SOURCE_DIR)
    install(DIRECTORY ${SDL3_image_SOURCE_DIR}/include/
        DESTINATION include
        COMPONENT Engine
        FILES_MATCHING PATTERN "*.h"
    )
endif()

if(DEFINED SDL3_mixer_SOURCE_DIR)
    install(DIRECTORY ${SDL3_mixer_SOURCE_DIR}/include/
        DESTINATION include
        COMPONENT Engine
        FILES_MATCHING PATTERN "*.h"
    )
endif()

if(DEFINED imgui_SOURCE_DIR)
    install(DIRECTORY ${imgui_SOURCE_DIR}/
        DESTINATION include
        COMPONENT Engine
        FILES_MATCHING PATTERN "imgui*.h" PATTERN "imconfig.h"
    )
endif()

if(DEFINED cereal_SOURCE_DIR)
    install(DIRECTORY ${cereal_SOURCE_DIR}/include/
        DESTINATION include
        COMPONENT Engine
        FILES_MATCHING PATTERN "*.hpp" PATTERN "*.h"
    )
endif()

if(DEFINED JoltPhysics_SOURCE_DIR)
    install(DIRECTORY ${JoltPhysics_SOURCE_DIR}/
        DESTINATION include
        COMPONENT Engine
        FILES_MATCHING PATTERN "*.h" PATTERN "*.hpp"
    )
endif()

if(DEFINED fastgltf_SOURCE_DIR)
    install(DIRECTORY ${fastgltf_SOURCE_DIR}/include/
        DESTINATION include
        COMPONENT Engine
        FILES_MATCHING PATTERN "*.hpp" PATTERN "*.h"
    )
endif()

if(DEFINED spdlog_SOURCE_DIR)
    install(DIRECTORY ${spdlog_SOURCE_DIR}/include/
        DESTINATION include
        COMPONENT Engine
        FILES_MATCHING PATTERN "*.hpp" PATTERN "*.h"
    )
endif()

if(DEFINED RmlUi_SOURCE_DIR)
    install(DIRECTORY ${RmlUi_SOURCE_DIR}/Include/
        DESTINATION include
        COMPONENT Engine
        FILES_MATCHING PATTERN "*.h" PATTERN "*.hpp" PATTERN "*.inl"
    )
endif()

if(DEFINED slang_SOURCE_DIR)
    install(DIRECTORY ${slang_SOURCE_DIR}/include/
        DESTINATION include
        COMPONENT Engine
        FILES_MATCHING PATTERN "*.h" PATTERN "*.hpp"
    )
endif()

if(DEFINED imgui_SOURCE_DIR)
    install(DIRECTORY ${imgui_SOURCE_DIR}/
        DESTINATION include
        COMPONENT Engine
        FILES_MATCHING PATTERN "imgui*.h" PATTERN "imconfig.h"
    )
endif()

if(DEFINED SDL3_image_SOURCE_DIR)
    install(DIRECTORY ${SDL3_image_SOURCE_DIR}/include/
        DESTINATION include
        COMPONENT Engine
        FILES_MATCHING PATTERN "*.h"
    )
endif()

if(DEFINED SDL3_mixer_SOURCE_DIR)
    install(DIRECTORY ${SDL3_mixer_SOURCE_DIR}/include/
        DESTINATION include
        COMPONENT Engine
        FILES_MATCHING PATTERN "*.h"
    )
endif()

install(DIRECTORY ${CMAKE_BINARY_DIR}/compiled_shaders DESTINATION share/weasel COMPONENT Engine)
install(DIRECTORY ${CMAKE_BINARY_DIR}/otf DESTINATION share/weasel COMPONENT Engine)
install(DIRECTORY ${CMAKE_BINARY_DIR}/icons DESTINATION share/weasel COMPONENT Engine)

if(APPLE)
    # Nest the runtime resource directories inside the .app bundle so
    # the editor's main.cpp resource lookup (exe/../Resources/share/weasel)
    # works after the .pkg installer places Weasel.app in /Applications/.
    install(DIRECTORY ${CMAKE_BINARY_DIR}/compiled_shaders
        DESTINATION Applications/Weasel.app/Contents/Resources/share/weasel
        COMPONENT Engine
    )
    install(DIRECTORY ${CMAKE_BINARY_DIR}/otf
        DESTINATION Applications/Weasel.app/Contents/Resources/share/weasel
        COMPONENT Engine
    )
    install(DIRECTORY ${CMAKE_BINARY_DIR}/icons
        DESTINATION Applications/Weasel.app/Contents/Resources/share/weasel
        COMPONENT Engine
    )
endif()

if(UNIX AND NOT APPLE)
    install(FILES "${CMAKE_SOURCE_DIR}/packaging/linux/Makefile" DESTINATION . COMPONENT Engine)
endif()

set(CPACK_PACKAGE_NAME "WeaselEngine")
set(CPACK_PACKAGE_VERSION "0.1.0")
set(CPACK_PACKAGE_VENDOR "Weasel Engine")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Weasel Engine — a game engine")
set(CPACK_COMPONENTS_ALL Engine CLI McpServer)
set(CPACK_COMPONENT_ENGINE_DISPLAY_NAME "Weasel Engine")
set(CPACK_COMPONENT_ENGINE_DESCRIPTION "Weasel Engine core library, editor, headers and resources")
set(CPACK_COMPONENT_CLI_DISPLAY_NAME "weasel-cli")
set(CPACK_COMPONENT_CLI_DESCRIPTION "Weasel Engine command-line interface")
set(CPACK_COMPONENT_CLI_DEPENDS Engine)
set(CPACK_COMPONENT_MCPSERVER_DISPLAY_NAME "weasel-mcp-server")
set(CPACK_COMPONENT_MCPSERVER_DESCRIPTION "Weasel Engine MCP server")
set(CPACK_COMPONENT_MCPSERVER_DEPENDS Engine)

# Override CPack's install step so it only installs our own components.
# Without this CPack runs a full "cmake --install ." which also executes
# broken install() rules from CPM dependencies (slang, ai-sdk-cpp, brotli...).
set(CPACK_INSTALL_CMAKE_PROJECTS
    "${CMAKE_BINARY_DIR};weasel;Engine;/"
    "${CMAKE_BINARY_DIR};weasel;CLI;/"
    "${CMAKE_BINARY_DIR};weasel;McpServer;/"
)

if(WIN32)
    set(CPACK_GENERATOR "NSIS;ZIP" CACHE STRING "CPack generator" FORCE)
    set(CPACK_NSIS_DISPLAY_NAME "Weasel Engine")
    set(CPACK_NSIS_HELP_LINK "https://github.com/Mario1159/weasel")
    set(CPACK_NSIS_URL_INFO_ABOUT "https://github.com/Mario1159/weasel")
elseif(APPLE)
    set(CPACK_GENERATOR "productbuild;TGZ" CACHE STRING "CPack generator" FORCE)
    set(CPACK_PRODUCTBUILD_IDENTIFIER "com.weaselengine")
    # The .pkg installs the .app bundle into /Applications/ and the CLI
    # tools into /usr/local/bin/, both of which are inside the system
    # root, so the product must be installable into "/". Without these
    # productbuild either errors out or refuses to lay files into
    # /Applications.
    set(CPACK_PRODUCTBUILD_DEFAULT_INSTALL_PREFIX "/")
    set(CPACK_PRODUCTBUILD_PACKAGE_LOCATION "/")
else()
    # DEB/RPM omitted: they require distro-specific tooling (dpkg, rpmbuild) and
    # packaging metadata that is not available in CI containers.
    set(CPACK_GENERATOR "TGZ" CACHE STRING "CPack generator" FORCE)
endif()

set(CPACK_PROJECT_CONFIG_FILE "${CMAKE_SOURCE_DIR}/cmake/CPackProjectConfig.cmake")

include(CPack)
