find_package(Boost REQUIRED)

set(DEPENDENT_PACKAGES assert_log)
set(DEPENDENT_INCLUDES ${Boost_INCLUDE_DIR})
set(DEPENDENT_LIBRARIES ${Boost_LIBRARIES})
set(DEPENDENT_LINK_DIRECTORIES ${Boost_LIBRARY_DIRS})
set(CREATED_PACKAGES base)
