# Building AppImageUpdate

## Building the AppImages of AppImageUpdate and appimageupdatetool

This section describes how to build `AppImageUpdate.AppImage` and `appimageupdatetool.AppImage`. This implies using as few -dev packages from the distribution as possible and privately bundling everything that cannot reasonably be assumed to be there in the default installation of all target systems (distributions).

We generally recommend to use our pre-built AppImages if possible.
See https://github.com/AppImage/AppImageUpdate/blob/rewrite/.travis.yml for how these get built.

## Building the libraries

This section describes how to build `libappimageupdate` and `libappimage` for consumption in distribution packaging. This implies using as many -dev packages from the distribution as possible.

### Ubuntu 18.04

```# Compile and install libappimageupdate

sudo apt -y install wget git cmake g++ libcurl4-openssl-dev libx11-dev libz-dev

git clone --recursive https://github.com/AppImage/AppImageUpdate
cd AppImageUpdate/

mkdir build/
cd build/

cmake -DBUILD_QT_UI=OFF -DCMAKE_INSTALL_PREFIX=/usr ..
make -j$(nproc)

sudo make install

cd ..

# Also compile and install libappimage

sudo apt -y install libssl-dev libinotifytools0-dev libarchive-dev libfuse-dev liblzma-dev 

git clone --recursive https://github.com/AppImage/AppImageKit
cd AppImageKit/

mkdir build/
cd build/

cmake -DUSE_SYSTEM_XZ=ON -DUSE_SYSTEM_INOTIFY_TOOLS=ON -DUSE_SYSTEM_LIBARCHIVE=ON -DUSE_SYSTEM_GTEST=OFF -DCMAKE_INSTALL_PREFIX=/usr ..
make -j$(nproc)

sudo make install
```

### CentOS 7

```
cat /etc/redhat-release 
# CentOS Linux release 7.2.1511 (Core) 

# Compile and install libappimageupdate

wget http://dl.fedoraproject.org/pub/epel/epel-release-latest-7.noarch.rpm
rpm -ivh epel-release-latest-7.noarch.rpm

yum install git cmake3 gcc-c++ curl-devel libX11-devel zlib-devel

git clone --recursive https://github.com/AppImage/AppImageUpdate
cd AppImageUpdate/

mkdir build/
cd build/

cmake3 -DBUILD_QT_UI=OFF -DCMAKE_INSTALL_PREFIX=/usr ..
make -j$(nproc)

sudo make install

cd ..

# Also compile libappimage; work in progress

git clone https://github.com/AppImage/AppImageKit
cd AppImageKit/

mkdir build
cd build
cmake3 -DUSE_SYSTEM_XZ=ON -DUSE_SYSTEM_INOTIFY_TOOLS=ON -DUSE_SYSTEM_LIBARCHIVE=ON -DUSE_SYSTEM_GTEST=ON ..
make -j$(nproc)

# Section to be completed

```
