cmake_minimum_required(VERSION 3.22)
project(go-demo LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Weasel Engine dependency
find_package(Weasel REQUIRED)

file(GLOB_RECURSE SOURCES
    "src/*.cpp"
    "src/components/*.cpp"
    "src/systems/*.cpp"
    "src/singletons/*.cpp"
)

# AOT-compile .das files. Only system/entry scripts are AOT-compiled;
# component .das files define `require`-able modules (e.g. `class MouseRotate`)
# and are passed as `-I` search roots via COMPONENTS, not compiled standalone.
file(GLOB_RECURSE DAS_COMPONENTS "src/components/*.das")
file(GLOB_RECURSE DAS_SYSTEMS "src/systems/*.das")
if(DAS_SYSTEMS)
    weasel_aot_das(FILES ${DAS_SYSTEMS} COMPONENTS ${DAS_COMPONENTS} OUTPUT_VAR AOT_SRCS)
    list(APPEND SOURCES ${AOT_SRCS})
endif()

add_executable(${PROJECT_NAME} ${SOURCES})
target_link_libraries(${PROJECT_NAME} PRIVATE wsl)

if(NOT DEFINED Weasel_RESOURCE_PATH OR Weasel_RESOURCE_PATH STREQUAL "")
  set(Weasel_RESOURCE_PATH "${Weasel_DIR}")
endif()
target_compile_definitions(${PROJECT_NAME} PRIVATE WSL_RESOURCE_PATH="${Weasel_RESOURCE_PATH}")

# --- Installation and Packaging ---
install(TARGETS ${PROJECT_NAME}
    RUNTIME DESTINATION bin
)

# Install project resources
install(DIRECTORY src DESTINATION share/${PROJECT_NAME})
install(DIRECTORY audio DESTINATION share/${PROJECT_NAME} OPTIONAL)
install(DIRECTORY textures DESTINATION share/${PROJECT_NAME} OPTIONAL)
install(DIRECTORY rml DESTINATION share/${PROJECT_NAME} OPTIONAL)
install(DIRECTORY otf DESTINATION share/${PROJECT_NAME} OPTIONAL)
install(DIRECTORY scenes DESTINATION share/${PROJECT_NAME} OPTIONAL)
install(DIRECTORY shaders DESTINATION share/${PROJECT_NAME} OPTIONAL)

set(CPACK_PACKAGE_NAME "${PROJECT_NAME}")
set(CPACK_PACKAGE_VERSION "0.1.0")
set(CPACK_GENERATOR "TGZ;DEB;RPM")

include(CPack)
