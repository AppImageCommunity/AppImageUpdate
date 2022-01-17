#! /bin/bash

set -x
set -e
set -o pipefail

# use RAM disk if possible
if [ "$CI" == "" ] && [ -d /dev/shm ]; then
    TEMP_BASE=/dev/shm
else
    TEMP_BASE=/tmp
fi

BUILD_DIR="$(mktemp -d -p "$TEMP_BASE" AppImageUpdate-build-XXXXXX)"

cleanup () {
    if [ -d "$BUILD_DIR" ]; then
        rm -rf "$BUILD_DIR"
    fi
}

trap cleanup EXIT

# store repo root as variable
REPO_ROOT="$(readlink -f "$(dirname "$(dirname "$0")")")"
OLD_CWD="$(readlink -f .)"

pushd "$BUILD_DIR"

export ARCH=${ARCH:-"$(uname -m)"}

if [ "$ARCH" == "i386" && "$DOCKER" == "" ]; then
    EXTRA_CMAKE_ARGS=("-DCMAKE_TOOLCHAIN_FILE=$REPO_ROOT/cmake/toolchains/i386-linux-gnu.cmake")
fi

cmake "$REPO_ROOT" \
    -DBUILD_QT_UI=ON \
    -DCMAKE_INSTALL_PREFIX=/usr \
    -DCMAKE_BUILD_TYPE=RelWithDebInfo \
    "${EXTRA_CMAKE_ARGS[@]}"

# now, compile and install to AppDir
make -j"$(nproc)"
make install DESTDIR=AppImageUpdate.AppDir
make install DESTDIR=appimageupdatetool.AppDir

# install resources into AppDirs
for appdir in AppImageUpdate.AppDir appimageupdatetool.AppDir; do
    mkdir -p "$appdir"/resources
    cp -v "$REPO_ROOT"/resources/*.xpm "$appdir"/resources/
done

# determine Git commit ID
# appimagetool uses this for naming the file
VERSION="$(cd "$REPO_ROOT" && git rev-parse --short HEAD)"
export VERSION

# prepend GitHub run number if possible
if [ "$GITHUB_RUN_NUMBER" != "" ]; then
    export VERSION="$GITHUB_RUN_NUMBER-$VERSION"
fi


# remove unnecessary binaries from AppDirs
rm AppImageUpdate.AppDir/usr/bin/appimageupdatetool
rm appimageupdatetool.AppDir/usr/bin/AppImageUpdate
rm appimageupdatetool.AppDir/usr/lib/*/libappimageupdate-qt.so


# remove other unnecessary data
find {appimageupdatetool,AppImageUpdate}.AppDir -type f -iname '*.a' -delete
rm -rf {appimageupdatetool,AppImageUpdate}.AppDir/usr/include


# get linuxdeploy and its qt plugin
wget https://github.com/TheAssassin/linuxdeploy/releases/download/continuous/linuxdeploy-"$ARCH".AppImage
wget https://github.com/TheAssassin/linuxdeploy-plugin-qt/releases/download/continuous/linuxdeploy-plugin-qt-"$ARCH".AppImage
chmod +x linuxdeploy*.AppImage

patch_appimage() {
    while [[ "$1" != "" ]]; do
        dd if=/dev/zero of="$1" conv=notrunc bs=1 count=3 seek=8
        shift
    done
}
patch_appimage linuxdeploy*.AppImage

for app in appimageupdatetool AppImageUpdate; do
    find "$app".AppDir/

    export UPD_INFO="gh-releases-zsync|AppImage|AppImageUpdate|continuous|$app-*$ARCH.AppImage.zsync"

    if [ "$app" == "AppImageUpdate" ]; then export EXTRA_FLAGS=("--plugin" "qt"); fi

    # overwrite AppImage filename to get static filenames
    # see https://github.com/AppImage/AppImageUpdate/issues/89
    export OUTPUT="$app"-"$ARCH".AppImage

    # bundle application
    ./linuxdeploy-"$ARCH".AppImage -v0 --appdir "$app".AppDir --output appimage "${EXTRA_FLAGS[@]}" -d "$REPO_ROOT"/resources/"$app".desktop -i "$REPO_ROOT"/resources/appimage.png
done

# move AppImages to old cwd
mv {appimageupdatetool,AppImageUpdate}*.AppImage* "$OLD_CWD"/

popd
