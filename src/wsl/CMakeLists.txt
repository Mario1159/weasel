file(GLOB_RECURSE WSL_SOURCES 
    "*.cpp"
    "*.c"
    "log/*.cpp"
)

add_library(wsl SHARED ${WSL_SOURCES})
add_library(Weasel::wsl ALIAS wsl)

set_target_properties(wsl PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
)

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
    EnTT::EnTT
    spdlog
    cereal
    imguizmo
    imguitextselect
    imsearch
    imviewguizmo::imviewguizmo
    ImGuiColorTextEdit::imguicolortextedit
    glm_dep
    slang::slang
)

if(TARGET weasel_clang_repl)
    target_link_libraries(wsl PUBLIC weasel_clang_repl)
endif()

target_link_libraries(wsl PRIVATE
    GameNetworkingSockets
    TracyClient
    LibArchive::LibArchive
    ${CMAKE_DL_LIBS}
)

if(TARGET imgui)
    target_link_libraries(wsl PUBLIC imgui)
endif()

target_compile_definitions(wsl PUBLIC JPH_DEBUG_RENDERER CPP_RTTI_ENABLED)
target_compile_definitions(wsl PRIVATE WEASEL_BUILD_EDITOR)
target_compile_definitions(wsl PRIVATE
    WEASEL_SOURCE_DIR=\"${CMAKE_SOURCE_DIR}\"
    WEASEL_BUILD_DIR=\"${CMAKE_BINARY_DIR}\"
)

if(MSVC)
    target_compile_options(wsl PRIVATE /W4 /permissive-)
else()
    target_compile_options(wsl PRIVATE -Wall -Wextra -Wpedantic -rdynamic)
endif()

if(WEASEL_ENABLE_INTERPRETER AND TARGET weasel_clang_repl)
    add_dependencies(wsl weasel_clang_repl)
endif()
