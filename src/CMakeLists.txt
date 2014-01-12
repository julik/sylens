cmake_minimum_required (VERSION 2.6) 

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/cmake/")

project(SyLens)

set(CMAKE_CONFIGURATION_TYPES Release CACHE STRING "" FORCE)

set(CMAKE_BUILD_TYPE "Release" CACHE STRING "" FORCE)
set(CMAKE_CXX_FLAGS "/DWIN32 /D_WINDOWS /D_USRDLL /W3 /Zm1000 /EHsc /Zi /GL /MP" CACHE STRING "" FORCE)
set(CMAKE_CXX_FLAGS_RELEASE "/MD /O2 /D NDEBUG" CACHE STRING "")
set(CMAKE_SHARED_LINKER_FLAGS "/machine:x64 /SUBSYSTEM:WINDOWS /OPT:ICF /OPT:REF " CACHE STRING "" FORCE)
set(CMAKE_SHARED_LINKER_FLAGS_RELEASE "${CMAKE_SHARED_LINKER_FLAGS_RELEASE} /DEBUG " CACHE STRING "" FORCE)

add_library (SyLens SHARED SyLens.cpp)
add_library (SyUV SHARED SyUV.cpp)
add_library (SyCamera SHARED SyCamera.cpp)
add_library (SyShader SHARED SyShader.cpp)
add_library (SyGeo SHARED SyGeo.cpp)

find_package(Nuke REQUIRED)
include_directories(${NUKE_INCLUDE_DIRS})
target_link_libraries (SyLens ${NUKE_LIBRARIES})
target_link_libraries (SyUV ${NUKE_LIBRARIES})
target_link_libraries (SyCamera ${NUKE_LIBRARIES})
target_link_libraries (SyShader ${NUKE_LIBRARIES})
target_link_libraries (SyGeo ${NUKE_LIBRARIES})
