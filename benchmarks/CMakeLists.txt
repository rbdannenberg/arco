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



add_executable(arcolike arcolike.cpp) 
target_compile_definitions(arcolike PUBLIC O2_NO_O2DISCOVERY) 
target_include_directories(arcolike PUBLIC ${O2_INCL}) 
target_link_libraries(arcolike ${O2_OPT_LIB} ${EXTRA_LIBS}) 


add_executable(arcolikecnt arcolikecnt.cpp) 
target_compile_definitions(arcolikecnt PUBLIC O2_NO_O2DISCOVERY) 
target_include_directories(arcolikecnt PUBLIC ${O2_INCL}) 
target_link_libraries(arcolikecnt ${O2_OPT_LIB} ${EXTRA_LIBS}) 


add_executable(singlechannel singlechannel.cpp) 
target_compile_definitions(singlechannel PUBLIC O2_NO_O2DISCOVERY) 
target_include_directories(singlechannel PUBLIC ${O2_INCL}) 
target_link_libraries(singlechannel ${O2_OPT_LIB} ${EXTRA_LIBS}) 


add_executable(fastchannels fastchannels.cpp) 
target_compile_definitions(fastchannels PUBLIC O2_NO_O2DISCOVERY) 
target_include_directories(fastchannels PUBLIC ${O2_INCL}) 
target_link_libraries(fastchannels ${O2_OPT_LIB} ${EXTRA_LIBS}) 


add_executable(noterminate noterminate.cpp) 
target_compile_definitions(noterminate PUBLIC O2_NO_O2DISCOVERY) 
target_include_directories(noterminate PUBLIC ${O2_INCL}) 
target_link_libraries(noterminate ${O2_OPT_LIB} ${EXTRA_LIBS}) 

add_executable(nopoly nopoly.cpp) 
target_compile_definitions(nopoly PUBLIC O2_NO_O2DISCOVERY) 
target_include_directories(nopoly PUBLIC ${O2_INCL}) 
target_link_libraries(nopoly ${O2_OPT_LIB} ${EXTRA_LIBS}) 

add_executable(allaudio allaudio.cpp) 
target_compile_definitions(allaudio PUBLIC O2_NO_O2DISCOVERY) 
target_include_directories(allaudio PUBLIC ${O2_INCL}) 
target_link_libraries(allaudio ${O2_OPT_LIB} ${EXTRA_LIBS}) 

add_executable(block64 block64.cpp) 
target_compile_definitions(block64 PUBLIC O2_NO_O2DISCOVERY) 
target_include_directories(block64 PUBLIC ${O2_INCL}) 
target_link_libraries(block64 ${O2_OPT_LIB} ${EXTRA_LIBS}) 

add_executable(block16 block16.cpp) 
target_compile_definitions(block16 PUBLIC O2_NO_O2DISCOVERY) 
target_include_directories(block16 PUBLIC ${O2_INCL}) 
target_link_libraries(block16 ${O2_OPT_LIB} ${EXTRA_LIBS}) 

add_executable(block8 block8.cpp) 
target_compile_definitions(block8 PUBLIC O2_NO_O2DISCOVERY) 
target_include_directories(block8 PUBLIC ${O2_INCL}) 
target_link_libraries(block8 ${O2_OPT_LIB} ${EXTRA_LIBS}) 

add_executable(block4 block4.cpp) 
target_compile_definitions(block4 PUBLIC O2_NO_O2DISCOVERY) 
target_include_directories(block4 PUBLIC ${O2_INCL}) 
target_link_libraries(block4 ${O2_OPT_LIB} ${EXTRA_LIBS}) 

add_executable(block2 block2.cpp) 
target_compile_definitions(block2 PUBLIC O2_NO_O2DISCOVERY) 
target_include_directories(block2 PUBLIC ${O2_INCL}) 
target_link_libraries(block2 ${O2_OPT_LIB} ${EXTRA_LIBS}) 

add_executable(block128 block128.cpp) 
target_compile_definitions(block128 PUBLIC O2_NO_O2DISCOVERY) 
target_include_directories(block128 PUBLIC ${O2_INCL}) 
target_link_libraries(block128 ${O2_OPT_LIB} ${EXTRA_LIBS}) 

add_executable(block256 block256.cpp) 
target_compile_definitions(block256 PUBLIC O2_NO_O2DISCOVERY) 
target_include_directories(block256 PUBLIC ${O2_INCL}) 
target_link_libraries(block256 ${O2_OPT_LIB} ${EXTRA_LIBS}) 

add_executable(block128cnt block128cnt.cpp) 
target_compile_definitions(block128cnt PUBLIC O2_NO_O2DISCOVERY) 
target_include_directories(block128cnt PUBLIC ${O2_INCL}) 
target_link_libraries(block128cnt ${O2_OPT_LIB} ${EXTRA_LIBS}) 

add_executable(block2cnt block2cnt.cpp) 
target_compile_definitions(block2cnt PUBLIC O2_NO_O2DISCOVERY) 
target_include_directories(block2cnt PUBLIC ${O2_INCL}) 
target_link_libraries(block2cnt ${O2_OPT_LIB} ${EXTRA_LIBS}) 

add_executable(noclosure noclosure.cpp) 
target_compile_definitions(noclosure PUBLIC O2_NO_O2DISCOVERY) 
target_include_directories(noclosure PUBLIC ${O2_INCL}) 
target_link_libraries(noclosure ${O2_OPT_LIB} ${EXTRA_LIBS}) 

add_executable(arcolike16 arcolike16.cpp) 
target_compile_definitions(arcolike16 PUBLIC O2_NO_O2DISCOVERY) 
target_include_directories(arcolike16 PUBLIC ${O2_INCL}) 
target_link_libraries(arcolike16 ${O2_OPT_LIB} ${EXTRA_LIBS}) 
