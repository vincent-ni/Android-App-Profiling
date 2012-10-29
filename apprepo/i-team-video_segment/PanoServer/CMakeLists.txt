cmake_minimum_required(VERSION 2.6)

project(PanoServer)

set(CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/../cmake")
include(${CMAKE_MODULE_PATH}/common.cmake)
include("${CMAKE_SOURCE_DIR}/depend.cmake")

include(${QT_USE_FILE})

set(SOURCES main.cpp main_window.cpp thumbnail_view.cpp
	          compositing_widget.cpp pano_server_tcp.cpp
            pano_server_udp.cpp)

headers_from_sources_cpp(HEADERS "${SOURCES}")

QT4_WRAP_CPP(MOC_SRC main_window.h 
                     thumbnail_view.h
                     compositing_widget.h
                     pano_server_tcp.h
                     pano_server_udp.h)

set(SOURCES "${SOURCES}" "${HEADERS}" "${MOC_SRC}")

add_executable(PanoServer ${SOURCES} )

include_directories(${Boost_INCLUDE_DIR})
target_link_libraries(PanoServer ${QT_LIBRARIES})

apply_dependencies(PanoServer)
