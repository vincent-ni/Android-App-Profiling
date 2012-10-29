cmake_minimum_required(VERSION 2.6)

if (APPLE)
  project(patch_track)
else(APPLE)
  project(patch_track C CXX Fortran)
endif(APPLE)

set(CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/../cmake")
include(${CMAKE_MODULE_PATH}/common.cmake)
include("${CMAKE_SOURCE_DIR}/depend.cmake")

find_package(Qt4 COMPONENTS QtCore QtGui REQUIRED)
include(${QT_USE_FILE})

set(SOURCES main.cpp
            main_window.cpp
            buffered_image.cpp)

headers_from_sources_cpp(HEADERS "${SOURCES}")

QT4_WRAP_CPP(MOC_SRC main_window.h)

set(SOURCES "${SOURCES}" "${HEADERS}" "${MOC_SRC}")

add_executable(patch_track ${SOURCES} )

if(APPLE)
  find_library(COCOA NAMES Cocoa)
  find_library(QTKIT NAMES QTKIT)
  target_link_libraries(patch_track ${QT_LIBRARIES}
                                    ${COCOA}
                                    ${QTKIT})
else(APPLE)
  target_link_libraries(patch_track ${QT_LIBRARIES})
endif(APPLE)

apply_dependencies(patch_track)
