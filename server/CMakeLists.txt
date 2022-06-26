# CMakeList for arco server interface

# Structure of the CMake system (PROJ = server instance name)
#
# apps/PROJ/CMakeLists.txt
#     server/arco4.cmakeinclude
#         arco/CMakeLists.txt -- arco sources for arco library
#
# library organization:
#    arco4lib -- arco audio processingm, including unit generators
#    o2_static, portaudio_static, libflac_static, sndfile_static,
#        vorbis_static -- arco depends on these


# The version minimum is required by CMake 
cmake_minimum_required(VERSION 3.24)

include("../apps/common/libraries.txt")  # audio and O2 library prefs

include("../apps/common/arco_cmake_macros.txt")
globals_initialize()

set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR})
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR})

message(STATUS CMAKE_SOURCE_DIR=${CMAKE_SOURCE_DIR} " "
               CMAKE_CURRENT_LIST_DIR=${CMAKE_CURRENT_LIST_DIR})
# "FRIENDS" is where to find dependent software: o2, serpent, liblo, portaudio,
#  etc.  We are in arco4/serpent, so go up two levels:
get_filename_component(FRIENDS ${CMAKE_CURRENT_LIST_DIR} DIRECTORY)
get_filename_component(FRIENDS ${FRIENDS} DIRECTORY)

set(CMAKE_CONFIGURATION_TYPES "Debug;Release" CACHE STRING
              "Semicolon-separated list of supported configuration types"
              FORCE)

set(CMAKE_OSX_ARCHITECTURES x86_64 CACHE STRING "only build forx86_64" FORCE)

option(USE_STATIC_LIBS "Build with static libs?" TRUE)

SET(SOURCES src/arco4.cpp src/arco_ui.cpp src/arco_ui.h
            src/cmtio.c src/cmtio.h  
            src/svprefs.cpp src/svprefs.h)  
 
add_executable(${ARCO_SERVER_NAME} ${SOURCES}) 
register_target(${ARCO_SERVER_NAME})


if(APPLE) 
  set(CURSES_LIB curses)
else()
  set(CURSES_LIB ncurses)
endif() 

# O2 is not in arcolib because it is already in wxslib (if you link
# with wxserpent), but this server version does not use wxserpent,
# so we have to find and add O2 libraries here
append_to_global(ALL_COMPILE_DEFINITIONS "O2_NO_O2DISCOVERY=1") 
if(O2_INCL)
  append_to_global(ALL_INCLUDE_DIRECTORIES "${O2_INCL}") 
endif()  # otherwise, assume portmidi.h is on default include paths
# if no O2_OPT_LIB or O2_DBG_LIB, look for an installed one:
find_library(O2_OPT_LIB NAMES o2_static.a o2.a o2) 
find_library(O2_DBG_LIB NAMES o2_static.a o2.a o2) 
target_link_libraries(${ARCO_SERVER_NAME} PRIVATE debug ${O2_DBG_LIB}
                                                  optimized ${O2_OPT_LIB}) 
if(UNIX AND NOT APPLE)  # linux
  target_link_libraries(${ARCO_SERVER_NAME} PRIVATE
                        avahi-client avahi-common atomic rt)
endif()


# add arco -- builds audio and arco libraries
add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/../arco build)

target_link_whole_library(${ARCO_SERVER_NAME} arco4lib)
target_link_libraries(${ARCO_SERVER_NAME} PRIVATE
                      debug ${O2_DBG_LIB} optimized ${O2_OPT_LIB})
target_link_libraries(${ARCO_SERVER_NAME} PRIVATE form ${CURSES_LIB})

if(UNIX AND NOT APPLE)  # linux
  target_link_libraries(arco4lib PRIVATE avahi-client avahi-common atomic rt) 
endif()

apply_common_settings()
message(STATUS "--------- bottom of server/CMakeLists.txt ---------")
