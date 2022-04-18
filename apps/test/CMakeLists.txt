# CMakeList for Digital Audio Serpent (Serpent + Arco)

# This is an example of how to make an daserpent application. The idea
# is that you can make special files to be linked in with all 
# arco code to make a customized binary. You put that together with
# Serpent code to further customizes the package.

# The version minimum is required by CMake
cmake_minimum_required(VERSION 3.21)

# Name your project here
set(ARCO_TARGET_NAME daserpent)

# CMake requires this as a top-level command. Don't change it.
PROJECT(${ARCO_TARGET_NAME})

# Add custom files -- .cpp and .h -- to be compiled and included
set(CUSTOM_ARCO )
# icon file must be named daserpent.icns, but you can specify where to get it,
#   it must end in a slash so we can prepend it to wxserpent.icns
set(WXSICON_DIRECTORY "${CMAKE_SOURCE_DIR}/")

# This command pulls in all the greatness of Arco. The file
# parameter is in the path to arco3.
include(../../serpent/arco4.cmakeinclude)
