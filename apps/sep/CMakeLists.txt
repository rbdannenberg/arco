# CMakeList for Separation Logic (Serpent + Arco)
#
# Based on arco/apps/test/CMakeLists.txt
#
# Roger B. Dannenberg  
# Oct 2023  

# The version minimum is required by CMake
cmake_minimum_required(VERSION 3.24)

# Name your project here
set(ARCO_TARGET_NAME sep)

# Add custom files -- .cpp and .h -- to be compiled and included
set(CUSTOM_ARCO )

# icon file must be named daserpent.icns, but you can specify where to get it,
#   it must end in a slash so we can prepend it to wxserpent.icns
set(WXSICON_DIRECTORY "${CMAKE_SOURCE_DIR}/")

# CMake requires this as a top-level command. Don't change it.
PROJECT(${ARCO_TARGET_NAME})

# This command pulls in all the greatness of Arco (assumes we are in
# <arco-root>/apps/<this-app-name>/):
include(../../serpent/arco.cmakeinclude)
