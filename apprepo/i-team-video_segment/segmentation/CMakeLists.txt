cmake_minimum_required(VERSION 2.6)

project(segmentation)
set(CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/../cmake")
include(${CMAKE_MODULE_PATH}/common.cmake)
include("${CMAKE_SOURCE_DIR}/depend.cmake")

set(SOURCES segmentation_unit.cpp
            segmentation.cpp
            segmentation_graph.cpp
            segmentation_common.cpp
            dense_segmentation_graph.cpp
            region_segmentation_graph.cpp
            region_descriptor.cpp
            histograms.cpp
            pixel_distance.cpp
            depth.cpp
            )

headers_from_sources_cpp(HEADERS "${SOURCES}")
set(SOURCES "${SOURCES}" "${HEADERS}")

# opencl
find_package(OpenCL REQUIRED)
include_directories(${OPENCL_INCLUDE_DIR})
add_subdirectory(timer)
add_subdirectory(simpleCL)
link_directories(${TVL1_BINARY_DIR}/simpleCL)


# Custom build steps.
find_package(Protobuf REQUIRED)
include_directories(${CMAKE_CURRENT_BINARY_DIR})
PROTOBUF_GENERATE_CPP(PROTO_SRCS PROTO_HDRS segmentation_options.proto)

add_library(segmentation ${SOURCES} ${PROTO_SRCS} ${PROTO_HDRS})

include_directories(${OpenCV_INCLUDE_DIRS} ${Boost_INCLUDE_DIR}
                    ${PROTOBUF_INCLUDE_DIR})

target_link_libraries(segmentation simplecl ${OPENCL_LIBRARIES})

apply_dependencies(segmentation)
