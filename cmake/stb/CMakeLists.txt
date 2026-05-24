cmake_minimum_required(VERSION 3.22)
project(stb_image_builder)

add_library(stb_image STATIC stb_image_impl.c)

add_library(stb::stb_image ALIAS stb_image)

target_include_directories(stb_image
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
)