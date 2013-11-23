#TODO:
# - Linux/Mac Support
# - Find newest version
# - Let user specify version to use
# - Find path in environment variable NDK_PATH, NUKE_PATH

file(GLOB NUKE_DIR "C:/Program Files/Nuke*")

find_path(NUKE_INCLUDE_DIR DDImage/Op.h HINTS ${NUKE_DIR}/include)

find_library(NUKE_LIBRARY DDImage HINTS ${NUKE_DIR})
  
set(NUKE_LIBRARIES ${NUKE_LIBRARY} )
set(NUKE_INCLUDE_DIRS ${NUKE_INCLUDE_DIR})

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(Nuke DEFAULT_MSG NUKE_LIBRARY NUKE_INCLUDE_DIR)