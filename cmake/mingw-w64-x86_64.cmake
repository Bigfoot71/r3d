# Sample toolchain file for building for Windows from a Linux system.
#
# Typical usage:
#    On Ubuntu/Debian:
#       *) install cross compiler: `sudo apt-get install mingw-w64`
#    On Fedora:
#       *) install cross compiler: `sudo dnf install mingw64-gcc mingw64-gcc-c++`
#    *) cd build
#    *) cmake -DCMAKE_TOOLCHAIN_FILE=cmake/mingw-w64-x86_64.cmake ..
# This is free and unencumbered software released into the public domain.

set(CMAKE_SYSTEM_NAME Windows)
set(TOOLCHAIN_PREFIX x86_64-w64-mingw32)

# cross compilers to use for C, C++
set(CMAKE_C_COMPILER ${TOOLCHAIN_PREFIX}-gcc)
set(CMAKE_CXX_COMPILER ${TOOLCHAIN_PREFIX}-g++)
set(CMAKE_RC_COMPILER ${TOOLCHAIN_PREFIX}-windres)

# target environment on the build host system
set(CMAKE_FIND_ROOT_PATH /usr/${TOOLCHAIN_PREFIX})

# modify default behavior of FIND_XXX() commands
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
