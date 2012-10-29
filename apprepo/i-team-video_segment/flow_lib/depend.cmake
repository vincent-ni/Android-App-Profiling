find_package(Boost REQUIRED)
find_package(OpenCV2 REQUIRED)
find_package(Protobuf REQUIRED)

set(DEPENDENT_PACKAGES assert_log imagefilter video_framework segment_util
				               )

set(DEPENDENT_INCLUDES ${PROTOBUF_INCLUDE_DIRS}
                       ${OpenCV_INCLUDE_DIRS}
                       ${Boost_INCLUDE_DIR}
                       )

set(DEPENDENT_LIBRARIES ${PROTOBUF_LIBRARIES}
                        ${OpenCV_LIBRARIES}
			 								  ${Boost_LIBRARIES}
                        )
set(DEPENDENT_LINK_DIRECTORIES ${OpenCV_LINK_DIRECTORIES} ${Boost_LIBRARY_DIRS})
set(CREATED_PACKAGES flow_lib)
