cmake_minimum_required(VERSION 2.6)

project(joint_seg_tree_sample)

set(CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/../cmake")
include(${CMAKE_MODULE_PATH}/common.cmake)
include("${CMAKE_SOURCE_DIR}/depend.cmake")

add_executable(joint_seg_tree_sample joint_seg_tree.cpp)

apply_dependencies(joint_seg_tree_sample)
