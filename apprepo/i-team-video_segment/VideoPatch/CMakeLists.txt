cmake_minimum_required(VERSION 2.6)

project(VideoPatch)

set(CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/../cmake")
include(${CMAKE_MODULE_PATH}/common.cmake)
include("${CMAKE_SOURCE_DIR}/depend.cmake")

include(${QT_USE_FILE})

set(SOURCES main.cpp main_window.cpp thumbnail_view.cpp
	          compositing_widget.cpp)

headers_from_sources_cpp(HEADERS "${SOURCES}")

QT4_WRAP_CPP(MOC_SRC main_window.h 
                     thumbnail_view.h
                     compositing_widget.h)

set(SOURCES "${SOURCES}" "${HEADERS}" "${MOC_SRC}")

add_executable(VideoPatch ${SOURCES} )

include_directories(${Boost_INCLUDE_DIR})
target_link_libraries(VideoPatch ${QT_LIBRARIES})

apply_dependencies(VideoPatch)
