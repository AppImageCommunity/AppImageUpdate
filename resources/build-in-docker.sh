#! /bin/bash

set -xe

repo_root=$(readlink -f $(dirname "$0")/../)

IMAGE=appimageupdate-build

# building local image to "cache" installed dependencies for subsequent builds
docker build -t "$IMAGE" "$repo_root"/resources

docker run --rm -it -e CI=1 -v "$repo_root":/ws:ro -v $(readlink -f .):/out "$IMAGE" bash -c 'cd /out && bash -xe /ws/resources/build-appimages.sh'
