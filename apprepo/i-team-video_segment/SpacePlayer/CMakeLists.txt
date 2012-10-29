cmake_minimum_required(VERSION 2.6)

project(SpacePlayer)

set(CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/../cmake")
include(${CMAKE_MODULE_PATH}/common.cmake)
include("${CMAKE_SOURCE_DIR}/depend.cmake")

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DQT_MAC_USE_COCOA")
find_package(Qt4 COMPONENTS QtCore QtGui REQUIRED)
include(${QT_USE_FILE})

set(SOURCES main.cpp main_window.cpp thumbnail_view.cpp
	          compositing_widget.cpp buffered_image.cpp play_unit.cpp
  	        video_analyzer.cpp canvas_cut.cpp foreground_tracker.cpp)

headers_from_sources_cpp(HEADERS "${SOURCES}")

QT4_WRAP_CPP(MOC_SRC main_window.h 
                     thumbnail_view.h
                     compositing_widget.h
                     play_unit.h
                     video_analyzer.h)

set(SOURCES "${SOURCES}" "${HEADERS}" "${MOC_SRC}")

add_executable(SpacePlayer ${SOURCES} )

include_directories(${Boost_INCLUDE_DIR})

target_link_libraries(SpacePlayer ${QT_LIBRARIES})

#set_target_properties(SpacePlayer MACOSX_BUNDLE)

apply_dependencies(SpacePlayer)
