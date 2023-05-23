#! /bin/bash

set -euxo pipefail

tempdir="$(mktemp -d)"

_cleanup() {
    [[ -d "$tempdir" ]] && rm -r "$tempdir"
}
trap _cleanup EXIT

cd "$tempdir"

git clone https://github.com/google/googletest -b v1.13.0 .

mkdir build
cd build

cmake .. -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=/usr
make -j6
make install
