include_guard(GLOBAL)

option(WEASEL_ENABLE_INTERPRETER "Enable embedded C++ interpretation via Clang-Repl." ON)

if(NOT WEASEL_ENABLE_INTERPRETER)
    return()
endif()

# Manual configuration to avoid naming conflicts with sdl_shadercross's internal LLVM
set(LLVM20_PREFIX "/usr/lib/llvm20")
set(LLVM20_INCLUDE_DIR "${LLVM20_PREFIX}/include")
set(LLVM20_LIB_DIR "${LLVM20_PREFIX}/lib")

# We link against the combined shared libraries which is safer and avoids conflicts
set(CLANG_CPP_LIB "${LLVM20_LIB_DIR}/libclang-cpp.so")
set(LLVM_SO_LIB "${LLVM20_LIB_DIR}/libLLVM.so")

if(NOT EXISTS "${CLANG_CPP_LIB}")
    message(FATAL_ERROR "Could not find ${CLANG_CPP_LIB}")
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
    WEASEL_CLANG_RESOURCE_DIR=\"/usr/lib/llvm20/lib/clang/20\"
)

message(STATUS "Using official Clang Interpreter (libclang-cpp) from LLVM 20 as script backend.")
