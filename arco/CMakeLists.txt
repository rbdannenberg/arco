# Arco4 CMakeLists.txt
#
# Roger B. Dannenberg
# Dec 2021

cmake_minimum_required(VERSION 3.24)

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

append_to_global(ALL_INCLUDE_DIRECTORIES
                 "${CMAKE_CURRENT_LIST_DIR}/src")
append_to_global(ALL_INCLUDE_DIRECTORIES
                 "${CMAKE_CURRENT_LIST_DIR}/../../o2/src")
append_to_global(ALL_INCLUDE_DIRECTORIES
                 "${CMAKE_CURRENT_LIST_DIR}/../../portaudio/include")
append_to_global(ALL_INCLUDE_DIRECTORIES
                 "${CMAKE_CURRENT_LIST_DIR}/../../nyquist/nylsf")

if(APPLE)
  find_library(COREAUDIO CoreAudio)
  find_library(FOUNDATION Foundation)
  find_library(COREFOUNDATION CoreFoundation)
  find_library(CORESERVICES CoreServices)
  find_library(AUDIOTOOLBOX AudioToolbox)
  find_library(AUDIOUNIT AudioUnit)
  find_library(AVFOUNDATION AVFoundation)
  find_library(COREMIDI CoreMIDI)
endif()

append_to_global(ALL_LINK_DIRECTORIES
                 "${CMAKE_CURRENT_LIST_DIR}/../../o2")
append_to_global(ALL_LINK_DIRECTORIES
                 "${CMAKE_CURRENT_LIST_DIR}/../../portaudio")
append_to_global(ALL_LINK_DIRECTORIES
                 "${CMAKE_CURRENT_LIST_DIR}/../../nyquist")

add_library(arco4lib STATIC ${ARCO_SRC} ${ARCO4_APP_SRC})
register_target(arco4lib)

#-----------------------------------------------------------------------
#------ libsndfile and externals ---------------------------------------
option(USE_LIBSNDFILE_EXTERNALS "Link with Ogg, Vorbis, and FLAC" FALSE)

# since libraries depend on the USE_STATIC_LIBS option, clear the libraries
#  from the cache
set(SNDFILE_LIB SNDFILE_LIB-NOTFOUND)
set(OGG_LIB OGG_LIB-NOTFOUND) 
set(FLAC_LIB FLAC_LIB-NOTFOUND)
set(VORBIS_LIB VORBIS_LIB-NOTFOUND)
set(VORBISENC_LIB VORBISENC_LIB-NOTFOUND) 

if(USE_STATIC_LIBS)
  message(STATUS "** Searching for static libs FRIENDS ${FRIENDS}")

  # Ubuntu's libportaudio.a will not link without a Jack library, so
  # if we do not find our OWN library in portaudio or nyquist,
  # in this special case we'll look for the dynamic (.so) library
  if(UNIX AND NOT APPLE) # Linux
    set(PREFERRED_PORTAUDIO_LIB libportaudio.so)
    set(CURSES_LIB ncurses)
  else()
    set(CURSES_LIB curses)
  endif()
  find_library(PORTAUDIO_LIB
               NAMES portaudio_static.a portaudio_static 
                     libportaudio.a portaudio
               PATHS "${FRIENDS}/portaudio/lib"
                     "${FRIENDS}/portaudio"
                     "${FRIENDS}/nyquist" 
               PATH-SUFFIXES Release Debug)
  # fallback position: use system library
  find_library(PORTAUDIO_LIB
               NAMES ${PREFERRED_PORTAUDIO_LIB}
                     portaudio_static.a portaudio_static 
                     libportaudio.a portaudio
               PATHS "/usr/lib/x86_64-linux-gnu"
               PATH-SUFFIXES Release Debug)
  message(STATUS "find_library PORTAUDIO_LIB -> ${PORTAUDIO_LIB}")
  # if we found the library locally, use local installation as include 
  # path. Choices are:
  #   ${FRIENDS}/portaudio/ - assume ${FRIENDS}/portaudio/include
  #   /nyquist/ - otherwise, assume directory is /nyquist/portaudio/include
  string(REGEX MATCH "${FRIENDS}/portaudio/" HAS_FRIEND "${PORTAUDIO_LIB}")
  string(REGEX MATCH "/nyquist/" HAS_NYQ "${PORTAUDIO_LIB}")
  if(HAS_FRIEND)
    # strip off libportaudio.a or whatever the library name is:
    get_filename_component(PA_LIB_DIR ${PORTAUDIO_LIB} DIRECTORY) 
    # get the directory ending in "/portaudio/":
    string(FIND "${PA_LIB_DIR}/" "/portaudio/" LOC REVERSE)
    math(EXPR ENDLOC "${LOC} + 10")
    # continue below after "else()"
  elseif(HAS_NYQ)
    string(FIND "${PORTAUDIO_LIB}" "/nyquist/" LOC REVERSE)
    math(EXPR ENDLOC "${LOC} + 8")
    # continue below after "else()"
  endif()
  if("${PORTAUDIO_LIB}" STREQUAL "PORTAUDIO_LIB-NOTFOUND" OR
         "${PORTAUDIO_LIB}" STREQUAL "")
    message(STATUS "Did not find portaudio: assuming /usr/lib/...")
    set(PORTAUDIO_LIB "/usr/lib/x86_64-linux-gnu/libportaudio.so.2"
            CACHE FILEPATH "PortAudio library" FORCE)
    # should we add something to ALL_INCLUDE_DIRECTORIES?
  else()  # both HAS_NYQ and HAS_FRIENDS cases:
    string(SUBSTRING "${PORTAUDIO_LIB}" 0 ${ENDLOC} PA_BASE)
    set(PA_INCLUDE "${PA_BASE}/portaudio/include")
    message(STATUS "found the portaudio lib, include path ${PA_INCLUDE}")
    append_to_global(ALL_INCLUDE_DIRECTORIES ${PA_INCLUDE})
  endif()
  find_library(SNDFILE_LIB NAMES libsndfile_static.a sndfile_static
                                 libsndfile.a sndfile
               HINTS "${FRIENDS}/libsndfile"
                     "${FRIENDS}/libsndfile/src/.libs"
                     "${FRIENDS}/nyquist/Release"
                     "${FRIENDS}/nyquist/Debug"
                     "/usr/lib/x86_64-linux-gnu"
               PATH_SUFFIXES Release Debug)
  message(STATUS "after find_library, PATHS has "
                 "    ${FRIENDS}/libsndfile, SNDFILE_LIB = ${SNDFILE_LIB}")
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

# Now we need additional linux libraries:
if(UNIX AND NOT APPLE)  # linux
  append_to_global(ALL_COMPILE_DEFINITIONS "PA_USE_ALSA=1")
  append_to_global(ALL_COMPILE_DEFINITIONS "HAVE_LINUX_SOUNDCARD_H")
  set(EXTRA_PA_LIBS asound pthread m)
endif()


message(STATUS "PORTAUDIO_LIB ${PORTAUDIO_LIB}")
message(STATUS "SNDFILE_LIBS ${SNDFILE_LIBS}")


#-------end of libsndfile and externals --------------------------------

message(STATUS "EXTRA_PA_LIBS ${EXTRA_PA_LIBS}")

target_link_libraries(arco4lib PRIVATE
    ${PORTAUDIO_LIB}
    ${SNDFILE_LIBS}
#   "curses" "form" -- removed because it is not found on Linux, but
#                   -- we need to find it and link to it to use the
#                   -- ncurses interface
#    "-lm"
    ${EXTRA_PA_LIBS}  # for linux
    ${COREAUDIO}
    ${FOUNDATION}
    ${COREMIDI}
    ${COREFOUNDATION}
    ${CORESERVICES}
    ${AUDIOTOOLBOX}
    ${AUDIOUNIT}
    ${AVFOUNDATION})


