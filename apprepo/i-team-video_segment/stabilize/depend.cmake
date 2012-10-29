find_package(MKL REQUIRED)

set(DEPENDENT_INCLUDES ${MKL_INCLUDE_DIR}
                       )

set(DEPENDENT_LIBRARIES ${MKL_LIBRARIES} 
                        )

set(DEPENDENT_PACKAGES flow_lib video_framework)
set(CREATED_PACKAGES stabilization)
