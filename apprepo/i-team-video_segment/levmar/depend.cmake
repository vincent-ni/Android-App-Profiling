find_package(MKL REQUIRED)

set(DEPENDENT_INCLUDES ${MKL_INCLUDES}
                       )

set(DEPENDENT_LIBRARIES ${MKL_LIBRARIES})
set(CREATED_PACKAGES levmar)