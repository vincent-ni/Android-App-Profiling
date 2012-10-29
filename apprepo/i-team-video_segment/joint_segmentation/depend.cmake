find_package(OpenCV2 REQUIRED)
find_package(Boost REQUIRED)

set(DEPENDENT_PACKAGES assert_log
                       imagefilter
                       segmentation
                       segment_util
                       sift_lib
                       video_framework)

set(DEPENDENT_INCLUDES ${OpenCV_INCLUDE_DIRS}
                       ${Boost_INCLUDE_DIR}
                       )

set(DEPENDENT_LIBRARIES ${OpenCV_LIBRARIES} 
                        ${Boost_LIBRARIES}
                        )

set(CREATED_PACKAGES joint_segmentation)
