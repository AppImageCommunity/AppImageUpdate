#! /bin/bash

if [[ "$DIST" == "" ]] || [[ "$ARCH" == "" ]]; then
    echo "Usage: env ARCH=... DIST=... bash $0"
    exit 1
fi

set -e
set -x

cwd="$PWD"
repo_root="$(readlink -f "$(dirname "$0")"/..)"

# needed to keep user ID in and outside Docker in sync to be able to write to workspace directory
image=appimageupdate-build:"$DIST"-"$ARCH"
dockerfile=Dockerfile."$ARCH"

if [ ! -f "$repo_root"/ci/"$dockerfile" ]; then
    echo "Error: $dockerfile could not be found"
    exit 1
fi

# building local image to "cache" installed dependencies for subsequent builds
docker build -t "$image" -f "$repo_root"/ci/"$dockerfile" --build-arg DIST="$DIST" "$repo_root"/ci

# run the build with the current user to
#   a) make sure root is not required for builds
#   b) allow the build scripts to "mv" the binaries into the /out directory
uid="$(id -u)"
# mount workspace read-only, trying to make sure the build doesn't ever touch the source code files
# of course, this only works reliably if you don't run this script from that directory
# but it's still not the worst idea to do so
docker run --user "$uid" --rm -i -e ARCH -e GITHUB_RUN_NUMBER -e CI=1 -v "$repo_root":/ws:ro -v "$cwd":/out "$image" \
    bash -xec 'cd /out && bash -xe /ws/ci/build-appimages.sh'
