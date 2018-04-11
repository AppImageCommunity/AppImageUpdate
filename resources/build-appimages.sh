#! /bin/bash

set -x
set -e

# use RAM disk if possible
if [ -d /dev/shm ]; then
    TEMP_BASE=/dev/shm
else
    TEMP_BASE=/tmp
fi

BUILD_DIR=$(mktemp -d -p "$TEMP_BASE" AppImageUpdate-build-XXXXXX)

cleanup () {
    if [ -d "$BUILD_DIR" ]; then
        rm -rf "$BUILD_DIR"
    fi
}

trap cleanup EXIT

# store repo root as variable
REPO_ROOT=$(readlink -f $(dirname $(dirname $0)))
OLD_CWD=$(readlink -f .)

pushd "$BUILD_DIR"

cmake "$REPO_ROOT" \
    -DBUILD_QT_UI=ON \
    -DCMAKE_INSTALL_PREFIX=/usr \
    -DCMAKE_BUILD_TYPE=RelWithDebInfo

# create AppDir
mkdir -p AppDir

# now, compile and install to AppDir
make -j$(nproc) install DESTDIR=AppImageUpdate.AppDir
make -j$(nproc) install DESTDIR=appimageupdatetool.AppDir

# install resources into AppDirs
for appdir in AppImageUpdate.AppDir appimageupdatetool.AppDir; do
    mkdir -p "$appdir"/usr/share/{applications,icons/hicolor/scalable/apps/} "$appdir"/resources
    cp -v "$REPO_ROOT"/resources/*.desktop "$appdir"/usr/share/applications/
    cp -v "$REPO_ROOT"/resources/*.svg "$appdir"/usr/share/icons/hicolor/scalable/apps/
    cp -v "$REPO_ROOT"/resources/*.xpm "$appdir"/resources/
done

# determine Git commit ID
# linuxdeployqt uses this for naming the file
export VERSION=$(cd "$REPO_ROOT" && git rev-parse --short HEAD)

# prepend Travis build number if possible
if [ "$TRAVIS_BUILD_NUMBER" != "" ]; then
    export VERSION="$TRAVIS_BUILD_NUMBER-$VERSION"
fi


# remove unnecessary binaries from AppDirs
rm AppImageUpdate.AppDir/usr/bin/appimageupdatetool
rm appimageupdatetool.AppDir/usr/bin/AppImageUpdate
rm appimageupdatetool.AppDir/usr/lib/*qt*.so*


# remove other unnecessary data
find {appimageupdatetool,AppImageUpdate}.AppDir -type f -iname '*.a' -delete
rm -rf {appimageupdatetool,AppImageUpdate}.AppDir/usr/include


# get linuxdeployqt
wget https://github.com/probonopd/linuxdeployqt/releases/download/continuous/linuxdeployqt-continuous-x86_64.AppImage
chmod +x linuxdeployqt-continuous-x86_64.AppImage

# get appimagetool
wget https://github.com/AppImage/AppImageKit/releases/download/continuous/appimagetool-x86_64.AppImage
chmod +x appimagetool-x86_64.AppImage


LINUXDEPLOYQT_ARGS=

if [ "$CI" == "" ]; then
    LINUXDEPLOYQT_ARGS=" -no-copy-copyright-files"
fi


for app in appimageupdatetool AppImageUpdate; do
    find "$app".AppDir/

    # bundle application
    ./linuxdeployqt-continuous-x86_64.AppImage \
        "$app".AppDir/usr/share/applications/"$app".desktop \
        $LINUXDEPLOYQT_ARGS \
        -verbose=1 -bundle-non-qt-libs

    # create AppImageUpdate AppImage
    ./appimagetool-x86_64.AppImage -v "$app".AppDir \
        -u 'gh-releases-zsync|AppImage|AppImageUpdate|continuous|$app-*x86_64.AppImage.zsync'
done

# move AppImages to old cwd
mv {appimageupdatetool,AppImageUpdate}*.AppImage* "$OLD_CWD"/

popd
