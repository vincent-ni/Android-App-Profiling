cmake_minimum_required(VERSION 2.6)

project(gpu-flow)
set(CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/../cmake")
include(${CMAKE_MODULE_PATH}/common.cmake)

include("${CMAKE_SOURCE_DIR}/../gpu-flow/cmd/cmd.cmake")
include("${CMAKE_SOURCE_DIR}/../gpu-flow/depend.cmake")

apply_dependencies(gpu-flow)
