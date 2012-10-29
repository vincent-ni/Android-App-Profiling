find_package(Boost COMPONENTS filesystem system program_options thread REQUIRED)
find_package(OpenCV2 REQUIRED)
find_package(FFMPEG REQUIRED)
find_package(ZLIB REQUIRED)

set(DEPENDENT_PACKAGES assert_log base imagefilter)

set(DEPENDENT_INCLUDES ${OpenCV_INCLUDE_DIRS}
                       ${Boost_INCLUDE_DIR}
								       ${FFMPEG_INCLUDE_DIR})

set(DEPENDENT_LIBRARIES ${OpenCV_LIBRARIES}
                        ${Boost_LIBRARIES}
                        ${FFMPEG_LIBRARIES}
												${ZLIB_LIBRARIES}
)
set(DEPENDENT_LINK_DIRECTORIES ${OpenCV_LINK_DIRECTORIES}
                               ${Boost_LIBRARY_DIRS}
                               ${FFMPEG_LIBRARY_DIR})
set(CREATED_PACKAGES video_framework)
