cmake_minimum_required(VERSION 2.6)

project(seg_server)

set(CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/../cmake")
include(${CMAKE_MODULE_PATH}/common.cmake)
include("${CMAKE_SOURCE_DIR}/depend.cmake")

add_executable(seg_server seg_server.cpp)

apply_dependencies(seg_server)
