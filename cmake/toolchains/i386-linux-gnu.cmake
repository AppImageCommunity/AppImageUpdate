# this toolchain file works for cross compiling on Ubuntu when the following prerequisites are given:
# - all dependencies that would be needed for a normal build must be installed as i386 versions
# - building XZ/liblzma doesn't work yet, so one has to install liblzma-dev:i386 and set -DUSE_SYSTEM_XZ=ON
# - building GTest doesn't work yet, so one has to install libgtest-dev:i386 and set -DUSE_SYSTEM_GTEST=ON
# - building libarchive doesn't work yet, so one has to install liblzma-dev:i386 and set -DUSE_SYSTEM_LIBARCHIVE=ON (TODO: link system libarchive statically like liblzma)
# some of the packets interfere with their x86_64 version (e.g., libfuse-dev:i386, libglib2-dev:i386), so building on a
# normal system will most likely not work, but on systems like Travis it should work fine

set(CMAKE_SYSTEM_NAME Linux CACHE STRING "" FORCE)
set(CMAKE_SYSTEM_PROCESSOR i386 CACHE STRING "" FORCE)

set(CMAKE_C_FLAGS "-m32" CACHE STRING "" FORCE)
set(CMAKE_CXX_FLAGS "-m32" CACHE STRING "" FORCE)

# may help with some rare issues
set(CMAKE_PREFIX_PATH /usr/lib/i386-linux-gnu CACHE STRING "" FORCE)

# makes sure that at least on Ubuntu pkg-config will search for the :i386 packages
set(ENV{PKG_CONFIG_PATH} /usr/lib/i386-linux-gnu/pkgconfig/ CACHE STRING "" FORCE)

# make qtchooser find qt5
set(ENV{QT_SELECT} qt5 CACHE STRING "" FORCE)
