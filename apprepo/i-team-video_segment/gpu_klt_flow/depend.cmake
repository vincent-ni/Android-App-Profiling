find_package(GLEW REQUIRED)
find_package(OpenCV REQUIRED)
find_package(OpenGL REQUIRED)
find_package(CG REQUIRED)

set(DEPENDENT_INCLUDES ${GLEW_INCLUDE_DIR}
		                   ${OpenCV_INCLUDE_DIR}
		                   ${OPENGL_INCLUDE_DIR}
                       ${CG_INCLUDE_PATH})

set(DEPENDENT_LIBRARIES ${GLEW_LIBRARY}
		                  	${OpenCV_LIBRARIES}
		                    ${OPENGL_LIBRARIES}
                        ${CG_LIBRARY}
                        ${CG_GL_LIBRARY})

set(DEPENDENT_LINK_DIRECTORIES)
set(CREATED_PACKAGES gpu_klt_flow)
