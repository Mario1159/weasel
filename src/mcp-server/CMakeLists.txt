file(GLOB MCP_SERVER_LIB_SOURCES "*.cpp")
file(GLOB MCP_SERVER_LIB_HEADERS "*.hpp")

list(REMOVE_ITEM MCP_SERVER_LIB_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/main.cpp")

add_library(mcp_server_lib STATIC ${MCP_SERVER_LIB_SOURCES} ${MCP_SERVER_LIB_HEADERS})
add_library(Weasel::mcp_server_lib ALIAS mcp_server_lib)

target_include_directories(mcp_server_lib PUBLIC
    ${CMAKE_SOURCE_DIR}/src
    ${cpp-mcp_SOURCE_DIR}/include
    ${cpp-mcp_SOURCE_DIR}/common
)
target_link_libraries(mcp_server_lib PUBLIC mcp CLI11::CLI11)

add_executable(weasel-mcp-server main.cpp)
set_target_properties(weasel-mcp-server PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
    INSTALL_RPATH "$ORIGIN/../lib"
    BUILD_RPATH "$ORIGIN;$ORIGIN/../lib"
)
target_link_libraries(weasel-mcp-server PRIVATE mcp_server_lib)
