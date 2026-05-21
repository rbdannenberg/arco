# benchmarks CMakeLists.txt
#
# Roger B. Dannenberg
# Jan 2025

cmake_minimum_required(VERSION 3.24)

project(benchmarks)

include(../apps/common/libraries.txt)

# from o2/CMakeLists.txt:
if(UNIX)
  if(APPLE)
    set(CMAKE_CXX_FLAGS "-std=c++11")
    find_library(COREAUDIO CoreAudio)
    set(EXTRA_LIBS "${COREAUDIO}")
  else(APPLE) # must be Linux
# struct addrinfo not defined by default, so fix it here:
    add_definitions("-D_POSIX_C_SOURCE=201112L")
    set(EXTRA_LIBS m atomic avahi-client avahi-common rt) # needed by liblo
#   FOR DEBUGGING, I COMPILED MY OWN LOCAL LIBRARY:
#    link_directories(BEFORE avahi-0.7/avahi-common/.libs
#                     avahi-0.7/avahi-core/.libs
#                     avahi-0.7/avahi-utils/.libs
#                     avahi-0.7/avahi-client/.libs)
  endif(APPLE)
else(UNIX)
  add_compile_options("/utf-8")
  message(STATUS "Windows /utf-8 option set.")
endif(UNIX)



add_executable(arco2 arco2.cpp const.h ugen.h)
target_compile_definitions(arco2 PUBLIC O2_NO_O2DISCOVERY) 
target_include_directories(arco2 PUBLIC ${O2_INCL}) 
target_link_libraries(arco2 ${O2_OPT_LIB} ${EXTRA_LIBS}) 

add_executable(arco4 arco4.cpp const.h ugen.h) 
target_compile_definitions(arco4 PUBLIC O2_NO_O2DISCOVERY) 
target_include_directories(arco4 PUBLIC ${O2_INCL}) 
target_link_libraries(arco4 ${O2_OPT_LIB} ${EXTRA_LIBS}) 

add_executable(arco8 arco8.cpp const.h ugen.h) 
target_compile_definitions(arco8 PUBLIC O2_NO_O2DISCOVERY) 
target_include_directories(arco8 PUBLIC ${O2_INCL}) 
target_link_libraries(arco8 ${O2_OPT_LIB} ${EXTRA_LIBS}) 

add_executable(arco16 arco16.cpp const.h ugen.h) 
target_compile_definitions(arco16 PUBLIC O2_NO_O2DISCOVERY) 
target_include_directories(arco16 PUBLIC ${O2_INCL}) 
target_link_libraries(arco16 ${O2_OPT_LIB} ${EXTRA_LIBS}) 

add_executable(arco32 arco32.cpp const.h ugen.h) 
target_compile_definitions(arco32 PUBLIC O2_NO_O2DISCOVERY) 
target_include_directories(arco32 PUBLIC ${O2_INCL}) 
target_link_libraries(arco32 ${O2_OPT_LIB} ${EXTRA_LIBS}) 

add_executable(arco64 arco64.cpp const.h ugen.h) 
target_compile_definitions(arco64 PUBLIC O2_NO_O2DISCOVERY) 
target_include_directories(arco64 PUBLIC ${O2_INCL}) 
target_link_libraries(arco64 ${O2_OPT_LIB} ${EXTRA_LIBS}) 

add_executable(arco128 arco128.cpp const.h ugen.h) 
target_compile_definitions(arco128 PUBLIC O2_NO_O2DISCOVERY) 
target_include_directories(arco128 PUBLIC ${O2_INCL}) 
target_link_libraries(arco128 ${O2_OPT_LIB} ${EXTRA_LIBS}) 

add_executable(arco256 arco256.cpp const.h ugen.h) 
target_compile_definitions(arco256 PUBLIC O2_NO_O2DISCOVERY) 
target_include_directories(arco256 PUBLIC ${O2_INCL}) 
target_link_libraries(arco256 ${O2_OPT_LIB} ${EXTRA_LIBS}) 

add_executable(singlechannel singlechannel.cpp const.h ugen.h) 
target_compile_definitions(singlechannel PUBLIC O2_NO_O2DISCOVERY) 
target_include_directories(singlechannel PUBLIC ${O2_INCL}) 
target_link_libraries(singlechannel ${O2_OPT_LIB} ${EXTRA_LIBS}) 

add_executable(fastchannels fastchannels.cpp const.h ugen.h) 
target_compile_definitions(fastchannels PUBLIC O2_NO_O2DISCOVERY) 
target_include_directories(fastchannels PUBLIC ${O2_INCL}) 
target_link_libraries(fastchannels ${O2_OPT_LIB} ${EXTRA_LIBS}) 

add_executable(noterminate noterminate.cpp const.h ugen.h) 
target_compile_definitions(noterminate PUBLIC O2_NO_O2DISCOVERY) 
target_include_directories(noterminate PUBLIC ${O2_INCL}) 
target_link_libraries(noterminate ${O2_OPT_LIB} ${EXTRA_LIBS}) 

add_executable(nopoly nopoly.cpp const.h ugen.h) 
target_compile_definitions(nopoly PUBLIC O2_NO_O2DISCOVERY) 
target_include_directories(nopoly PUBLIC ${O2_INCL}) 
target_link_libraries(nopoly ${O2_OPT_LIB} ${EXTRA_LIBS}) 

add_executable(block32 block32.cpp const.h ugen.h) 
target_compile_definitions(block32 PUBLIC O2_NO_O2DISCOVERY) 
target_include_directories(block32 PUBLIC ${O2_INCL}) 
target_link_libraries(block32 ${O2_OPT_LIB} ${EXTRA_LIBS}) 

add_executable(block64 block64.cpp const.h ugen.h) 
target_compile_definitions(block64 PUBLIC O2_NO_O2DISCOVERY) 
target_include_directories(block64 PUBLIC ${O2_INCL}) 
target_link_libraries(block64 ${O2_OPT_LIB} ${EXTRA_LIBS}) 

add_executable(block16 block16.cpp const.h ugen.h) 
target_compile_definitions(block16 PUBLIC O2_NO_O2DISCOVERY) 
target_include_directories(block16 PUBLIC ${O2_INCL}) 
target_link_libraries(block16 ${O2_OPT_LIB} ${EXTRA_LIBS}) 

add_executable(block8 block8.cpp const.h ugen.h) 
target_compile_definitions(block8 PUBLIC O2_NO_O2DISCOVERY) 
target_include_directories(block8 PUBLIC ${O2_INCL}) 
target_link_libraries(block8 ${O2_OPT_LIB} ${EXTRA_LIBS}) 

add_executable(block4 block4.cpp const.h ugen.h) 
target_compile_definitions(block4 PUBLIC O2_NO_O2DISCOVERY) 
target_include_directories(block4 PUBLIC ${O2_INCL}) 
target_link_libraries(block4 ${O2_OPT_LIB} ${EXTRA_LIBS}) 

add_executable(block2 block2.cpp const.h ugen.h) 
target_compile_definitions(block2 PUBLIC O2_NO_O2DISCOVERY) 
target_include_directories(block2 PUBLIC ${O2_INCL}) 
target_link_libraries(block2 ${O2_OPT_LIB} ${EXTRA_LIBS}) 

add_executable(block128 block128.cpp const.h ugen.h) 
target_compile_definitions(block128 PUBLIC O2_NO_O2DISCOVERY) 
target_include_directories(block128 PUBLIC ${O2_INCL}) 
target_link_libraries(block128 ${O2_OPT_LIB} ${EXTRA_LIBS}) 

add_executable(block256 block256.cpp const.h ugen.h) 
target_compile_definitions(block256 PUBLIC O2_NO_O2DISCOVERY) 
target_include_directories(block256 PUBLIC ${O2_INCL}) 
target_link_libraries(block256 ${O2_OPT_LIB} ${EXTRA_LIBS}) 

add_executable(noclosure noclosure.cpp const.h ugen.h) 
target_compile_definitions(noclosure PUBLIC O2_NO_O2DISCOVERY) 
target_include_directories(noclosure PUBLIC ${O2_INCL}) 
target_link_libraries(noclosure ${O2_OPT_LIB} ${EXTRA_LIBS}) 

add_executable(singleugen singleugen.cpp const.h ugen.h) 
target_compile_definitions(singleugen PUBLIC O2_NO_O2DISCOVERY) 
target_include_directories(singleugen PUBLIC ${O2_INCL}) 
target_link_libraries(singleugen ${O2_OPT_LIB} ${EXTRA_LIBS}) 

add_executable(fixphase fixphase.cpp const.h ugen.h) 
target_compile_definitions(fixphase PUBLIC O2_NO_O2DISCOVERY) 
target_include_directories(fixphase PUBLIC ${O2_INCL}) 
target_link_libraries(fixphase ${O2_OPT_LIB} ${EXTRA_LIBS}) 

add_executable(writetest writetest.cpp) 
target_compile_definitions(writetest PUBLIC O2_NO_O2DISCOVERY) 
target_include_directories(writetest PUBLIC ${O2_INCL}) 
target_link_libraries(writetest ${O2_OPT_LIB} ${EXTRA_LIBS}) 

add_executable(arcolike2 arcolike2.cpp) 
target_compile_definitions(arcolike2 PUBLIC O2_NO_O2DISCOVERY) 
target_include_directories(arcolike2 PUBLIC ${O2_INCL}) 
target_link_libraries(arcolike2 ${O2_OPT_LIB} ${EXTRA_LIBS}) 

add_executable(arcoopt arcoopt.cpp) 
target_compile_definitions(arcoopt PUBLIC O2_NO_O2DISCOVERY) 
target_include_directories(arcoopt PUBLIC ${O2_INCL}) 
target_link_libraries(arcoopt ${O2_OPT_LIB} ${EXTRA_LIBS}) 

