# Copyright (c) ICG. All rights reserved.
#
# Institute for Computer Graphics and Vision
# Graz University of Technology / Austria
#
#
# This software is distributed WITHOUT ANY WARRANTY; without even
# the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
# PURPOSE.  See the above copyright notices for more information.
#
#
# Project     : vmgpu
# Module      : FlowLib Command line tool
# Class       : $RCSfile$
# Language    : CMake
# Description : CMakeFile for FlowLib Command line tool
#
# Author     :
# EMail      :

#cmake_minimum_required(VERSION 2.6)

# Create command line tool
SET(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH}
		      $ENV{CMAKE_MODULE_PATH}
		      ${CMAKE_SOURCE_DIR}/../gpu-flow/)

SET(CudaTemplates_DIR ${CMAKE_SOURCE_DIR}/../gpu-flow/cuda_templates)

SET(GPU_FLOW_SOURCES cmd/gpu_flow_unit.cpp)

if(NOT CUDA_BUILD_TYPE)
  set(CUDA_BUILD_TYPE Device)
  #set(CUDA_BUILD_TYPE Emulation)
endif()

FIND_PACKAGE(CUDA REQUIRED )
FIND_PACKAGE(CudaTemplates REQUIRED)

FIND_PACKAGE(Boost REQUIRED)
INCLUDE_DIRECTORIES(${Boost_INCLUDE_DIR})

message("CMAKE_CURRENT_SOURCE_DIR = ${CMAKE_CURRENT_SOURCE_DIR}")
message("CUDATEMPLATES_INCLUDE_DIR = ${CUDATEMPLATES_INCLUDE_DIR}")

INCLUDE_DIRECTORIES(
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${CMAKE_CURRENT_BINARY_DIR}
  ${CUDA_INCLUDE_DIRS}
  ${CUDA_CUT_INCLUDE_DIR}
  ${CUDATEMPLATES_INCLUDE_DIR}
  ${CMAKE_CURRENT_SOURCE_DIR}/include
  )

CUDA_INCLUDE_DIRECTORIES( 
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${CUDA_INCLUDE_DIRS}
  ${CUDA_CUT_INCLUDE_DIR}
  ${CUDATEMPLATES_INCLUDE_DIR}
  ${CMAKE_CURRENT_SOURCE_DIR}/include
  )

# ADD_DEFINITIONS(-use_fast_math -O3)
link_directories(
  ${CMAKE_BINARY_DIR} 
  ${Boost_LIBRARY_DIRS}
  ${CMAKE_CURRENT_SOURCE_DIR}/lib
  )

if(COMMAND cmake_policy)
      cmake_policy(SET CMP0003 NEW)
endif(COMMAND cmake_policy)

add_library(gpu-flow ${GPU_FLOW_SOURCES})

SET(FLOW_LIB_DIR ${CMAKE_CURRENT_SOURCE_DIR}/lib)

# Add system specific link libraries
if (WIN32)
  target_link_libraries(gpu-flow ${CUDA_LIBRARIES} ${CUDA_CUT_LIBRARIES} common_static vm flow)
else (WIN32)
  target_link_libraries(gpu-flow ${CUDA_LIBRARIES}
                                 ${CUDA_CUT_LIBRARIES} 
                                 ${FLOW_LIB_DIR}/libcommon.so
                                 ${FLOW_LIB_DIR}/libvm.so
                                 ${FLOW_LIB_DIR}/libflow.so)
endif (WIN32)
