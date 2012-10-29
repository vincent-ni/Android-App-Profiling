cmake_minimum_required(VERSION 2.6)

project(video_playground)
set(CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/../cmake")
include(${CMAKE_MODULE_PATH}/common.cmake)
include("${CMAKE_SOURCE_DIR}/depend.cmake")

set(SOURCES main.cpp color_checker.cpp)
headers_from_sources_cpp(HEADERS "${SOURCES}")
set(SOURCES "${SOURCES}" "${HEADERS}")

add_executable(video_playground ${SOURCES})

apply_dependencies(video_playground)
