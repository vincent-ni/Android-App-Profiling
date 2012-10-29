cmake_minimum_required(VERSION 2.6)

if(APPLE)
  project(irradiance_estimation)
else(APPLE)
  project(irradiance_estimation C CXX Fortran)
endif(APPLE)

set(CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/../cmake")
include(${CMAKE_MODULE_PATH}/common.cmake)
include("${CMAKE_SOURCE_DIR}/depend.cmake")

set(SOURCES main.cpp irradiance_estimation.cpp)
headers_from_sources_cpp(HEADERS "${SOURCES}")
set(SOURCES "${SOURCES}" "${HEADERS}")

add_executable(irradiance_estimation ${SOURCES})

include(FindOpenMP)
if(OPENMP_FOUND)
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${OpenMP_C_FLAGS}")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${OpenMP_CXX_FLAGS}")
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} ${OpenMP_EXE_LINKER_FLAGS}")
endif()

apply_dependencies(irradiance_estimation)

