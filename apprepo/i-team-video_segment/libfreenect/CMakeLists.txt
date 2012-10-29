cmake_minimum_required(VERSION 2.6)

project(glview)
set(CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/../cmake")
include(${CMAKE_MODULE_PATH}/common.cmake)

include("${CMAKE_SOURCE_DIR}/depend.cmake")

add_subdirectory(c)

apply_dependencies(glview)
