# Arco3 CMakeLists.txt
#
# Roger B. Dannenberg
# Dec 2021

cmake_minimum_required(VERSION 3.10)

project(arco3 VERSION 1.0)

# add_executable(iotest
#     src/iotest.cpp 
#     src/cmtio.c 
# )

set(SRC
    audioio.cpp audioio.h
    ugen.cpp ugen.h
    arcoutil.cpp arcoutil.h
    arcotypes.h
    prefs.cpp prefs.h
    thru.cpp thru.h
    zero.cpp zero.h 
    zerob.cpp zerob.h 
    const.cpp const.h 
    pwl.cpp pwl.h 
    pwlb.cpp pwlb.h 
    mix.cpp mix.h 
    testtone.cpp testtone.h
    # NOTE: list of desired unit generators should be moved to
    # CMakeLists for individual applications so that we do not
    # drag in the whole world (could be large) for every application.
    # But how do we know what unit generators are needed? It could
    # be painful for users to specify, so we should find some way
    # to scan the user's code to determine what unit generators are
    # instantiated. For now, we're focused on a server that has all
    # the code, and to keep things simple, the unit generators are here:
    ../../ugens/mult.cpp ../../ugens/mult.h 
    ../../ugens/multb.cpp ../../ugens/multb.h 
    recperm.mm recperm.h
    upsample.cpp upsample.h 
    ../../ugens/sine.cpp ../../ugens/sine.h 
    ../../ugens/sineb.cpp ../../ugens/sineb.h
)

list(TRANSFORM SRC PREPEND "${CMAKE_CURRENT_SOURCE_DIR}/src/"
               OUTPUT_VARIABLE FULL)
set(ARCO4_SRC ${FULL} PARENT_SCOPE)

set(ARCO_INCLUDE_DIRECTORIES 
    "${CMAKE_CURRENT_LIST_DIR}/src"
    "${CMAKE_CURRENT_LIST_DIR}/../../o2/src"
    "${CMAKE_CURRENT_LIST_DIR}/../../portaudio/include"
    "${CMAKE_CURRENT_LIST_DIR}/../../nyquist/nylsf"
    PARENT_SCOPE)

find_library(COREAUDIO CoreAudio)   
find_library(COREFOUNDATION CoreFoundation) 
find_library(CORESERVICES CoreServices)   
find_library(AUDIOTOOLBOX AudioToolbox)  
find_library(AUDIOUNIT AudioUnit) 
find_library(AVFOUNDATION AVFoundation)

set(ARCO_LINK_DIRECTORIES
    "${CMAKE_CURRENT_LIST_DIR}/../../o2"
    "${CMAKE_CURRENT_LIST_DIR}/../../portaudio"
    "${CMAKE_CURRENT_LIST_DIR}/../../nyquist"
    PARENT_SCOPE)

set(ARCO_LINK_LIBRARIES
    "o2_static.a"
    "portaudio_static"
    "libflac_static.a"
    "sndfile_static"
    "vorbis_static"
    "curses"
    "form"
    "m"
    ${COREAUDIO}
    ${COREFOUNDATION}
    ${CORESERVICES}
    ${AUDIOTOOLBOX}
    ${AUDIOUNIT}
    ${AVFOUNDATION} 
    PARENT_SCOPE)

set(ARCO_DEFINITIONS O2_NO_O2DISCOVERY)

