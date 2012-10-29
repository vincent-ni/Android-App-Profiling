cmake_minimum_required(VERSION 2.6)

project(imagefilter)

set(CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/../cmake")
include("${CMAKE_MODULE_PATH}/common.cmake")
include("${CMAKE_SOURCE_DIR}/depend.cmake")

set(SOURCES image_filter.cpp nnf.cpp bilateral_filter_1D.cpp warping.cpp)

headers_from_sources_cpp(HEADERS "${SOURCES}")
set(SOURCES "${SOURCES}" "${HEADERS}" image_util.h)

add_library(imagefilter ${SOURCES})

include_directories(${OpenCV_INCLUDE_DIRS} ${Boost_INCLUDE_DIR})

apply_dependencies(imagefilter)
