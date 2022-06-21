# CMakeList for Arco Server with curses interface

# This is an example of how to make a command-line arco server
# application.  The idea is that you can make special files to be
# linked in with all arco code to make a customized binary. You put
# that together with the curses interface to make an runable server.

# The version minimum is required by CMake 
cmake_minimum_required(VERSION 3.21) 

# Name your project here
set(ARCO_SERVER_NAME arcobasic)

# CMake requires this as a top-level command. Don't change it.
PROJECT(${ARCO_SERVER_NAME})

# Add custom files -- .cpp and .h -- to be compiled and included
set(CUSTOM_ARCO )

# This command pulls in all the greatness of Arco.
add_subdirectory(../../server arco-build)
