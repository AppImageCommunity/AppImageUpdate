#! /bin/bash

set -e

if ! which wget 2>&1 1>/dev/null; then
    apt-get update
    apt-get install -y wget
fi

echo 'deb http://download.opensuse.org/repositories/home:/TheAssassin:/AppImageLibraries/xUbuntu_14.04/ /' > /etc/apt/sources.list.d/curl-httponly.list
wget -nv https://download.opensuse.org/repositories/home:TheAssassin:AppImageLibraries/xUbuntu_14.04/Release.key -O- | sudo apt-key add -
apt-get update
apt-get install -y git g++ libxpm-dev libcurl3 libcurl4-openssl-dev automake libtool desktop-file-utils build-essential clang g++ libssl-dev pkg-config

if ! which cmake 2>&1 1>/dev/null; then
    wget https://cmake.org/files/v3.12/cmake-3.12.1-Linux-x86_64.tar.gz -O- | tar -xz --strip-components=1 -C /usr
fi

if [ -f /opt/qt*/bin/qt*-env.sh ]; then
    . /opt/qt*/bin/qt*-env.sh || true
fi

# install Gtk 2 platform themes
# Gtk 2 dev files must be installed first, otherwise qmake won't be able to build the plugin (will just skip it silently)
apt-get -y install libgtk2.0-dev

git clone http://code.qt.io/qt/qtstyleplugins.git

pushd qtstyleplugins

mkdir build
cd build

qmake ..

make -j$(nproc)
sudo make install

popd
