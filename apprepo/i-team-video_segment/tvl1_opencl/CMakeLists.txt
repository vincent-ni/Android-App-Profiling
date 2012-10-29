cmake_minimum_required(VERSION 2.8)

project(tvl1_opencl)

#find_package(OpenCV REQUIRED)

set(CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/../cmake")
include(${CMAKE_MODULE_PATH}/common.cmake)
include("${CMAKE_SOURCE_DIR}/depend.cmake")

find_package(CUDA REQUIRED)
find_package(OpenCL REQUIRED)

#include_directories(${OpenCV_INCLUDE_DIR})
include_directories(${OpenCV_INCLUDE_DIRS})
include_directories(${CUDA_INCLUDE_DIR})
include_directories(${OPENCL_INCLUDE_DIR})
include_directories(${tvl1_opencl_SOURCE_DIR})
include_directories(${tvl1_opencl_SOURCE_DIR}/simpleCL)
include_directories(${tvl1_opencl_SOURCE_DIR}/tvl1)
include_directories(${tvl1_opencl_SOURCE_DIR}/timer)

include_directories(${Boost_INCLUDE_DIR})
link_directories(${OpenCV_LINK_DIRECTORIES})

link_directories(${tvl1_opencl_BINARY_DIR}/simpleCL)
link_directories(${tvl1_opencl_BINARY_DIR}/tvl1)
link_directories(${tvl1_opencl_BINARY_DIR}/timer)

add_subdirectory(timer)
add_subdirectory(simpleCL)
add_subdirectory(tvl1)

add_library(tvl1_opencl cl_flow_unit.cpp)
#target_link_libraries(tvl1_opencl tvl1 simplecl ${OpenCV_LIBS} ${OPENCL_LIBRARIES})
target_link_libraries(tvl1_opencl tvl1 simplecl ${OPENCL_LIBRARIES} ${OpenCV_LIBRARIES})

apply_dependencies(tvl1_opencl)

add_executable(demo demo.cpp)
#target_link_libraries(demo tvl1 simplecl ${OpenCV_LIBS} ${OPENCL_LIBRARIES})
target_link_libraries(demo tvl1 simplecl ${OPENCL_LIBRARIES} ${OpenCV_LIBRARIES})
