#find_package(ProtoBuf REQUIRED)
find_package(Protobuf REQUIRED)
find_package(OpenCV2 REQUIRED)
find_package(Boost REQUIRED)

set(DEPENDENT_PACKAGES assert_log base)

set(DEPENDENT_INCLUDES ${PROTOBUF_INCLUDE_DIRS}
                       ${OpenCV_INCLUDE_DIRS}
		       ${Boost_INCLUDE_DIR})

set(DEPENDENT_LIBRARIES ${PROTOBUF_LIBRARIES}
                        ${OpenCV_LIBRARIES}
			${Boost_LIBRARIES})

set(CREATED_PACKAGES segment_util)
