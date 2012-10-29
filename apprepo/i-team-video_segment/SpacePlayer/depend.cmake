find_package(Boost REQUIRED)
find_package(MKL REQUIRED)
find_package(ZLIB REQUIRED)

set(DEPENDENT_PACKAGES assert_log
                       video_framework
			 								 imagefilter 
											 graphcut
											 levmar
											 flow_lib
											 segment_util
                       segmentation)
set(DEPENDENT_LIBRARIES ${Boost_LIBRARIES}
            		        ${MKL_LIBRARIES}
                        ${ZLIB_LIBRARIES}
)

set (DEPENDENT_INCLUDES ${MKL_INCLUDES})
                        
set(DEPENDENT_LINK_DIRECTORIES)
