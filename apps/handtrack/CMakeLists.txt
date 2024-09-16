# CMakeList for Digital Audio Serpent (Serpent + Arco)

# This is an example of how to make a daserpent application. The idea
# is that you can make special files to be linked in with all 
# arco code to make a customized binary. You put that together with
# Serpent code to further customize the package.
#
# This file ONLY sets global application-specific settings, then includes
# arco4.cmakeinclude on the last line to do the real work that's common
# to all applications using wxserpent64:
#
# This uses a non-conventional approach to CMake described in
# serpent/64bit/CMakeLists.txt and using macros in
# ../common/arco_cmake_macros.txt, which has more descriptions.

# The version minimum is required by CMake
cmake_minimum_required(VERSION 3.24)

# Name your project here
set(ARCO_TARGET_NAME handtrack)

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
