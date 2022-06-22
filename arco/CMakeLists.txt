# Arco4 CMakeLists.txt
#
# Roger B. Dannenberg
# Dec 2021

cmake_minimum_required(VERSION 3.10)

set(ARCO_DEFINITIONS O2_NO_O2DISCOVERY=1 CACHE STRING
                     "definitions for all compiles" FORCE)
 
set(ARCO_SRC
    arcougen.h
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
    upsample.cpp upsample.h 
    ../../ugens/sine.cpp ../../ugens/sine.h 
    ../../ugens/sineb.cpp ../../ugens/sineb.h
)

if(APPLE)
list(APPEND ARCO_SRC recperm.mm recperm.h)
endif()

list(TRANSFORM ARCO_SRC PREPEND "${CMAKE_CURRENT_SOURCE_DIR}/src/")

set(ARCO_INCLUDE_DIRECTORIES 
    "${CMAKE_CURRENT_LIST_DIR}/src"
    "${CMAKE_CURRENT_LIST_DIR}/../../o2/src"
    "${CMAKE_CURRENT_LIST_DIR}/../../portaudio/include"
    "${CMAKE_CURRENT_LIST_DIR}/../../nyquist/nylsf")

# I think the includes and dependencies should be "inherited" when we
# link with this library, but because we have to play tricks to include
# all of the object code in the library, these dependencies do not
# seem to work. Therefore, the user of this library (PARENT) must
# explicitly add these include directories:
set(ARCO_INCLUDE_DIRECTORIES ${ARCO_INCLUDE_DIRECTORIES} PARENT_SCOPE)

if(APPLE)
  find_library(COREAUDIO CoreAudio)
  find_library(COREFOUNDATION CoreFoundation)
  find_library(CORESERVICES CoreServices)
  find_library(AUDIOTOOLBOX AudioToolbox)
  find_library(AUDIOUNIT AudioUnit)
  find_library(AVFOUNDATION AVFoundation)
endif()

set(ARCO_LINK_DIRECTORIES
    "${CMAKE_CURRENT_LIST_DIR}/../../o2"
    "${CMAKE_CURRENT_LIST_DIR}/../../portaudio"
    "${CMAKE_CURRENT_LIST_DIR}/../../nyquist")

set(ARCO_LINK_LIBRARIES
    ${PORTAUDIO_LIB}
    ${SNDFILE_LIBS}
#   "curses" "form" -- removed because it is not found on Linux, but
#                   -- we need to find it and link to it to use the
#                   -- ncurses interface
    "-lm"
    ${COREAUDIO}
    ${COREFOUNDATION}
    ${CORESERVICES}
    ${AUDIOTOOLBOX}
    ${AUDIOUNIT}
    ${AVFOUNDATION})

message(STATUS "arco link with ${ARCO_LINK_LIBRARIES}")

add_library(arco4lib STATIC ${ARCO_SRC} ${ARCO4_APP_SRC})
target_include_directories(arco4lib PRIVATE ${ARCO_INCLUDE_DIRECTORIES})  
message(STATUS "arco library compile definitions ${ARCO_DEFINITIONS}")
target_compile_definitions(arco4lib PRIVATE ${ARCO_DEFINITIONS}) 

# create interface to force load all of arco4lib because of Initializer class
if(UNIX)
  if(APPLE)
    add_library(force_arco4lib INTERFACE)
    target_link_options(force_arco4lib INTERFACE
                        "LINKER:-force_load,$<TARGET_FILE:arco4lib>")
  else()
    # linux is handled through linker options 
    list(APPEND WHOLE_LIBRARIES libarco4lib.a)
    list(APPEND EXTRA_DEPENDENCIES arco4lib)
  endif()
else()
# WINDOWS
  add_library(force_arco4lib INTERFACE)
  target_link_options(force_arco4lib INTERFACE
                      "LINKER:/WHOLEARCHIVE:$<TARGET_FILE:arco4lib>") 
endif()


if(APPLE OR WIN32)
  list(APPEND EXTRA_DEPENDENCIES force_arco4lib)  
  target_link_libraries(force_arco4lib INTERFACE ${ARCO_LINK_LIBRARIES}) 
  target_link_directories(force_arco4lib INTERFACE ${ARCO_LINK_DIRECTORIES}) 
  add_dependencies(force_arco4lib arco4lib)
  list(APPEND EXTRA_DEPENDENCIES force_arco4lib)
else() # linux
  list(APPEND EXTRA_LIBRARIES ${ARCO_LINK_LIBRARIES})
endif()

set(EXTRA_LIBRARIES ${EXTRA_LIBRARIES} PARENT_SCOPE)
set(WHOLE_LIBRARIES ${WHOLE_LIBRARIES} PARENT_SCOPE)
set(EXTRA_DEPENDENCIES ${EXTRA_DEPENDENCIES} PARENT_SCOPE)

message(STATUS 
    "include_directories force_arco4lib ${ARCO_INCLUDE_DIRECTORIES}") 


