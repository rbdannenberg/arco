# Arco CMakeLists.txt
#
# Roger B. Dannenberg
# Dec 2021

cmake_minimum_required(VERSION 3.24)

# execute python:
set(PYTHON "python3")

execute_process(COMMAND python3 --version
                OUTPUT_VARIABLE pyversion_out
                ERROR_VARIABLE pyerror_out)

# If we get an error, assume python3 does not exist. On Windows, maybe we have python:
if(pyerror_out)
  execute_process(COMMAND python --version
                  OUTPUT_VARIABLE pyversion_out
                  ERROR_VARIABLE pyerror_out ENCODING AUTO)
  string(FIND ${pyversion_out} "Python 3" py3found)
  message(STATUS "python --version ${pyversion_out} error ${pyerror_out} loc ${py3found}")
  if(py3found GREATER_EQUAL 0)
    set(PYTHON "python")
  else()
    message(ERROR " Neither python3 nor python seems to run Python v3.")
  endif()
endif()

message(STATUS "Found ${PYTHON} to run Python v.3.")

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
    multx.cpp multx.h
    const.cpp const.h 
    upsample.cpp upsample.h 
    audioblock.cpp audioblock.h
    testtone.cpp testtone.h
)

if(APPLE) 
  list(APPEND ARCO_SRC recperm.mm recperm.h)
  # curses library is required by fluidsynth library, which *may* be
  # linked - it depends on dspsources.cmakeinclude, which is generated
  # from the application's dspmanifest.txt file:
  set(CURSES_LIB curses)

  # I am debugging with Xcode, and symbols are not included in binary now.
  # Debugging used to work automatically (before 2023). See if this helps.
  # set(CMAKE_XCODE_ATTRIBUTE_GCC_GENERATE_DEBUGGING_SYMBOLS "YES")
  # (The above does not set Generate Debug Symbols to Yes)
  set(CMAKE_CXX_FLAGS -g)
  message(STATUS "set CMAKE_CXX_FLAGS to -g")
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
execute_process(COMMAND ${PYTHON} ${CMAKE_CURRENT_LIST_DIR}/../preproc/makedspmakefile.py
                    ${ARCO_PATH}
                    ${CMAKE_SOURCE_DIR}/dspmanifest.txt
                    ${CMAKE_SOURCE_DIR}/dspmakefile
                    ${CMAKE_SOURCE_DIR}/dspsources.cmakeinclude
                OUTPUT_VARIABLE makedspmakefile_out
                ERROR_VARIABLE makedspmakefile_err)
if(makedspmakefile_out) 
    message(STATUS "makedspmakefile.py output: " ${makedspmakefile_out}) 
endif() 
if(makedspmakefile_err) 
    message(FATAL_ERROR "makedspmakefile.py error output: " ${makedspmakefile_err}) 
endif()


#
# run the dspmakefile at CMake Configure time to make sure there are
# source files in place when CMake looks to add them to the project.
# To run make properly, we need set the PATH because CMake the app
# does not set up paths the way a shell does in a terminal window.
# Thus, we first make a shell command and put it in the project
# directory, then we run it. We can also set up dependencies
# to run it at build time whenever something changes.
#
message(STATUS
        "CMAKE_SOURCE_DIR (where to run dspmakefile): ${CMAKE_SOURCE_DIR}")
if(WIN32)
  add_custom_target(make_faust_ugens ALL
      COMMAND nmake /F dspmakefile
      WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
      DEPENDS ${CMAKE_SOURCE_DIR}/dspmakefile)
else()
  set(MFU_SET_PATH "")
  if(FAUST_PATH)
    set(MFU_SET_PATH "export PATH=$PATH:${FAUST_PATH}\n")
  else()
    message(STATUS "FAUST_PATH not set in apps/common/libraries.txt")
    message(STATUS "   This may result in not finding FAUST compiler when")
    message(STATUS "   running dspmakefile")
  endif()
  file(WRITE "${CMAKE_SOURCE_DIR}/make_faust_ugens.sh"
       "${MFU_SET_PATH}"
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
endif()
#
# now that sources are made, add them here using the cmakeinclude file
# made by makedspmakefile.py called by make_faust_ugens.sh
#

append_to_global(ALL_INCLUDE_DIRECTORIES  
                 "${CMAKE_CURRENT_LIST_DIR}/src")
append_to_global(ALL_INCLUDE_DIRECTORIES
                 "${CMAKE_CURRENT_LIST_DIR}/../ffts/src")
append_to_global(ALL_INCLUDE_DIRECTORIES
                 "${CMAKE_CURRENT_LIST_DIR}/../pffft/")
append_to_global(ALL_INCLUDE_DIRECTORIES  
                 "${CMAKE_CURRENT_LIST_DIR}/../modal/modal/detectionfunctions/")  
append_to_global(ALL_INCLUDE_DIRECTORIES  
                 "${CMAKE_CURRENT_LIST_DIR}/../modal/src/")  
append_to_global(ALL_INCLUDE_DIRECTORIES
                 "${CMAKE_CURRENT_LIST_DIR}/../cmupv/src")
append_to_global(ALL_INCLUDE_DIRECTORIES "${O2_INCL}")
append_to_global(ALL_INCLUDE_DIRECTORIES "${PA_INCL}")
append_to_global(ALL_INCLUDE_DIRECTORIES "${SNDFILE_INCL}")
if(FLSYN_INCL)
  append_to_global(ALL_INCLUDE_DIRECTORIES "${FLSYN_INCL}")
endif()
if(FLSYN_INCL2)
  append_to_global(ALL_INCLUDE_DIRECTORIES "${FLSYN_INCL2}")
endif()
if(MODAL_INCL)
  append_to_global(ALL_INCLUDE_DIRECTORIES "${MODAL_INCL}")
endif()
if(FFTW_INCL)
  append_to_global(ALL_INCLUDE_DIRECTORIES "${FFTW_INCL}")
endif()
if(CHROMAGRAM_INCL)
    append_to_global(ALL_INCLUDE_DIRECTORIES ${CHROMAGRAM_INCL})
endif()

add_library(arcolib STATIC ${ARCO_APP_SRC})

set_target_properties(arcolib PROPERTIES
                      MSVC_RUNTIME_LIBRARY 
                      "MultiThreaded$<$<CONFIG:Debug>:DebugDLL>"
                      WINDOWS_EXPORT_ALL_SYMBOLS TRUE
                      XCODE_ATTRIBUTE_GCC_GENERATE_DEBUGGING_SYMBOLS
                      "$<IF:$<CONFIG:DEBUG>,YES,NO>")
include("${CMAKE_SOURCE_DIR}/dspsources.cmakeinclude") 
message(STATUS "ARCO_SRC: ${ARCO_SRC}") 

register_target(arcolib)
add_dependencies(arcolib make_faust_ugens)

#-----------------------------------------------------------------------
#------ libsndfile and externals ---------------------------------------
option(USE_LIBSNDFILE_EXTERNALS "Link with Ogg, Vorbis, and FLAC" ON)

if(SNDFILE_INCL)
  append_to_global(ALL_INCLUDE_DIRECTORIES ${SNDFILE_INCL})   
endif()

# find_library(SNDFILE_OPT_LIB
#              NAMES libsndfile_static.a sndfile_static sndfile.a sndfile 
#              HINTS "/usr/lib/x86_64-linux-gnu" PATH_SUFFIXES Release Debug)  
# find_library(SNDFILE_DBG_LIB 
#              NAMES libsndfile_static.a sndfile_static sndfile.a sndfile 
#              HINTS "/usr/lib/x86_64-linux-gnu" PATH_SUFFIXES Release Debug)  

# find_library(FFTW_OPT_LIB
#              NAMES libfftw3.a
#              PATH_SUFFIXES Release Debug)
# find_library(FFTW_DBG_LIB
#              NAMES libfftw3.a
#              PATH_SUFFIXES Release Debug)

if(PA_INCL)
  append_to_global(ALL_INCLUDE_DIRECTORIES ${PA_INCL})
endif()
if(FFTW_INCL)
  append_to_global(ALL_INCLUDE_DIRECTORIES ${FFTW_INCL})
endif()
# NOTE:  Ubuntu's libportaudio.a will not link without a Jack library, so
#        if we do not find our OWN library in portaudio or nyquist,  
#        in this special case we'll look for the dynamic (.so) library  
if(UNIX AND NOT APPLE) # Linux  
  set(PREFERRED_PORTAUDIO_LIB libportaudio.so)  
endif()  
# find_library(PA_OPT_LIB NAMES ${PREFERRED_PORTAUDIO_LIB} portaudio_static.a 
#                               portaudio_static portaudio.a portaudio
#              PATHS "/usr/lib/x86_64-linux-gnu" PATH-SUFFIXES Release Debug)
 
# find_library(PA_DBG_LIB NAMES ${PREFERRED_PORTAUDIO_LIB} portaudio_static.a 
#                               portaudio_static portaudio.a portaudio
#              PATHS "/usr/lib/x86_64-linux-gnu" PATH-SUFFIXES Release Debug)
message(STATUS "PA_OPT_LIB ${PA_OPT_LIB}, PA_DBG_LIB ${PA_DBG_LIB}")


target_link_libraries(arcolib PRIVATE 
    debug ${PA_DBG_LIB} optimized ${PA_OPT_LIB}
    debug ${SNDFILE_DBG_LIB} optimized ${SNDFILE_OPT_LIB})


if(USE_LIBSNDFILE_EXTERNALS)
  # libsndfile also uses libogg and libvorbis. For these, we don't need
  #   the include path because they are accessed only indirectly through 
  #   libsndfile
  # if(OGG_INCL) 
  #     append_to_global(ALL_INCLUDE_DIRECTORIES ${OGG_INCL})  
  # endif() 
  # find_library(OGG_OPT_LIB NAMES ogg_static.a ogg_static ogg.a ogg 
  #              HINTS "/usr/lib/x86_64-linux-gnu" PATH_SUFFIXES Release Debug) 
  # find_library(OGG_DBG_LIB NAMES ogg_static.a ogg_static ogg.a ogg 
  #              HINTS "/usr/lib/x86_64-linux-gnu" PATH_SUFFIXES Release Debug) 

  message(STATUS "OGG_OPT_LIB ${OGG_OPT_LIB}, OGG_DBG_LIB ${OGG_DBG_LIB}")      

  # FLAC_INCL has a non-standard assert.h that breaks compilations, so
  # see if we can get away with NOT putting this on the include path -
  # libsndfile is built separately
  # if(FLAC_INCL)
  #     append_to_global(ALL_INCLUDE_DIRECTORIES ${FLAC_INCL})  
  # endif() 
  # find_library(FLAC_OPT_LIB NAMES flac_static.a FLAC_static.a flac_static 
  #                                 FLAC_static flac.a FLAC.a flac FLAC
  #              HINTS "/usr/lib/x86_64-linux-gnu" PATH_SUFFIXES Release Debug) 
  # find_library(FLAC_DBG_LIB NAMES flac_static.a FLAC_static.a flac_static 
  #                                 FLAC_static flac.a FLAC.a flac FLAC
  #              HINTS "/usr/lib/x86_64-linux-gnu" PATH_SUFFIXES Release Debug) 

  message(STATUS "FLAC_OPT_LIB ${FLAC_OPT_LIB}, FLAC_DBG_LIB ${FLAC_DBG_LIB}") 

  # Since FLAC_INCL seems unnecessary, let's eliminate VORBIS_INCL too
  # if(VORBIS_INCL)
  #     append_to_global(ALL_INCLUDE_DIRECTORIES ${VORBIS_INCL})
  # endif() 
  # find_library(VORBIS_OPT_LIB NAMES vorbis_static.a vorbis_static 
  #                                   vorbis.a vorbis 
  #              HINTS "/usr/lib/x86_64-linux-gnu" PATH_SUFFIXES Release Debug) 
  # find_library(VORBIS_DBG_LIB NAMES vorbis_static.a vorbis_static 
  #                                   vorbis.a vorbis 
  #              HINTS "/usr/lib/x86_64-linux-gnu" PATH_SUFFIXES Release Debug) 
  # find_library(VORBISENC_OPT_LIB NAMES vorbisenc_static.a vorbisenc_static 
  #                                   vorbisenc.a vorbisenc 
  #              HINTS "/usr/lib/x86_64-linux-gnu" PATH_SUFFIXES Release Debug) 
  # find_library(VORBISENC_DBG_LIB NAMES vorbisenc_static.a vorbisenc_static 
  #                                   vorbisenc.a vorbisenc 
  #              HINTS "/usr/lib/x86_64-linux-gnu" PATH_SUFFIXES Release Debug) 
  # find_library(VORBISFILE_OPT_LIB NAMES vorbisfile_static.a vorbisfile_static 
  #                                   vorbisfile.a vorbisfile 
  #              HINTS "/usr/lib/x86_64-linux-gnu" PATH_SUFFIXES Release Debug) 
  # find_library(VORBISFILE_DBG_LIB NAMES vorbisfile_static.a vorbisfile_static 
  #                                   vorbisfile.a vorbisfile 
  #              HINTS "/usr/lib/x86_64-linux-gnu" PATH_SUFFIXES Release Debug) 

  message(STATUS "VORBIS_OPT_LIB is ${VORBIS_OPT_LIB}, "
                 "VORBIS_DBG_LIB is ${VORBIS_DBG_LIB}") 
  message(STATUS "VORBISENC_OPT_LIB is ${VORBISENC_OPT_LIB}, "
                 "VORBISENC_DBG_LIB is ${VORBISENC_DBG_LIB}") 
  message(STATUS "VORBISFILE_OPT_LIB is ${VORBISFILE_OPT_LIB}, "
                 "VORBISFILE_DBG_LIB is ${VORBISFILE_DBG_LIB}") 

  message(STATUS "Adding compression libraries (OGG, FLAC, VORBIS) to arcolib")
  target_link_libraries(arcolib PRIVATE
    debug ${OGG_DBG_LIB} optimized ${OGG_OPT_LIB}
    debug ${FLAC_DBG_LIB} optimized ${FLAC_OPT_LIB}
    debug ${VORBIS_DBG_LIB} optimized ${VORBIS_OPT_LIB}
    debug ${VORBISENC_DBG_LIB} optimized ${VORBISENC_OPT_LIB}
    debug ${VORBISFILE_DBG_LIB} optimized ${VORBISFILE_OPT_LIB})
  # I don't think opus is necessarily required by libsndfile, so
  # if undefined, do not try to use it:
  if(OPUS_DBG_LIB)
    target_link_libraries(arcolib PRIVATE
        debug ${OPUS_DBG_LIB} optimized ${OPUS_OPT_LIB})
  else()
    message(STATUS, "OPUS_DBG_LIB undefined, so will not link with Opus libs")
  endif()
endif()

# Now we need additional linux libraries:
if(UNIX AND NOT APPLE)  # linux
  append_to_global(ALL_COMPILE_DEFINITIONS "PA_USE_ALSA=1")
  append_to_global(ALL_COMPILE_DEFINITIONS "HAVE_LINUX_SOUNDCARD_H")
  set(EXTRA_PA_LIBS asound pthread m)
endif()

#-------end of libsndfile and externals --------------------------------

message(STATUS "EXTRA_PA_LIBS ${EXTRA_PA_LIBS}")

