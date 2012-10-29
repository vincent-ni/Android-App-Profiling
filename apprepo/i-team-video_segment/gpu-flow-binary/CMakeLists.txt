cmake_minimum_required(VERSION 2.6)

project(gpu-flow-binary)

set(CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/../cmake")
include(${CMAKE_MODULE_PATH}/common.cmake)
include("${CMAKE_SOURCE_DIR}/depend.cmake")

add_executable(gpu-flow-binary flow_demo.cpp)

apply_dependencies(gpu-flow-binary)
