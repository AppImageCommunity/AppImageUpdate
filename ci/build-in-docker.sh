#! /bin/bash

if [[ "$ARCH" == "" ]]; then
    echo "Usage: env ARCH=... bash $0"
    exit 1
fi

set -euxo pipefail

case "$ARCH" in
    x86_64)
        docker_platform=linux/amd64
        ;;
    i686)
        CMAKE_ARCH=i386
        docker_platform=linux/386
        ;;
    armhf)
        docker_platform=linux/arm/v7
        ;;
    aarch64)
        docker_platform=linux/arm64/v8
        ;;
    *)
        echo "Unsupported architecture: $ARCH"
        exit 2
esac

CMAKE_ARCH="${CMAKE_ARCH:-"$ARCH"}"

cwd="$PWD"
repo_root="$(readlink -f "$(dirname "$0")"/..)"

# use docker image tag provided by GitHub actions if possible
DOCKER_TAG="${DOCKER_TAG:-ghcr.io/appimage/appimageupdate-build:local-build}"

default_branch_tag="$(echo "$DOCKER_TAG" | cut -d: -f1):$(echo "$GITHUB_DEFAULT_BRANCH" | rev | cut -d/ -f1 | rev)"

# building local image to "cache" installed dependencies for subsequent builds
docker build \
    --cache-to inline \
    --cache-from "type=registry,ref=${DOCKER_TAG}" \
    --cache-from "type=registry,ref=${default_branch_tag}" \
    --platform "$docker_platform" \
    -t "$DOCKER_TAG" \
    --build-arg ARCH="$ARCH" \
    --build-arg CMAKE_ARCH="$CMAKE_ARCH" \
    "$repo_root"/ci

# run the build with the current user to
#   a) make sure root is not required for builds
#   b) allow the build scripts to "mv" the binaries into the /out directory
uid="$(id -u)"

tty_args=()
if [ -t 0 ]; then tty_args+=("-t"); fi

# mount workspace read-only, trying to make sure the build doesn't ever touch the source code files
# of course, this only works reliably if you don't run this script from that directory
# but it's still not the worst idea to do so
docker run \
    --rm \
    -i \
    "${tty_args[@]}" \
    -e CI=1 \
    -e GITHUB_RUN_NUMBER \
    -v "$repo_root":/ws:ro \
    -v "$cwd":/out \
    -w /out \
    --user "$uid" \
    "$DOCKER_TAG" \
    bash /ws/ci/build-appimages.sh
