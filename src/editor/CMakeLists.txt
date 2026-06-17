file(GLOB_RECURSE EDITOR_SOURCES
    "*.cpp"
)

# Ensure new files are picked up if glob fails to update
list(APPEND EDITOR_SOURCES
    "console.cpp"
    "logger.cpp"
)
list(REMOVE_DUPLICATES EDITOR_SOURCES)

add_executable(weasel ${EDITOR_SOURCES})

set_target_properties(weasel PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
    INSTALL_RPATH "$ORIGIN/../lib"
    BUILD_RPATH "$ORIGIN;$ORIGIN/../lib"
)

target_link_libraries(weasel PRIVATE
    wsl
    cli
    SDL3::SDL3
    SDL3_image::SDL3_image
    SDL3_mixer::SDL3_mixer
    RmlUi::RmlUi
    Jolt
    fastgltf
    EnTT::EnTT
    TracyClient
    imguizmo
    spdlog
    cereal
    LibArchive::LibArchive
    imguitextselect
    imsearch
    imviewguizmo::imviewguizmo
    ImGuiColorTextEdit::imguicolortextedit
    imgizmo2d
    glm_dep
    slang::slang
    CLI11::CLI11
    ai::sdk
    ${CMAKE_DL_LIBS}
)

if(TARGET weasel_clang_repl)
    target_link_libraries(weasel PRIVATE weasel_clang_repl)
endif()

if(TARGET imgui)
    target_link_libraries(weasel PRIVATE imgui)
endif()

target_compile_definitions(weasel PUBLIC JPH_DEBUG_RENDERER CPP_RTTI_ENABLED WEASEL_BUILD_EDITOR)
if(WEASEL_ENABLE_MULTIPLAYER)
    target_compile_definitions(weasel PUBLIC WEASEL_ENABLE_MULTIPLAYER)
endif()
target_compile_definitions(weasel PRIVATE
    WEASEL_SOURCE_DIR=\"${CMAKE_SOURCE_DIR}\"
    WEASEL_BUILD_DIR=\"${CMAKE_BINARY_DIR}\"
)

set_target_properties(weasel PROPERTIES ENABLE_EXPORTS ON)

target_include_directories(weasel PUBLIC ${CMAKE_SOURCE_DIR}/src)

if(MSVC)
    target_compile_options(weasel PRIVATE /W4 /permissive-)
else()
    target_compile_options(weasel PRIVATE -Wall -Wextra -Wpedantic -rdynamic)
endif()

add_dependencies(weasel compile_shaders copy_otf copy_icons)
