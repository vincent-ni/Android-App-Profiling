cmake_minimum_required(VERSION 2.8)
project(TVL1)

SET( CMAKE_COLOR_MAKEFILE ON )
SET( CMAKE_INCLUDE_CURRENT_DIR TRUE )

set(CMAKE_MODULE_PATH ${TVL1_SOURCE_DIR}/cmake)

find_package(OpenCV REQUIRED)
find_package(CUDA REQUIRED)
find_package(OpenCL REQUIRED)
find_package(Qt4 COMPONENTS QtCore QtGui REQUIRED)

include(${QT_USE_FILE})

include_directories(${OpenCV_INCLUDE_DIR})
include_directories(${CUDA_INCLUDE_DIR})
include_directories(${OPENCL_INCLUDE_DIR})
include_directories(${TVL1_SOURCE_DIR})
include_directories(${TVL1_SOURCE_DIR}/simpleCL)
include_directories(${TVL1_SOURCE_DIR}/tvl1)
include_directories(${TVL1_SOURCE_DIR}/timer)

link_directories(${TVL1_BINARY_DIR}/simpleCL)
link_directories(${TVL1_BINARY_DIR}/tvl1)
link_directories(${TVL1_BINARY_DIR}/timer)

add_subdirectory(timer)
add_subdirectory(simpleCL)
add_subdirectory(tvl1)

### Qt Gui stuff ###
SET(headers_ui 
	MainWindow.h
    FrameLabel.h
)
SET(uis
    mainWindow.ui
)
SET(ui_srcs
    MainWindow.cpp
    FrameLabel.cpp
)
#Generate .h files from the .ui files
QT4_WRAP_UI(moc_uis ${uis})
#This will generate moc_* for Qt
QT4_WRAP_CPP(moc_srcs ${headers_ui})
### Qt Gui stuff  end###

add_executable(qt_flow_demo main.cpp ${ui_srcs} ${moc_srcs} ${moc_uis} )
target_link_libraries(qt_flow_demo tvl1 simplecl ${OpenCV_LIBS} ${OPENCL_LIBRARIES} ${QT_LIBRARIES})
