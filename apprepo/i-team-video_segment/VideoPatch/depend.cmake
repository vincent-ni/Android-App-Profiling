find_package(Qt4 COMPONENTS QtCore QtGui REQUIRED)
find_package(Boost REQUIRED)

set(DEPENDENT_PACKAGES )

set(DEPENDENT_LIBRARIES ${QT_LIBRARIES}
                        ${Boost_LIBRARIES})                      
set(DEPENDENT_LINK_DIRECTORIES)
