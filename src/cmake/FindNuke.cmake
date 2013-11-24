#TODO:
# - Linux/Mac Support

file(GLOB NUKE_INSTALL_DIRS "C:/Program Files/Nuke*")

message(STATUS "NDK_PATH = " $ENV{NDK_PATH})
list(GET NUKE_INSTALL_DIRS -1 NEWEST_NUKE_INSTALL_DIR)
message(STATUS "NEWEST_NUKE_INSTALL_DIR = " ${NEWEST_NUKE_INSTALL_DIR})

find_path(NUKE_INCLUDE_DIR
		  DDImage/Op.h 
		  PATHS $ENV{NDK_PATH} ${NEWEST_NUKE_INSTALL_DIR}
		  PATH_SUFFIXES include)

find_library(NUKE_LIBRARY
			 DDImage
			 PATHS $ENV{NDK_PATH} ${NEWEST_NUKE_INSTALL_DIR})
  
include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(Nuke DEFAULT_MSG NUKE_LIBRARY NUKE_INCLUDE_DIR)