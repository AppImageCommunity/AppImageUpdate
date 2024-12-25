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

if [ "$ARCH" == "i386" ] && [ "$DOCKER" == "" ]; then
    EXTRA_CMAKE_ARGS=("-DCMAKE_TOOLCHAIN_FILE=$REPO_ROOT/cmake/toolchains/i386-linux-gnu.cmake")
fi

cmake "$REPO_ROOT" \
    -DBUILD_QT_UI=ON \
    -DCMAKE_INSTALL_PREFIX=/usr \
    -DCMAKE_BUILD_TYPE=RelWithDebInfo \
    "${EXTRA_CMAKE_ARGS[@]}"

# next step is to build the binaries
make -j"$(nproc)"

# set up the AppDirs initially
for appdir in {appimageupdatetool,AppImageUpdate,validate}.AppDir; do
    make install DESTDIR="$appdir"
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
rm AppImageUpdate.AppDir/usr/bin/validate
rm appimageupdatetool.AppDir/usr/bin/AppImageUpdate
rm appimageupdatetool.AppDir/usr/bin/validate
rm appimageupdatetool.AppDir/usr/lib/*/libappimageupdate-qt*.so*
rm validate.AppDir/usr/bin/{AppImageUpdate,appimageupdatetool}
rm validate.AppDir/usr/lib/*/libappimageupdate*.so*


# remove other unnecessary data
find {appimageupdatetool,AppImageUpdate,validate}.AppDir -type f -iname '*.a' -delete
rm -rf {appimageupdatetool,AppImageUpdate}.AppDir/usr/include


# get linuxdeploy and its qt plugin
wget https://github.com/TheAssassin/linuxdeploy/releases/download/continuous/linuxdeploy-"$CMAKE_ARCH".AppImage
wget https://github.com/TheAssassin/linuxdeploy-plugin-qt/releases/download/continuous/linuxdeploy-plugin-qt-"$CMAKE_ARCH".AppImage
wget https://github.com/darealshinji/linuxdeploy-plugin-checkrt/releases/download/continuous/linuxdeploy-plugin-checkrt.sh
chmod +x linuxdeploy*.AppImage linuxdeploy-plugin-checkrt.sh

patch_appimage() {
    while [[ "$1" != "" ]]; do
        dd if=/dev/zero of="$1" conv=notrunc bs=1 count=3 seek=8
        shift
    done
}
patch_appimage linuxdeploy*.AppImage

for app in appimageupdatetool AppImageUpdate validate; do
    find "$app".AppDir/

    export UPD_INFO="gh-releases-zsync|AppImage|AppImageUpdate|continuous|$app-*$ARCH.AppImage.zsync"

    # note that we need to overwrite this in every iteration, otherwise the value will leak into the following iterationso
    extra_flags=()
    if [ "$app" == "AppImageUpdate" ]; then
        extra_flags=("--plugin" "qt");
    fi

    # overwrite AppImage filename to get static filenames
    # see https://github.com/AppImage/AppImageUpdate/issues/89
    export OUTPUT="$app"-"$ARCH".AppImage

    # bundle application
    ./linuxdeploy-"$CMAKE_ARCH".AppImage --appdir "$app".AppDir --output appimage "${extra_flags[@]}" -d "$REPO_ROOT"/resources/"$app".desktop -i "$REPO_ROOT"/resources/appimage.png --plugin checkrt
done

# move AppImages to old cwd
mv {appimageupdatetool,AppImageUpdate,validate}*.AppImage* "$OLD_CWD"/

popd
