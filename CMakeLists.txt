cmake_minimum_required(VERSION 3.10)

project(RoadPorrada)

# this is heuristically generated, and may not be correct
find_package(bgfx CONFIG REQUIRED)
# note: 1 additional targets are not displayed.
target_link_libraries(main PRIVATE bgfx::bx bgfx::bgfx bgfx::bimg bgfx::bimg_decode)


add_executable(RoadPorrada roadporrada.cpp)

target_link_libraries(RoadPorrada)