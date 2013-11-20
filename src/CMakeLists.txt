cmake_minimum_required (VERSION 2.6) 

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/cmake/")

set (PROJECT SyLens) 
project (${PROJECT})
 
set(CMAKE_BUILD_TYPE "Release")
set(CMAKE_CONFIGURATION_TYPES "Release")
set(CMAKE_CXX_FLAGS "/DWIN32 /D_WINDOWS /D _USRDLL /W3 /Zm1000 /EHsc /Zi /GL /MP")
set(CMAKE_CXX_FLAGS_RELEASE "/MD /O2 /D NDEBUG")
set(CMAKE_SHARED_LINKER_FLAGS "/machine:x64 /SUBSYSTEM:WINDOWS /OPT:ICF /OPT:REF ")
set(CMAKE_SHARED_LINKER_FLAGS_RELEASE "${CMAKE_SHARED_LINKER_FLAGS_RELEASE} /DEBUG ")

add_library (SyLens SHARED SyLens.cpp)
add_library (SyUV SHARED SyUV.cpp)
add_library (SyCamera SHARED SyCamera.cpp)
add_library (SyShader SHARED SyShader.cpp)
add_library (SyGeo SHARED SyGeo.cpp)

find_package(Nuke REQUIRED)
if (Nuke_FOUND)
  include_directories(${Nuke_INCLUDE_DIR})
  target_link_libraries (SyLens ${Nuke_LIBRARIES})
  target_link_libraries (SyUV ${Nuke_LIBRARIES})
  target_link_libraries (SyCamera ${Nuke_LIBRARIES})
  target_link_libraries (SyShader ${Nuke_LIBRARIES})
  target_link_libraries (SyGeo ${Nuke_LIBRARIES})
endif (Nuke_FOUND)


 

