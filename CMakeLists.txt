cmake_minimum_required(VERSION 3.10)

project(RoadPorrada)

find_package(bgfx CONFIG REQUIRED)

find_package(glfw3 CONFIG REQUIRED)

add_executable(RoadPorrada roadporrada.cpp)

target_link_libraries(RoadPorrada PRIVATE glfw bgfx::bx bgfx::bgfx bgfx::bimg bgfx::bimg_decode)
