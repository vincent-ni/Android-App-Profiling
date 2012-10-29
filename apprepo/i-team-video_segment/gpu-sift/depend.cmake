find_package(GLEW REQUIRED)
find_package(GLUT REQUIRED)
find_package(IL REQUIRED)
find_package(OpenCV2 REQUIRED)
find_package(OpenGL REQUIRED)

set(DEPENDENT_INCLUDES ${GLEW_INCLUDE_DIR}
		                   ${GLUT_INCLUDE_DIR}
                       ${IL_INCLUDE_DIR}
            		       ${OpenCV_INCLUDE_DIR}
            		       ${OPENGL_INCLUDE_DIR})

set(DEPENDENT_LIBRARIES ${GLEW_LIBRARY}
                        ${GLUT_LIBRARY}
                        ${IL_LIBRARIES}
                  			${OpenCV_LIBRARIES}
                  			${OPENGL_LIBRARIES})

set(DEPENDENT_LINK_DIRECTORIES)
set(CREATED_PACKAGES gpu-sift)
