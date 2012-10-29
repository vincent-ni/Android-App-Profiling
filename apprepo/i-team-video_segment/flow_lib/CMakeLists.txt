cmake_minimum_required(VERSION 2.6)

project(flow_lib)
set(CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/../cmake")
include(${CMAKE_MODULE_PATH}/common.cmake)
include("${CMAKE_SOURCE_DIR}/depend.cmake")

set(SOURCES optical_flow_unit.cpp
            region_flow.cpp)

headers_from_sources_cpp(HEADERS "${SOURCES}")

find_package(Protobuf REQUIRED)
include_directories(${CMAKE_CURRENT_BINARY_DIR})
PROTOBUF_GENERATE_CPP(PROTO_SRCS PROTO_HDRS flow_utility_options.proto
                                            flow_frame.proto)
set(SOURCES "${SOURCES}"
            "${HEADERS}"
            "${PROTO_SRCS}"
            "${PROTO_HDRS}")

add_library(flow_lib ${SOURCES})

apply_dependencies(flow_lib)
