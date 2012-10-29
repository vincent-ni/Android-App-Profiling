cmake_minimum_required(VERSION 2.6)

project(gpu-sift)

set(CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/../cmake")
include("${CMAKE_MODULE_PATH}/common.cmake")
include("${CMAKE_SOURCE_DIR}/depend.cmake")

set(SOURCES FrameBufferObject.cpp
            GlobalUtil.cpp
            GLTexImage.cpp
            ProgramGLSL.cpp
            ProgramGPU.cpp
            ShaderMan.cpp
            SiftGPU.cpp
            SiftPyramid.cpp
            PyramidGL.cpp
            SiftMatch.cpp
            ProgramCG.cpp
            )

add_definitions(-DWINDOW_PREFER_GLUT)

headers_from_sources_cpp(HEADERS "${SOURCES}")
set(SOURCES "${SOURCES}" "${HEADERS}")

add_library(gpu-sift ${SOURCES})

apply_dependencies(gpu-sift)
