cmake_minimum_required(VERSION 3.22)
project(weasel LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
option(WEASEL_DISCONNECTED_DEPENDENCY_UPDATES
    "Skip CPM/FetchContent git update steps for already-populated dependencies"
    ON)
option(WEASEL_LIMIT_NINJA_RESOURCES
    "Limit Ninja compile/link parallelism to keep the editor responsive"
    ON)
option(WEASEL_ENABLE_MULTIPLAYER
    "Enable multiplayer support via GameNetworkingSockets"
    ON)

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
find_package(SDL3 CONFIG REQUIRED)
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
include(cmake/ClangRepl.cmake)

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
    NAME slang
    GITHUB_REPOSITORY shader-slang/slang
    GIT_TAG v2026.8.1
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
    OPTIONS "BUILD_SANDBOX OFF"
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

if(WEASEL_ENABLE_MULTIPLAYER)
    CPMAddPackage(
        NAME GameNetworkingSockets
        GITHUB_REPOSITORY ValveSoftware/GameNetworkingSockets
        GIT_TAG master
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
    OPTIONS "CPP_RTTI_ENABLED"
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
)

set(FRAGMENT_SHADER_SOURCES
    "${SHADER_SOURCE_DIR}/cube.frag.slang"
    "${SHADER_SOURCE_DIR}/unlit.frag.slang"
    "${SHADER_SOURCE_DIR}/preview_bg.frag.slang"
    "${SHADER_SOURCE_DIR}/flat.frag.slang"
    "${SHADER_SOURCE_DIR}/ui.frag.slang"
    "${SHADER_SOURCE_DIR}/ui_texture.frag.slang"
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

set(SHADER_OUTPUT_DIR "${CMAKE_BINARY_DIR}/compiled_shaders")
file(MAKE_DIRECTORY "${SHADER_OUTPUT_DIR}")

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
        COMMAND "${SLANG_EXE}" "${SHADER_FILE}" -target ${SLANG_SHADER_TARGET} ${SLANG_VERT_PROFILE} -o "${OUTPUT_FILE}" -fvk-t-shift 0 0 -fvk-s-shift 0 0 -fvk-b-shift 0 1 -warnings-disable 40100 ${SLANG_DXC_FLAGS}
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
        COMMAND "${SLANG_EXE}" "${SHADER_FILE}" -target ${SLANG_SHADER_TARGET} ${SLANG_FRAG_PROFILE} -o "${OUTPUT_FILE}" -fvk-t-shift 0 2 -fvk-s-shift 0 2 -fvk-b-shift 0 3 -warnings-disable 40100 ${SLANG_DXC_FLAGS}
        DEPENDS "${SHADER_FILE}"
        COMMENT "Compiling shader: ${SHADER_FILE}"
        VERBATIM
    )
    list(APPEND COMPILED_SHADERS "${OUTPUT_FILE}")
endforeach()

add_custom_target(compile_shaders ALL DEPENDS ${COMPILED_SHADERS})
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
find_package(Doxygen)
if(DOXYGEN_FOUND)
    set(DOXYGEN_IN ${CMAKE_CURRENT_SOURCE_DIR}/Doxyfile.in)
    set(DOXYGEN_OUT ${CMAKE_CURRENT_BINARY_DIR}/Doxyfile)
    configure_file(${DOXYGEN_IN} ${DOXYGEN_OUT} @ONLY)
    add_custom_target(docs
        COMMAND ${DOXYGEN_EXECUTABLE} ${DOXYGEN_OUT}
        WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
        COMMENT "Generating API documentation"
        VERBATIM
    )
endif()

# --- Testing ---
enable_testing()
set(ROLL_A_BALL_DIR "${CMAKE_SOURCE_DIR}/examples/0_roll-a-ball")
set(ROLL_A_BALL_PROJ "${ROLL_A_BALL_DIR}/wslpro.json")
set(ROLL_A_BALL_SCENE "${ROLL_A_BALL_DIR}/scenes/main.wscn.json")
add_test(NAME validate_roll_a_ball_project COMMAND weasel --validate-project "${ROLL_A_BALL_PROJ}")
add_test(NAME validate_roll_a_ball_scene COMMAND weasel --validate-scene "${ROLL_A_BALL_SCENE}" "${ROLL_A_BALL_PROJ}")

# --- Installation ---
# Only install our own targets/resources.  CPM dependencies (slang, ai-sdk-cpp,
# etc.) have their own install() rules that break in CI (wrong paths, man pages,
# missing files).  Tagging everything with COMPONENT Runtime and limiting
# CPACK_COMPONENTS_ALL prevents CPack from touching them.
install(TARGETS wsl weasel weasel-cli
    RUNTIME DESTINATION bin COMPONENT Runtime
    LIBRARY DESTINATION lib COMPONENT Runtime
    ARCHIVE DESTINATION lib COMPONENT Runtime
)

install(DIRECTORY ${CMAKE_BINARY_DIR}/compiled_shaders DESTINATION share/weasel COMPONENT Runtime)
install(DIRECTORY ${CMAKE_BINARY_DIR}/otf DESTINATION share/weasel COMPONENT Runtime)
install(DIRECTORY ${CMAKE_BINARY_DIR}/icons DESTINATION share/weasel COMPONENT Runtime)

set(CPACK_PACKAGE_NAME "WeaselEngine")
set(CPACK_PACKAGE_VERSION "0.1.0")
set(CPACK_PACKAGE_VENDOR "Weasel Engine")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Weasel Engine — a game engine")
set(CPACK_COMPONENTS_ALL Runtime)
set(CPACK_COMPONENT_RUNTIME_DISPLAY_NAME "Weasel Engine Runtime")
set(CPACK_COMPONENT_RUNTIME_DESCRIPTION "Weasel Engine runtime binaries and resources")

# Override CPack's install step so it only installs our Runtime component.
# Without this CPack runs a full "cmake --install ." which also executes
# broken install() rules from CPM dependencies (slang, ai-sdk-cpp, brotli...).
set(CPACK_INSTALL_CMAKE_PROJECTS "${CMAKE_BINARY_DIR};weasel;Runtime;/")

if(WIN32)
    set(CPACK_GENERATOR "NSIS;ZIP" CACHE STRING "CPack generator" FORCE)
    set(CPACK_NSIS_DISPLAY_NAME "Weasel Engine")
    set(CPACK_NSIS_HELP_LINK "https://github.com/Mario1159/weasel")
    set(CPACK_NSIS_URL_INFO_ABOUT "https://github.com/Mario1159/weasel")
elseif(APPLE)
    set(CPACK_GENERATOR "productbuild;TGZ" CACHE STRING "CPack generator" FORCE)
else()
    # DEB/RPM omitted: they require distro-specific tooling (dpkg, rpmbuild) and
    # packaging metadata that is not available in CI containers.
    set(CPACK_GENERATOR "TGZ" CACHE STRING "CPack generator" FORCE)
endif()

include(CPack)
