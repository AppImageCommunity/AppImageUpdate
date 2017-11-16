#! /bin/bash

if ! which wget 2>&1 1>/dev/null; then
    apt-get update
    apt-get install -y wget
fi

echo 'deb http://download.opensuse.org/repositories/home:/TheAssassin:/AppImageLibraries/xUbuntu_14.04/ /' > /etc/apt/sources.list.d/curl-httponly.list
wget -nv https://download.opensuse.org/repositories/home:TheAssassin:AppImageLibraries/xUbuntu_14.04/Release.key -O- | sudo apt-key add -
apt-get update
apt-get install -y git g++ libxpm-dev libcurl3 libcurl3-gnutls build-essential clang g++ libssl-dev

if ! which cmake 2>&1 1>/dev/null; then
    wget https://cmake.org/files/v3.10/cmake-3.10.0-rc5-Linux-x86_64.tar.gz -O- | tar -xz --strip-components=1 -C /
fi
