find_package(Boost REQUIRED)
find_package(OpenCV2 REQUIRED)

set(DEPENDENT_PACKAGES assert_log gpu-sift video_framework)

set(DEPENDENT_INCLUDES ${OpenCV_INCLUDE_DIRS}
                       ${Boost_INCLUDE_DIR})

set(DEPENDENT_LIBRARIES ${OpenCV_LIBRARIES}
			 								  ${Boost_LIBRARIES}
       	                )

set(DEPENDENT_LINK_DIRECTORIES ${OpenCV_LINK_DIRECTORIES}
                               ${Boost_LIBRARY_DIRS}
                               )
set(CREATED_PACKAGES sift_lib)
