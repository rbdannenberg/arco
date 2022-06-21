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
cmake_minimum_required(VERSION 3.21) 

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
option(USE_LIBSNDFILE_EXTERNALS "Link with Ogg, Vorbis, and FLAC" FALSE)

# since libraries depend on the USE_STATIC_LIBS option, clear the libraries
#  from the cache
set(SNDFILE_LIB SNDFILE_LIB-NOTFOUND)
set(OGG_LIB OGG_LIB-NOTFOUND) 
set(FLAC_LIB FLAC_LIB-NOTFOUND)
set(VORBIS_LIB VORBIS_LIB-NOTFOUND)
set(VORBISENC_LIB VORBISENC_LIB-NOTFOUND) 

SET(SOURCES src/arco4.cpp src/arco_ui.cpp src/arco_ui.h
            src/cmtio.c src/cmtio.h  
            src/svprefs.cpp src/svprefs.h)  
 
add_executable(${ARCO_SERVER_NAME} ${SOURCES}) 

if(APPLE) 
  set(FRAMEWORK_PATH ${CMAKE_OSX_SYSROOT}/System/Library/Frameworks 
      CACHE INTERNAL "framework path depending on OSX_SYSROOT") 
  target_include_directories(${ARCO_SERVER_NAME} PRIVATE 
      ${CMAKE_OSX_SYSROOT}/Developer/Headers/FlatCarbon) 
  target_link_libraries(${ARCO_SERVER_NAME} PRIVATE 
      # yes, serpent needs core audio for time functions (get nanos) 
      "${FRAMEWORK_PATH}/CoreAudio.framework"
#      "${FRAMEWORK_PATH}/CoreFoundation.framework"
      "${FRAMEWORK_PATH}/Foundation.framework"
      "${FRAMEWORK_PATH}/CoreMIDI.framework"
      "${FRAMEWORK_PATH}/CoreServices.framework") 
endif() 


if(USE_STATIC_LIBS)
  message(STATUS "** Searching for static libs FRIENDS ${FRIENDS}")

  # Ubuntu's libportaudio.a will not link without a Jack library, so
  # in this special case we'll look for the dynamic (.so) library first
  if(UNIX AND NOT APPLE) # Linux
    set(PREFERRED_PORTAUDIO_LIB libportaudio.so)
  endif()
  find_library(PORTAUDIO_LIB
               NAMES ${PREFERRED_PORTAUDIO_LIB}
                     portaudio_static.a portaudio_static 
                     libportaudio.a portaudio
               PATHS "${FRIENDS}/nyquist/Release" 
                     "${FRIENDS}/nyquist/Debug" 
                     "${FRIENDS}/portaudio/lib" 
                     "${FRIENDS}/portaudio/Release" 
                     "${FRIENDS}/portaudio/Debug" 
                     "/usr/lib/x86_64-linux-gnu"
               PATH-SUFFIXES Release Debug)
  message(STATUS "find_library PORTAUDIO_LIB -> ${PORTAUDIO_LIB}")
  # if we found the library locally, use local installation as include 
  #   path: go up two directory levels from _LIB and then down to /include:
  string(REGEX MATCH "/home/" HAS_HOME "${PORTAUDIO_LIB}")
  string(REGEX MATCH "/User/" HAS_USER "${PORTAUDIO_LIB}")
  string(REGEX MATCH "/nyquist/" HAS_NYQ "${PORTAUDIO_LIB}")
  if(HAS_NYQ)
    get_filename_component(PORTAUDIO_INCLUDES ${PORTAUDIO_LIB} DIRECTORY) 
    get_filename_component(PORTAUDIO_INCLUDES ${PORTAUDIO_INCLUDES} 
                                              DIRECTORY) 
    set(PORTAUDIO_INCLUDES "${PORTAUDIO_INCLUDES}/portaudio/include") 
    message(STATUS "found libportaudio in Nyquist, include path 
            ${PORTAUDIO_INCLUDES}")
  elseif(HAS_HOME OR HAS_USER)
    get_filename_component(PORTAUDIO_INCLUDES ${PORTAUDIO_LIB} DIRECTORY) 
    get_filename_component(PORTAUDIO_INCLUDES ${PORTAUDIO_INCLUDES} DIRECTORY) 
    set(PORTAUDIO_INCLUDES "${PORTAUDIO_INCLUDES}/include") 
    message(STATUS "found local libportaudio, include path 
                   ${PORTAUDIO_INCLUDES}")
  elseif("${PORTAUDIO_LIB}" STREQUAL "PORTAUDIO_LIB-NOTFOUND" OR
         "${PORTAUDIO_LIB}" STREQUAL "")
#    message(STATUS "Using find_package to find portaudio")
#    find_package(portaudio REQUIRED)
#    set(PORTAUDIO_INCLUDES ${Portaudio_INCLUDE_DIRS})
    set(PORTAUDIO_LIB "/usr/lib/x86_64-linux-gnu/libportaudio.so.2"
            CACHE FILEPATH "PortAudio library" FORCE)
  endif()

  find_library(SNDFILE_LIB NAMES libsndfile_static.a sndfile_static
                                 libsndfile.a sndfile
               HINTS "${FRIENDS}/libsndfile"
                     "${FRIENDS}/libsndfile/src/.libs"
                     "${FRIENDS}/nyquist/Release"
                     "${FRIENDS}/nyquist/Debug"
                     "/usr/lib/x86_64-linux-gnu"
               PATH_SUFFIXES Release Debug)
  message(STATUS "after find_library, PATHS has ${FRIENDS}/libsndfile, SNDFILE_LIB = ${SNDFILE_LIB}")
  # if we found the library locally, use local installation as include 
  #   path: go up two directory levels from _LIB:
  string(REGEX MATCH "/libsndfile" HAS_LSF "${SNDFILE_LIB}")
  string(REGEX MATCH "/home/" HAS_HOME "${SNDFILE_LIB}")
  string(REGEX MATCH "/User/" HAS_USER "${SNDFILE_LIB}")
  string(REGEX MATCH "/nyquist/" HAS_NYQ "${SNDFILE_LIB}")
  if(HAS_LSF)
    get_filename_component(SNDFILE_INCLUDES ${SNDFILE_LIB} DIRECTORY)
    set(SNDFILE_INCLUDES "${SNDFILE_INCLUDES}/include")
    message(STATUS "found libsndfile in libsndfile, include path
            ${SNDFILE_INCLUDES}")
  elseif(HAS_NYQ)
    get_filename_component(SNDFILE_INCLUDES ${SNDFILE_LIB} DIRECTORY) 
    get_filename_component(SNDFILE_INCLUDES ${SNDFILE_INCLUDES} 
                                                      DIRECTORY) 
    set(SNDFILE_INCLUDES "${SNDFILE_INCLUDES}/nylsf/include") 
    message(STATUS "found libsndfile in Nyquist, include path 
            ${SNDFILE_INCLUDES}")
  elseif(HAS_HOME OR HAS_USER)
    get_filename_component(SNDFILE_INCLUDES ${SNDFILE_LIB} DIRECTORY) 
    get_filename_component(SNDFILE_INCLUDES ${SNDFILE_INCLUDES} DIRECTORY) 
    set(SNDFILE_INCLUDES "${SNDFILE_INCLUDES}") 
    message(STATUS "found libsndfile, include path ${SNDFILE_INCLUDES}") 
  endif()

  set(SNDFILE_LIBS ${SNDFILE_LIB})

  if(USE_LIBSNDFILE_EXTERNALS)
    # libsndfile also uses libogg and libvorbis. For these, we don't need
    #   the include path because they are accessed only indirectly through 
    #   libsndfile
    find_library(OGG_LIB NAMES ogg_static.a ogg_static libogg.a ogg
                 HINTS "${FRIENDS}/ogg/src/.libs" "${FRIENDS}/nyquist"
                       "${FRIENDS}/nyquist/Release"
                       "${FRIENDS}/nyquist/Debug"
                       "/usr/lib/x86_64-linux-gnu"
                 PATH_SUFFIXES Release Debug)
    message(STATUS "OGG_LIB is ${OGG_LIB}")

    find_library(FLAC_LIB NAMES flac_static.a FLAC_static.a 
                                flac_static FLAC_static 
                                libflac.a libFLAC.a
                                flac FLAC
                 HINTS "${FRIENDS}/flac/src/libFLAC/.libs" "${FRIENDS}/nyquist"
                       "/usr/lib/x86_64-linux-gnu"
                 PATH_SUFFIXES Release Debug)
    message(STATUS "FLAC_LIB is ${FLAC_LIB}")

    find_library(VORBIS_LIB NAMES vorbis_static.a vorbis_static vorbis.a vorbis
                 HINTS "${FRIENDS}/vorbis/lib/.libs" "${FRIENDS}/nyquist"
                       "/usr/lib/x86_64-linux-gnu"
                 PATH_SUFFIXES Release Debug)

    find_library(VORBISENC_LIB NAMES vorbisenc_static.a vorbisenc_static 
                                     vorbisenc.a vorbisenc
                 HINTS "${FRIENDS}/vorbis/lib/.libs" "${FRIENDS}/nyquist"
                       "/usr/lib/x86_64-linux-gnu"
                 PATH_SUFFIXES Release Debug)
    message(STATUS "VORBIS_LIB is ${VORBIS_LIB}, 
            VORBISENC_LIB is ${VORBISENC_LIB}")

    list(APPEND SNDFILE_LIBS ${OGG_LIB} ${FLAC_LIB} ${VORBIS_LIB}
                             ${VORBISENC_LIB})
  endif()
else(USE_STATIC_LIBS)
  find_library(PORTAUDIO_LIB NAMES portaudio PATHS /usr/local/lib
                             ${FRIENDS}/portaudio/lib)
  find_library(SNDFILE_LIB NAMES sndfile PATHS
                           /usr/local/lib ${FRIENDS}/libsndfile/src/.libs)
  set(SNDFILE_LIBS ${SNDFILE_LIB})
  message(STATUS "WARNING: the cmake branch for not USE_STATIC_LIBS has not been tested and is probably not going to work")
endif(USE_STATIC_LIBS)

message(STATUS "PORTAUDIO_LIB ${PORTAUDIO_LIB} SNDFILE_LIBS ${SNDFILE_LIBS}")

# add arco -- builds audio and arco libraries
add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/../arco build)

target_include_directories(${ARCO_SERVER_NAME} PRIVATE ${ARCO_INCLUDE_DIRECTORIES})
target_link_libraries(${ARCO_SERVER_NAME} PRIVATE
                      force_arco4lib o2_static form curses)

set(ARCO_DEFINITIONS O2_NO_O2DISCOVERY=1 CACHE STRING
                     "definitions for all compiles" FORCE)
 
target_compile_definitions(${ARCO_SERVER_NAME} PRIVATE ${ARCO_DEFINITIONS}) 

# pick a discovery method (must match how O2 was compiled):

