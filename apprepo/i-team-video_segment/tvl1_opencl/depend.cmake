find_package(OpenCV2 REQUIRED)
find_package(Boost REQUIRED)

set(DEPENDENT_PACKAGES video_framework)

set(DEPENDENT_INCLUDES ${OpenCV_INCLUDE_DIRS}
                       ${OpenCV_INCLUDE_DIR}
                       ${Boost_INCLUDE_DIR}
                       )

set(DEPENDENT_LIBRARIES ${OpenCV_LIBRARIES} 
                        ${OpenCV_LIBS} 
                        ${Boost_LIBRARIES}
                        )


set(CREATED_PACKAGES tvl1_opencl)
