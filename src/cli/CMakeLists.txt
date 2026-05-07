file(GLOB CLI_LIB_SOURCES "*.cpp")
file(GLOB CLI_LIB_HEADERS "*.hpp")

# Remove main.cpp from library sources
list(REMOVE_ITEM CLI_LIB_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/main.cpp")


# CLI Library for shared logic
add_library(cli STATIC ${CLI_LIB_SOURCES} ${CLI_LIB_HEADERS})
add_library(Weasel::cli ALIAS cli)

target_include_directories(cli PUBLIC ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(cli PUBLIC wsl CLI11::CLI11)

# Standalone weasel-cli executable
add_executable(weasel-cli main.cpp)
set_target_properties(weasel-cli PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
    INSTALL_RPATH "$ORIGIN/../lib"
    BUILD_RPATH "$ORIGIN;$ORIGIN/../lib"
)
target_link_libraries(weasel-cli PRIVATE cli)
