# Building AppImageUpdate

## Building the AppImages of AppImageUpdate and appimageupdatetool

This section describes how to build `AppImageUpdate.AppImage` and `appimageupdatetool.AppImage`. This implies using as few -dev packages from the distribution as possible and privately bundling everything that cannot reasonably be assumed to be there in the default installation of all target systems (distributions).

## Building the library

This section describes how to build `libappimageupdate` for consumption in distribution packaging. This implies using as many -dev packages from the distribution as possible.

### Ubuntu-20.04

```
# Install dependencies
sudo apt-get update
sudo apt-get install -y --no-install-recommends software-properties-common
sudo add-apt-repository ppa:ubuntu-toolchain-r/test
sudo add-apt-repository -y ppa:beineri/opt-qt-5.15.2-focal
sudo apt-get update
sudo apt-get install -y g++-10 qt515base qt515wayland libgl1 libdrm-dev mesa-common-dev build-essential libssl-dev \
autoconf automake libtool wget vim-common desktop-file-utils pkgconf libgpgme-dev libglib2.0-dev libcairo2-dev \
librsvg2-dev libfuse-dev git libcurl4-openssl-dev argagg-dev libgcrypt20-dev libboost-dev libarchive-dev \
libzstd-dev nlohmann-json3-dev cmake

# Build
export CXX=g++-10
mkdir build
cd build
cmake .. -DCMAKE_INSTALL_PREFIX=/usr -DBUILD_TESTING=OFF
make -j$(nproc)
sudo make install
cd ..
```
