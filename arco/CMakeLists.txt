# Arco4 CMakeLists.txt
#
# Roger B. Dannenberg
# Dec 2021

cmake_minimum_required(VERSION 3.24)

set(ARCO_SRC
    arcoinit.cpp arcoinit.h
    ugenid.h arcougen.h
    audioio.cpp audioio.h
    ugen.cpp ugen.h
    arcoutil.cpp arcoutil.h
    arcotypes.h
    prefs.cpp prefs.h
    thru.cpp thru.h
    zero.cpp zero.h 
    zerob.cpp zerob.h 
    const.cpp const.h 
    upsample.cpp upsample.h 
    audioblock.cpp audioblock.h

#    pwl.cpp pwl.h 
#    pwlb.cpp pwlb.h
#    delay.cpp delay.h
#    mix.cpp mix.h


    testtone.cpp testtone.h
    # NOTE: list of desired unit generators should be moved to
    # CMakeLists for individual applications so that we do not
    # drag in the whole world (could be large) for every application.
    # But how do we know what unit generators are needed? It could
    # be painful for users to specify, so we should find some way
    # to scan the user's code to determine what unit generators are
    # instantiated. For now, we're focused on a server that has all
    # the code, and to keep things simple, the unit generators are here:


#    ../../ugens/mult/mult.cpp ../../ugens/mult/mult.h 
#    ../../ugens/mult/multb.cpp ../../ugens/mult/multb.h 
#    ../../ugens/sine/sine.cpp ../../ugens/sine/sine.h 
#    ../../ugens/sine/sineb.cpp ../../ugens/sine/sineb.h
#    ../../ugens/reson/reson.cpp ../../ugens/reson/reson.h
#    ../../ugens/reson/resonb.cpp ../../ugens/reson/resonb.h
)

if(APPLE) 
  list(APPEND ARCO_SRC recperm.mm recperm.h)
  # curses library is required by fluidsynth library, which *may* be
  # linked - it depends on dspsources.cmakeinclude, which is generated
  # from the application's dspmanifest.txt file:
  set(CURSES_LIB curses)
else()
  set(CURSES_LIB ncurses)
endif() 

list(TRANSFORM ARCO_SRC PREPEND "${CMAKE_CURRENT_SOURCE_DIR}/src/") 


# find arco base directory
cmake_path(GET CMAKE_CURRENT_LIST_DIR PARENT_PATH ARCO_PATH)
cmake_path(GET ARCO_PATH PARENT_PATH FRIENDS_PATH)

# make dspmakefile and dspsources.cmakeinclude from dspmanifest.txt:
#
message(STATUS "----- executing makedspmakefile.py")  
execute_process(COMMAND python3 ${CMAKE_CURRENT_LIST_DIR}/../preproc/makedspmakefile.py
                    ${ARCO_PATH}
                    ${CMAKE_SOURCE_DIR}/dspmanifest.txt
                    ${CMAKE_SOURCE_DIR}/dspmakefile
                    ${CMAKE_SOURCE_DIR}/dspsources.cmakeinclude)
#
# run the dspmakefile at CMake Configure time to make sure there are
# source files in place when CMake looks to add them to the project.
# To run make properly, we need set the PATH because CMake the app
# does not set up paths the way a shell does in a terminal window.
# Thus, we first make a shell command and put it in the project
# directory, then we run it. We can also set up dependencies
# to run it at build time whenever something changes.
#
file(WRITE "${CMAKE_SOURCE_DIR}/make_faust_ugens.sh"
     "export PATH=$PATH:${FAUST_PATH}\n"
     "make --makefile=dspmakefile\n")
#
# now we run make by running the shell command
#
message(STATUS "----- executing make_faust_ugens.sh")
execute_process(COMMAND sh make_faust_ugens.sh 
                WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
#
# also run make_faust_ugens at build time to make sure source code is generated
add_custom_target(make_faust_ugens ALL
    COMMAND sh make_faust_ugens.sh 
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    DEPENDS ${CMAKE_SOURCE_DIR}/dspmakefile)
#
# now that sources are made, add them here using the cmakeinclude file
# made by makedspmakefile.py called by make_faust_ugens.sh
#

append_to_global(ALL_INCLUDE_DIRECTORIES  
                 "${CMAKE_CURRENT_LIST_DIR}/src")
append_to_global(ALL_INCLUDE_DIRECTORIES  
                 "${CMAKE_CURRENT_LIST_DIR}/../ffts/src")  
append_to_global(ALL_INCLUDE_DIRECTORIES  
                 "${CMAKE_CURRENT_LIST_DIR}/../cmupv/src")  
append_to_global(ALL_INCLUDE_DIRECTORIES "${O2_INCL}")
append_to_global(ALL_INCLUDE_DIRECTORIES "${PA_INCL}")
append_to_global(ALL_INCLUDE_DIRECTORIES "${SNDFILE_INCL}")
append_to_global(ALL_INCLUDE_DIRECTORIES "${FLSYN_INCL}")

#if(APPLE)
#  find_library(COREAUDIO CoreAudio)
#  find_library(FOUNDATION Foundation)
#  find_library(COREFOUNDATION CoreFoundation)
#  find_library(CORESERVICES CoreServices)
#  find_library(AUDIOTOOLBOX AudioToolbox)
#  find_library(AUDIOUNIT AudioUnit)
#  find_library(AVFOUNDATION AVFoundation)
#  find_library(COREMIDI CoreMIDI)
#  find_library(APPKIT AppKit)
#  find_library(CARBON Carbon)
#endif()

add_library(arco4lib STATIC ${ARCO4_APP_SRC})  
include("${CMAKE_SOURCE_DIR}/dspsources.cmakeinclude") 
message(STATUS "ARCO_SRC: ${ARCO_SRC}") 

register_target(arco4lib)
add_dependencies(arco4lib make_faust_ugens) 

#-----------------------------------------------------------------------
#------ libsndfile and externals ---------------------------------------
option(USE_LIBSNDFILE_EXTERNALS "Link with Ogg, Vorbis, and FLAC" FALSE)

if(SNDFILE_INCL)
  append_to_global(ALL_INCLUDE_DIRECTORIES ${SNDFILE_INCL})   
endif()  
find_library(SNDFILE_OPT_LIB 
             NAMES libsndfile_static.a sndfile_static sndfile.a sndfile 
             HINTS "/usr/lib/x86_64-linux-gnu" PATH_SUFFIXES Release Debug)  
find_library(SNDFILE_DBG_LIB 
             NAMES libsndfile_static.a sndfile_static sndfile.a sndfile 
             HINTS "/usr/lib/x86_64-linux-gnu" PATH_SUFFIXES Release Debug)  

if(PA_INCL)
  append_to_global(ALL_INCLUDE_DIRECTORIES ${PA_INCL})  
endif() 
# NOTE:  Ubuntu's libportaudio.a will not link without a Jack library, so  
#        if we do not find our OWN library in portaudio or nyquist,  
#        in this special case we'll look for the dynamic (.so) library  
if(UNIX AND NOT APPLE) # Linux  
  set(PREFERRED_PORTAUDIO_LIB libportaudio.so)  
endif()  
find_library(PA_OPT_LIB NAMES ${PREFERRED_PORTAUDIO_LIB} portaudio_static.a 
                              portaudio_static portaudio.a portaudio
             PATHS "/usr/lib/x86_64-linux-gnu" PATH-SUFFIXES Release Debug)
 
find_library(PA_DBG_LIB NAMES ${PREFERRED_PORTAUDIO_LIB} portaudio_static.a 
                              portaudio_static portaudio.a portaudio
             PATHS "/usr/lib/x86_64-linux-gnu" PATH-SUFFIXES Release Debug)
message(STATUS "PA_OPT_LIB ${PA_OPT_LIB}, PA_DBG_LIB ${PA_DBG_LIB}")


target_link_libraries(arco4lib PRIVATE 
    debug ${PA_DBG_LIB} optimized ${PA_OPT_LIB}
    debug ${SNDFILE_DBG_LIB} optimized ${SNDFILE_OPT_LIB}) 


if(USE_LIBSNDFILE_EXTERNALS)
  # libsndfile also uses libogg and libvorbis. For these, we don't need
  #   the include path because they are accessed only indirectly through 
  #   libsndfile
  if(OGG_INCL) 
      append_to_global(ALL_INCLUDE_DIRECTORIES ${OGG_INCL})  
  endif() 
  find_library(OGG_OPT_LIB NAMES ogg_static.a ogg_static ogg.a ogg 
               HINTS "/usr/lib/x86_64-linux-gnu" PATH_SUFFIXES Release Debug) 
  find_library(OGG_DBG_LIB NAMES ogg_static.a ogg_static ogg.a ogg 
               HINTS "/usr/lib/x86_64-linux-gnu" PATH_SUFFIXES Release Debug) 

  message(STATUS "OGG_OPT_LIB ${OGG_OPT_LIB}, OGG_DBG_LIB ${OGG_DBG_LIB}")      

  if(FLAC_INCL)
      append_to_global(ALL_INCLUDE_DIRECTORIES ${FLAC_INCL})  
  endif() 
  find_library(FLAC_OPT_LIB NAMES flac_static.a FLAC_static.a flac_static 
                                  FLAC_static flac.a FLAC.a flac FLAC
               HINTS "/usr/lib/x86_64-linux-gnu" PATH_SUFFIXES Release Debug) 
  find_library(FLAC_DBG_LIB NAMES flac_static.a FLAC_static.a flac_static 
                                  FLAC_static flac.a FLAC.a flac FLAC
               HINTS "/usr/lib/x86_64-linux-gnu" PATH_SUFFIXES Release Debug) 

  message(STATUS "FLAC_OPT_LIB ${FLAC_OPT_LIB}, FLAC_DBG_LIB ${FLAC_DBG_LIB}") 

  if(VORBIS_INCL)
      append_to_global(ALL_INCLUDE_DIRECTORIES ${VORBIS_INCL})
  endif() 
  find_library(VORBIS_OPT_LIB NAMES vorbis_static.a vorbis_static 
                                    vorbis.a vorbis 
               HINTS "/usr/lib/x86_64-linux-gnu" PATH_SUFFIXES Release Debug) 
  find_library(VORBIS_DBG_LIB NAMES vorbis_static.a vorbis_static 
                                    vorbis.a vorbis 
               HINTS "/usr/lib/x86_64-linux-gnu" PATH_SUFFIXES Release Debug) 
  find_library(VORBISENC_OPT_LIB NAMES vorbisenc_static.a vorbisenc_static 
                                    vorbisenc.a vorbisenc 
               HINTS "/usr/lib/x86_64-linux-gnu" PATH_SUFFIXES Release Debug) 
  find_library(VORBISENC_DBG_LIB NAMES vorbisenc_static.a vorbisenc_static 
                                    vorbisenc.a vorbisenc 
               HINTS "/usr/lib/x86_64-linux-gnu" PATH_SUFFIXES Release Debug) 
  find_library(VORBISFILE_OPT_LIB NAMES vorbisfile_static.a vorbisfile_static 
                                    vorbisfile.a vorbisfile 
               HINTS "/usr/lib/x86_64-linux-gnu" PATH_SUFFIXES Release Debug) 
  find_library(VORBISFILE_DBG_LIB NAMES vorbisfile_static.a vorbisfile_static 
                                    vorbisfile.a vorbisfile 
               HINTS "/usr/lib/x86_64-linux-gnu" PATH_SUFFIXES Release Debug) 

  message(STATUS "VORBIS_OPT_LIB is ${VORBIS_OPT_LIB}, "
                 "VORBIS_DBG_LIB is ${VORBIS_DBG_LIB}") 
  message(STATUS "VORBISENC_OPT_LIB is ${VORBISENC_OPT_LIB}, "
                 "VORBISENC_DBG_LIB is ${VORBISENC_DBG_LIB}") 
  message(STATUS "VORBISFILE_OPT_LIB is ${VORBISFILE_OPT_LIB}, "
                 "VORBISFILE_DBG_LIB is ${VORBISFILE_DBG_LIB}") 

  target_link_libraries(arco4lib PRIVATE
    debug ${OGG_DBG_LIB} optimized ${OGG_OPT_LIB}
    debug ${FLAC_DBG_LIB} optimized ${FLAC_OPT_LIB}
    debug ${VORBIS_DBG_LIB} optimized ${VORBIS_OPT_LIB}
    debug ${VORBISENC_DBG_LIB} optimized ${VORBISFILE_OPT_LIB}
    debug ${VORBISFILE_DBG_LIB} optimized ${VORBISFILE_OPT_LIB})
endif()

# Now we need additional linux libraries:
if(UNIX AND NOT APPLE)  # linux
  append_to_global(ALL_COMPILE_DEFINITIONS "PA_USE_ALSA=1")
  append_to_global(ALL_COMPILE_DEFINITIONS "HAVE_LINUX_SOUNDCARD_H")
  set(EXTRA_PA_LIBS asound pthread m)
endif()

#-------end of libsndfile and externals --------------------------------

message(STATUS "EXTRA_PA_LIBS ${EXTRA_PA_LIBS}")

#target_link_libraries(arco4lib PUBLIC
#    ${EXTRA_PA_LIBS}  # for linux
#    ${COREAUDIO}
#    ${FOUNDATION}
#    ${COREMIDI}
#    ${COREFOUNDATION}
#    ${CORESERVICES}
#    ${AUDIOTOOLBOX}
#    ${AUDIOUNIT}
#    ${AVFOUNDATION}
#    ${APPKIT}
#    ${CARBON}
#    )


