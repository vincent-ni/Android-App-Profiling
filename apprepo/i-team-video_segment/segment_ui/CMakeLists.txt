cmake_minimum_required(VERSION 2.6)

project(segment_ui)

set(CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/../cmake")
include(${CMAKE_MODULE_PATH}/common.cmake)
include("${CMAKE_SOURCE_DIR}/depend.cmake")

find_package(Qt4 COMPONENTS QtCore QtGui REQUIRED)
include(${QT_USE_FILE})

set(SOURCES main.cpp
            mainWindow.cpp
            movieWidget.cpp
	          video_data.cpp)

headers_from_sources_cpp(HEADERS "${SOURCES}")

QT4_WRAP_CPP(MOC_SRC mainWindow.h
                     movieWidget.h)

set(SOURCES "${SOURCES}" "${HEADERS}" "${MOC_SRC}")
add_executable(segment_ui ${SOURCES} )

if (APPLE)
  find_library(COCOA NAMES Cocoa)
  find_library(QTKIT NAMES QTKIT)
  target_link_libraries(segment_ui ${QT_LIBRARIES}
                                   ${COCOA}
                                   ${QTKIT})
else (APPLE)
  target_link_libraries(segment_ui ${QT_LIBRARIES})
endif(APPLE)

apply_dependencies(segment_ui)
