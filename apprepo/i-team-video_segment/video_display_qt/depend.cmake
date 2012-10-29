set(DEPENDENT_PACKAGES base assert_log video_framework)

find_package(Qt4 COMPONENTS QtGui QtCore REQUIRED)
include(${QT_USE_FILE})
set(DEPENDENT_INCLUDES
                       )

set(DEPENDENT_LIBRARIES ${QT_LIBRARIES}
                        )

set(DEPENDENT_LINK_DIRECTORIES
                               )

set(CREATED_PACKAGES video_display_qt)
