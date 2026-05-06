include_guard(GLOBAL)

option(WEASEL_ENABLE_INTERPRETER "Enable embedded C++ interpretation via Clang-Repl." ON)

if(NOT WEASEL_ENABLE_INTERPRETER)
    return()
endif()

# Common install prefixes to search on each platform
if(WIN32)
    set(_LLVM20_CANDIDATE_PREFIXES
        "C:/Program Files/LLVM"
        "C:/Program Files (x86)/LLVM"
        "C:/LLVM"
        "$ENV{PROGRAMFILES}/LLVM"
    )
else()
    set(_LLVM20_CANDIDATE_PREFIXES
        "/usr/lib/llvm20"
        "/usr/lib/llvm-20"
        "/usr/local/lib/llvm20"
        "/usr/local/opt/llvm@20"
        "/opt/llvm20"
    )
endif()

# If the user hasn't set LLVM20_PREFIX, try to auto-detect an existing one
set(_LLVM20_DETECTED_PREFIX "")
if(NOT DEFINED LLVM20_PREFIX OR LLVM20_PREFIX STREQUAL "" OR LLVM20_PREFIX STREQUAL "/usr/lib/llvm20")
    foreach(_prefix IN LISTS _LLVM20_CANDIDATE_PREFIXES)
        if(EXISTS "${_prefix}/include/clang-c/Index.h" OR EXISTS "${_prefix}/lib/clang/20")
            set(_LLVM20_DETECTED_PREFIX "${_prefix}")
            break()
        endif()
    endforeach()
endif()

if(_LLVM20_DETECTED_PREFIX)
    set(LLVM20_PREFIX "${_LLVM20_DETECTED_PREFIX}" CACHE PATH "Path to LLVM 20 installation prefix" FORCE)
else()
    set(LLVM20_PREFIX "/usr/lib/llvm20" CACHE PATH "Path to LLVM 20 installation prefix")
endif()

set(LLVM20_INCLUDE_DIR "${LLVM20_PREFIX}/include")
set(LLVM20_LIB_DIR "${LLVM20_PREFIX}/lib")

# Auto-detect resource dir or allow override
set(WEASEL_CLANG_RESOURCE_DIR "${LLVM20_PREFIX}/lib/clang/20" CACHE PATH
    "Path to Clang builtin headers (resource dir)")

# We link against the combined shared libraries which is safer and avoids conflicts.
# On Windows the official LLVM installer uses different names (libclang / LLVM-C).
if(WIN32)
    find_library(CLANG_CPP_LIB
        NAMES clang-cpp libclang-cpp libclang
        PATHS "${LLVM20_LIB_DIR}" "${LLVM20_PREFIX}/bin"
        NO_DEFAULT_PATH
        NO_CACHE
    )
    find_library(LLVM_SO_LIB
        NAMES LLVM libLLVM LLVM-C
        PATHS "${LLVM20_LIB_DIR}" "${LLVM20_PREFIX}/bin"
        NO_DEFAULT_PATH
        NO_CACHE
    )
else()
    find_library(CLANG_CPP_LIB
        NAMES clang-cpp libclang-cpp
        PATHS "${LLVM20_LIB_DIR}"
        NO_DEFAULT_PATH
        NO_CACHE
    )
    find_library(LLVM_SO_LIB
        NAMES LLVM libLLVM
        PATHS "${LLVM20_LIB_DIR}"
        NO_DEFAULT_PATH
        NO_CACHE
    )
endif()

if(NOT CLANG_CPP_LIB)
    message(FATAL_ERROR "Could not find clang-cpp library in ${LLVM20_LIB_DIR}. "
        "Set LLVM20_PREFIX to the correct LLVM 20 installation path.")
endif()

# We define weasel_clang_repl as an interface library that points to the official Clang Interpreter
add_library(weasel_clang_repl INTERFACE)

target_include_directories(weasel_clang_repl INTERFACE
    "${LLVM20_INCLUDE_DIR}"
)

target_link_libraries(weasel_clang_repl INTERFACE
    "${CLANG_CPP_LIB}"
    "${LLVM_SO_LIB}"
)

target_compile_definitions(weasel_clang_repl INTERFACE
    WEASEL_HAS_EMBEDDED_INTERPRETER=1
    WEASEL_CLANG_RESOURCE_DIR=\"${WEASEL_CLANG_RESOURCE_DIR}\"
)

message(STATUS "Using official Clang Interpreter (libclang-cpp) from ${LLVM20_PREFIX} as script backend.")
