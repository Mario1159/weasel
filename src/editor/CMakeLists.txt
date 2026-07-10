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

if(APPLE)
    # macOS app bundle layout:
    #   Weasel.app/Contents/MacOS/weasel          (the executable)
    #   Weasel.app/Contents/Frameworks/*.dylib    (libwsl, libTracyClient)
    #   Weasel.app/Contents/Resources/share/weasel (compiled_shaders, otf, icons)
    #   Weasel.app/Contents/Info.plist
    set_target_properties(weasel PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
        MACOSX_BUNDLE TRUE
        MACOSX_BUNDLE_BUNDLE_NAME "Weasel Engine"
        MACOSX_BUNDLE_BUNDLE_VERSION "${CPACK_PACKAGE_VERSION}"
        MACOSX_BUNDLE_SHORT_VERSION_STRING "${CPACK_PACKAGE_VERSION}"
        MACOSX_BUNDLE_LONG_VERSION_STRING "${CPACK_PACKAGE_VERSION}"
        MACOSX_BUNDLE_GUI_IDENTIFIER "${CPACK_PRODUCTBUILD_IDENTIFIER}"
        MACOSX_BUNDLE_INFO_STRING "Weasel Engine ${CPACK_PACKAGE_VERSION}"
        MACOSX_BUNDLE_COPYRIGHT "${CPACK_PACKAGE_VENDOR}"
        MACOSX_BUNDLE_INFO_PLIST "${CMAKE_BINARY_DIR}/Weasel.app/Contents/Info.plist"
        INSTALL_RPATH "@executable_path/../Frameworks"
        BUILD_RPATH "@executable_path/../Frameworks"
    )

    set(MACOSX_BUNDLE_BUNDLE_NAME "Weasel Engine")
    set(MACOSX_BUNDLE_EXECUTABLE_NAME "weasel")
    configure_file(
        "${CMAKE_SOURCE_DIR}/cmake/Info.plist.in"
        "${CMAKE_BINARY_DIR}/Weasel.app/Contents/Info.plist"
        @ONLY
    )

    # Populate the bundle after build: dylibs go to Frameworks/, resources
    # (compiled_shaders, otf, icons) go to Resources/share/weasel/ so the
    # editor's runtime resource lookup (src/editor/main.cpp) can find them
    # at <exe>/../Resources/share/weasel.
    set(_WEASEL_APP_DIR "${CMAKE_BINARY_DIR}/Weasel.app")
    set(_WEASEL_FRAMEWORKS "${_WEASEL_APP_DIR}/Contents/Frameworks")
    set(_WEASEL_RESOURCES  "${_WEASEL_APP_DIR}/Contents/Resources")
    set(_WEASEL_SHARE      "${_WEASEL_RESOURCES}/share/weasel")

    add_custom_command(TARGET weasel POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory
            "${_WEASEL_FRAMEWORKS}"
            "${_WEASEL_SHARE}"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CMAKE_BINARY_DIR}/libwsl.dylib"
            "${_WEASEL_FRAMEWORKS}/"
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${CMAKE_BINARY_DIR}/compiled_shaders"
            "${_WEASEL_SHARE}/compiled_shaders"
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${CMAKE_BINARY_DIR}/otf"
            "${_WEASEL_SHARE}/otf"
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${CMAKE_BINARY_DIR}/icons"
            "${_WEASEL_SHARE}/icons"
        COMMENT "Populating Weasel.app bundle"
        VERBATIM
    )

    if(TARGET TracyClient)
        add_custom_command(TARGET weasel POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "$<TARGET_FILE:TracyClient>"
                "${_WEASEL_FRAMEWORKS}/"
            COMMENT "Copying TracyClient into Weasel.app"
            VERBATIM
        )
    endif()
else()
    set_target_properties(weasel PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
        INSTALL_RPATH "$ORIGIN/../lib"
        BUILD_RPATH "$ORIGIN;$ORIGIN/../lib"
    )
endif()

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
    imnodeflow
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

target_compile_definitions(weasel PUBLIC JPH_DEBUG_RENDERER CPP_RTTI_ENABLED WEASEL_BUILD_EDITOR IMGUI_DEFINE_MATH_OPERATORS)
if(WEASEL_ENABLE_MULTIPLAYER)
    target_compile_definitions(weasel PUBLIC WEASEL_ENABLE_MULTIPLAYER)
endif()
target_compile_definitions(weasel PRIVATE
    WEASEL_SOURCE_DIR=\"${CMAKE_SOURCE_DIR}\"
    WEASEL_BUILD_DIR=\"${CMAKE_BINARY_DIR}\"
    TRACY_ENABLE
)

set_target_properties(weasel PROPERTIES ENABLE_EXPORTS ON)

target_include_directories(weasel PUBLIC ${CMAKE_SOURCE_DIR}/src)

if(WEASEL_ENABLE_RENDERDOC)
    # Mirror the include path + define that the wsl target uses, so the
    # editor's UI code can call into wsl::gfx::rdoc directly.
    target_include_directories(weasel PRIVATE
        "${CMAKE_SOURCE_DIR}/cmake/renderdoc")
    target_compile_definitions(weasel PRIVATE WEASEL_ENABLE_RENDERDOC)
endif()

if(MSVC)
    target_compile_options(weasel PRIVATE /W4 /permissive-)
else()
    target_compile_options(weasel PRIVATE -Wall -Wextra -Wpedantic -rdynamic)
endif()

add_dependencies(weasel compile_shaders copy_otf copy_icons)

# Slang loads its downstream compiler plugins (slang-glslang, which also
# provides spirv-opt) at runtime via dlopen. These are separate shared
# libraries that are NOT linked into the executable, so they must be deployed
# where the dynamic loader can find them. The weasel executable's RUNPATH
# includes $ORIGIN (the directory containing the executable), so copying the
# plugin(s) there lets slang locate them when the shader graph compiles.
# Without this, slang fails to load the downstream compiler and then crashes
# with "free(): corrupted unsorted chunks" during Linkage teardown.
set(_WEASEL_SLANG_PLUGINS slang-glslang slang-glsl-module)
foreach(_slang_plugin ${_WEASEL_SLANG_PLUGINS})
  if(TARGET ${_slang_plugin})
    add_dependencies(weasel ${_slang_plugin})
    add_custom_command(TARGET weasel POST_BUILD
      COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "$<TARGET_FILE:${_slang_plugin}>"
        "$<TARGET_FILE_DIR:weasel>/"
      COMMENT "Deploying Slang runtime plugin ${_slang_plugin}"
      VERBATIM)
    if(NOT APPLE)
      install(FILES "$<TARGET_FILE:${_slang_plugin}>"
        DESTINATION lib
        COMPONENT Runtime)
    endif()
  endif()
endforeach()

# The shader graph compiles at runtime by shelling out to the `slangc`
# executable (see wsl/gfx/shader_compiler.cpp). Deploy it next to the editor
# so it is findable at runtime without relying on the system PATH.
if(TARGET slangc)
  add_dependencies(weasel slangc)
  add_custom_command(TARGET weasel POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
      "$<TARGET_FILE:slangc>"
      "$<TARGET_FILE_DIR:weasel>/"
    COMMENT "Deploying slangc next to the editor"
    VERBATIM)
  if(NOT APPLE)
    install(FILES "$<TARGET_FILE:slangc>"
      DESTINATION bin
      COMPONENT Runtime)
  endif()
endif()

if(APPLE)
  # Inside the macOS app bundle the plugin must live in Frameworks/ so the
  # @executable_path/../Frameworks RUNPATH resolves it.
  foreach(_slang_plugin ${_WEASEL_SLANG_PLUGINS})
    if(TARGET ${_slang_plugin})
      add_custom_command(TARGET weasel POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
          "$<TARGET_FILE:${_slang_plugin}>"
          "${_WEASEL_FRAMEWORKS}/"
        COMMENT "Deploying Slang runtime plugin ${_slang_plugin} into bundle"
        VERBATIM)
    endif()
  endforeach()
endif()
