cmake_minimum_required(VERSION 2.6)

project(joint_segmentation)
set(CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/../cmake")
include(${CMAKE_MODULE_PATH}/common.cmake)
include("${CMAKE_SOURCE_DIR}/depend.cmake")

set(SOURCES joint_segmentation_unit.cpp
            )

headers_from_sources_cpp(HEADERS "${SOURCES}")
set(SOURCES "${SOURCES}" "${HEADERS}")

add_library(joint_segmentation ${SOURCES})

apply_dependencies(joint_segmentation)
