cmake_minimum_required(VERSION 3.22)

project(ImGuiColorTextEdit LANGUAGES CXX)

add_library(imguicolortextedit
    TextEditor.cpp
    TextEditor.h
    TextDiff.cpp
    TextDiff.h
    dtl.h
    extras/TrieAutoComplete.cpp
    extras/TrieAutoComplete.h
)

target_include_directories(imguicolortextedit
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}>
        $<INSTALL_INTERFACE:include>
)

if (NOT TARGET imgui::imgui)
    find_package(imgui CONFIG QUIET)
endif()

if (TARGET imgui::imgui)
    target_link_libraries(imguicolortextedit PUBLIC imgui::imgui)
else()
    message(FATAL_ERROR
        "ImGuiColorTextEdit requires Dear ImGui. "
        "Make sure imgui::imgui target exists (e.g. add ImGui via CPM before this package)."
    )
endif()

add_library(ImGuiColorTextEdit::imguicolortextedit ALIAS imguicolortextedit)
