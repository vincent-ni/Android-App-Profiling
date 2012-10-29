find_package(BLAS REQUIRED)
find_package(LAPACK REQUIRED)
find_package(OpenCV2 REQUIRED)
find_package(GooglePerfTools REQUIRED)

set(DEPENDENT_INCLUDES ${OpenCV_INCLUDE_DIRS}
                       )

set(DEPENDENT_LIBRARIES ${OpenCV_LIBRARIES}
                        ${BLAS_LIBRARIES}
                        ${LAPACK_LIBRARIES}
                        ${PROFILER_LIBRARIES}
                        )

set(DEPENDENT_PACKAGES flow_lib video_framework)
set(CREATED_PACKAGES irradiance_estimation)
