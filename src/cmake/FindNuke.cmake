#TODO: Mac Support, check NUKE_ already defined via -D NUKE_...?

if (${WIN32})
    file(GLOB NUKE_INSTALL_DIRS "C:/Program Files/Nuke*")
elseif (${UNIX})
    file(GLOB NUKE_INSTALL_DIRS "/usr/local/Nuke*")
endif()

if(NUKE_INSTALL_DIRS)
    list(GET NUKE_INSTALL_DIRS -1 NEWEST_NUKE_INSTALL_DIR)
else()
    set(NEWEST_NUKE_INSTALL_DIR "")
endif()

find_path(NUKE_INCLUDE_DIR
          DDImage/Op.h 
          PATHS $ENV{NDK_PATH} ${NEWEST_NUKE_INSTALL_DIR}
          PATH_SUFFIXES include)

find_library(NUKE_LIBRARY
             DDImage
             PATHS $ENV{NDK_PATH} ${NEWEST_NUKE_INSTALL_DIR})

set(NUKE_LIBRARIES ${NUKE_LIBRARY} )
set(NUKE_INCLUDE_DIRS ${NUKE_INCLUDE_DIR} )

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(Nuke DEFAULT_MSG NUKE_LIBRARY NUKE_INCLUDE_DIR)