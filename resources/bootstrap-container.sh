#! /bin/bash

if ! which wget 2>&1 1>/dev/null; then
    apt-get update
    apt-get install -y wget
fi

echo 'deb http://download.opensuse.org/repositories/home:/TheAssassin:/AppImageLibraries/xUbuntu_16.04/ /' > /etc/apt/sources.list.d/curl-httponly.list
wget -nv https://download.opensuse.org/repositories/home:TheAssassin:AppImageLibraries/xUbuntu_16.04/Release.key -O- | sudo apt-key add -
apt-get update
apt-get install -y git g++ libxpm-dev libcurl3 libcurl4-openssl-dev automake libtool desktop-file-utils build-essential clang g++ libssl-dev pkg-config

if ! which cmake 2>&1 1>/dev/null; then
    wget https://cmake.org/files/v3.12/cmake-3.12.1-Linux-x86_64.tar.gz -O- | tar -xz --strip-components=1 -C /usr
fi
