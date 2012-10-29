find_package(Boost REQUIRED)
find_package(BLAS REQUIRED)
find_package(LAPACK REQUIRED)
find_package(OpenCV2 REQUIRED)

set(DEPENDENT_PACKAGES base
                       video_framework
			 								 imagefilter 
											 flow_lib)

set(DEPENDENT_LIBRARIES ${Boost_LIBRARIES}
                        ${BLAS_LIBRARIES}
                        ${LAPACK_LIBRARIES}
                        ${OpenCV_LIBRARIES}
												)

set (DEPENDENT_INCLUDES
                        ${OpenCV_INCLUDE_DIRS}
                        ${Boost_INCLUDE_DIR})

set(DEPENDENT_LINK_DIRECTORIES)
