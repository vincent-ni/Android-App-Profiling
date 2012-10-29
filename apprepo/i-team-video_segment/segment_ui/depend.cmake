find_package(Boost REQUIRED)
find_package(OpenCV2 REQUIRED)

set(DEPENDENT_PACKAGES segment_util
                       video_framework
                       imagefilter)

set(DEPENDENT_INCLUDES ${OpenCV_INCLUDE_DIRS}
                       ${Boost_INCLUDE_DIR}
                       )

set(DEPENDENT_LIBRARIES ${OpenCV_LIBRARIES} 
                        ${Boost_LIBRARIES}
                        )

set(DEPENDENT_LINK_DIRECTORIES)
