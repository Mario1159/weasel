include_guard(GLOBAL)

option(WEASEL_ENABLE_INTERPRETER "Enable embedded C++ interpretation via Clang-Repl." ON)

if(NOT WEASEL_ENABLE_INTERPRETER)
    return()
endif()

set(LLVM20_PREFIX "/usr/lib/llvm20" CACHE PATH "Path to LLVM 20 installation prefix")
set(LLVM20_INCLUDE_DIR "${LLVM20_PREFIX}/include")
set(LLVM20_LIB_DIR "${LLVM20_PREFIX}/lib")

# Auto-detect resource dir or allow override
set(WEASEL_CLANG_RESOURCE_DIR "${LLVM20_PREFIX}/lib/clang/20" CACHE PATH
    "Path to Clang builtin headers (resource dir)")

# We link against the combined shared libraries which is safer and avoids conflicts
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

if(NOT CLANG_CPP_LIB)
    message(FATAL_ERROR "Could not find clang-cpp library in ${LLVM20_LIB_DIR}. "
        "Set LLVM20_PREFIX to the correct LLVM installation path.")
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
