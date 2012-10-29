include(LibFindMacros)

# Dependencies
#libfind_package(IL IL)

# Use pkg-config to get hints about paths
libfind_pkg_check_modules(IL_PKGCONF IL)

# Include dir
find_path(IL_INCLUDE_DIR
  NAMES IL/il.h
  PATHS ${IL_PKGCONF_INCLUDE_DIRS}
)

# Finally the library itself
find_library(IL_LIBRARY
  NAMES IL
  PATHS ${IL_PKGCONF_LIBRARY_DIRS}
)

# Set the include dir variables and the libraries and let libfind_process do the rest.
# NOTE: Singular variables for this library, plural for libraries this this lib depends on.
set(IL_PROCESS_INCLUDES IL_INCLUDE_DIR IL_INCLUDE_DIRS)
set(IL_PROCESS_LIBS IL_LIBRARY IL_LIBRARIES)
libfind_process(IL)

