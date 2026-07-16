include_guard(GLOBAL)

option(WEASEL_ENABLE_DASLANG "Enable daslang scripting support" ON)

if(NOT WEASEL_ENABLE_DASLANG)
    return()
endif()

include(cmake/CPM.cmake)

# Suppress -Werror for deprecated declarations in daslang's own code
# (wstring_convert is deprecated in C++17 but daslang still uses it)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-error=deprecated-declarations")

# Disable modules that use CMAKE_SOURCE_DIR (broken under CPM subdirectory)
# or pull heavy dependencies we don't need
set(DAS_GLFW_DISABLED ON CACHE BOOL "" FORCE)
set(DAS_CLANG_BIND_DISABLED ON CACHE BOOL "" FORCE)
set(DAS_LLVM_DISABLED ON CACHE BOOL "" FORCE)
set(DAS_SMT_DISABLED ON CACHE BOOL "" FORCE)
set(DAS_SQLITE_DISABLED ON CACHE BOOL "" FORCE)
set(DAS_HV_DISABLED ON CACHE BOOL "" FORCE)
set(DAS_AUDIO_DISABLED ON CACHE BOOL "" FORCE)
set(DAS_STDDLG_DISABLED ON CACHE BOOL "" FORCE)
set(DAS_STBIMAGE_DISABLED ON CACHE BOOL "" FORCE)
set(DAS_TOOLS_DISABLED ON CACHE BOOL "" FORCE)
set(DAS_AOT_EXAMPLES_DISABLED ON CACHE BOOL "" FORCE)
set(DAS_TUTORIAL_DISABLED ON CACHE BOOL "" FORCE)
set(DAS_TESTS_DISABLED ON CACHE BOOL "" FORCE)
set(DAS_BUILD_DOCUMENTATION OFF CACHE BOOL "" FORCE)

CPMAddPackage(
    NAME daslang
    GITHUB_REPOSITORY GaijinEntertainment/daScript
    GIT_TAG v0.6.3-RC3
    OPTIONS
        "BUILD_TESTING OFF"
    SYSTEM ON
)

if(TARGET libDaScriptDyn)
    add_library(weasel_daslang INTERFACE)
    target_link_libraries(weasel_daslang INTERFACE libDaScriptDyn)
    target_compile_definitions(weasel_daslang INTERFACE WEASEL_HAS_DASLANG=1)
    target_include_directories(weasel_daslang INTERFACE ${daslang_SOURCE_DIR}/include)
    message(STATUS "daslang enabled (libDaScriptDyn found)")
elseif(TARGET libDaScript)
    add_library(weasel_daslang INTERFACE)
    target_link_libraries(weasel_daslang INTERFACE libDaScript)
    target_compile_definitions(weasel_daslang INTERFACE WEASEL_HAS_DASLANG=1)
    target_include_directories(weasel_daslang INTERFACE ${daslang_SOURCE_DIR}/include)
    message(STATUS "daslang enabled (libDaScript found)")
else()
    message(STATUS "daslang not found - daslang support disabled")
endif()
