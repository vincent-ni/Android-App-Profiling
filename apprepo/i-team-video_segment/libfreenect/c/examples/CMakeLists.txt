option (BUILD_EXAMPLES "Build example programs" ON)
message("MOOP")
if (BUILD_EXAMPLES)
	message("MAP")
	include_directories (${CMAKE_SOURCE_DIR}/include)
	link_directories (${CMAKE_BINARY_DIR}/lib)

	find_package(Threads REQUIRED)
	find_package(OpenGL REQUIRED)
	find_package(GLU REQUIRED)
	find_package(GLUT REQUIRED)
	find_package(OpenCV REQUIRED)
	include_directories(${OpenCV_INCLUDE_DIRS})

	find_package(USB REQUIRED)
	include_directories(${USB_INCLUDE_DIRS})

	add_executable(glview glview.cc)
	find_library (PTHREAD pthread)
	IF(PROJECT_OS_OSX)
  		set(CMAKE_EXE_LINKER_FLAGS "-framework OpenGL -framework GLUT")
  		target_link_libraries(glview freenect ${OpenCV_LIBRARIES})
	ELSE()
  		target_link_libraries(glview freenect GL GLU glut m)
	ENDIF()

	install (TARGETS glview
		DESTINATION bin)

endif (BUILD_EXAMPLES)
