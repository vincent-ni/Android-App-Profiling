cmake_minimum_required(VERSION 2.6)

project(gpu_klt_flow)

set(CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/../cmake")
include("${CMAKE_MODULE_PATH}/common.cmake")
include("${CMAKE_SOURCE_DIR}/depend.cmake")

set(SOURCES GL/v3d_gpubase.cpp
            GL/v3d_gpupyramid.cpp
            GL/v3d_gpuklt.cpp
            GL/v3d_gpuundistort.cpp
            GL/v3d_gpuflow.cpp)

add_definitions(-DV3DLIB_ENABLE_OPENCV)
add_definitions(-DV3DLIB_ENABLE_GPGPU)
add_definitions(-DV3DLIB_GPGPU_ENABLE_CG)

headers_from_sources_cpp(HEADERS "${SOURCES}")
set(SOURCES "${SOURCES}" "${HEADERS}")

include_directories(${CMAKE_SOURCE_DIR}/../gpu_klt_flow)
add_library(gpu_klt_flow ${SOURCES})

					
# Post build step. Copy shaders to binary dir of root project.
get_property(ROOT_PROJECT
             GLOBAL
						 PROPERTY ROOT_PROJECT_NAME)

SET(ROOT_PROJECT_BINARY_VAR "${ROOT_PROJECT}_BINARY_DIR")

ADD_CUSTOM_COMMAND(
				TARGET gpu_klt_flow
				POST_BUILD
				COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_SOURCE_DIR}/../gpu_klt_flow/GL/Shaders/
				${${ROOT_PROJECT_BINARY_VAR}})

apply_dependencies(gpu_klt_flow)
