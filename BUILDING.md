# Building the libraries

This page describes how to build `libappimageupdate` and `libappimage`. 

## Ubuntu 18.04

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

cd ./build/src/elf/AppImageKit-src/

mkdir build/
cd build/

cmake -DUSE_SYSTEM_XZ=ON -DUSE_SYSTEM_INOTIFY_TOOLS=ON -DUSE_SYSTEM_LIBARCHIVE=ON -DUSE_SYSTEM_GTEST=OFF -DCMAKE_INSTALL_PREFIX=/usr ..
make -j$(nproc)

sudo make install
```

## CentOS 7

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

cd ./src/elf/AppImageKit-src/
mkdir build
cd build
cmake3 -DUSE_SYSTEM_XZ=ON -DUSE_SYSTEM_INOTIFY_TOOLS=ON -DUSE_SYSTEM_LIBARCHIVE=ON -DUSE_SYSTEM_GTEST=ON ..
make -j$(nproc)

# Section to be completed

```
