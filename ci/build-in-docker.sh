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
docker_tag="ghcr.io/appimagecommunity/appimageupdate-build:$(cut -d: -f2- <<<"${DOCKER_METADATA_OUTPUT_TAGS:-:local-build}")"

default_branch_tag="$(cut -d: -f1 <<<"$docker_tag"):$(echo "${GITHUB_DEFAULT_BRANCH:-main}" | rev | cut -d/ -f1 | rev)"

extra_build_args=()

if [[ "${GITHUB_ACTIONS:-}" != "" ]]; then
    echo "Building on GitHub actions, pushing cache"
    extra_build_args+=(
        --output "type=registry,ref=${docker_tag}"
        --push
    )
else
    echo "Local build, not pushing cache"
fi

# building local image to "cache" installed dependencies for subsequent builds
docker buildx build \
    --platform "$docker_platform" \
    "${extra_build_args[@]}" \
    --cache-from "type=registry,ref=${docker_tag}" \
    --cache-from "type=registry,ref=${default_branch_tag}" \
    -t "$docker_tag" \
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
    --platform "$docker_platform" \
    --rm \
    -i \
    "${tty_args[@]}" \
    -e CI=1 \
    -e GITHUB_RUN_NUMBER \
    -v "$repo_root":/ws:ro \
    -v "$cwd":/out \
    -w /out \
    --user "$uid" \
    "$docker_tag" \
    bash /ws/ci/build-appimages.sh
