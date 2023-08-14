# resampletest CMakeLists.txt
#
# Roger B. Dannenberg
# July 2023

cmake_minimum_required(VERSION 3.24)  

project(resamptest)
if(APPLE)
    find_library(COREAUDIO CoreAudio)
endif()

include("${CMAKE_CURRENT_SOURCE_DIR}/../apps/common/libraries.txt")

set(FFTS ${CMAKE_CURRENT_SOURCE_DIR}/../ffts/src)

set(SRC src/resamptest.cpp
        src/resamp.cpp src/resamp.h
        src/cmupv.c src/cmupv.h
        src/internal.c src/internal.h
        ${FFTS}/fftext.c ${FFTS}/fftext.h
        ${FFTS}/fftlib.c ${FFTS}/fftlib.h 
        ${FFTS}/matlib.c ${FFTS}/matlib.h) 

add_executable(resamptest ${SRC})

target_compile_definitions(resamptest PUBLIC "O2_NO_O2DISCOVERY=1")

target_include_directories(resamptest PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/../arco/src/
        ${FFTS}
        ${O2_INCL})

target_link_libraries(resamptest PRIVATE ${COREAUDIO}
        debug ${O2_DBG_LIB} optimized ${O2_OPT_LIB})

