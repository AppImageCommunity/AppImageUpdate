FROM debian:oldstable

ARG ARCH
ARG DOCKER_ARCH
ARG CMAKE_ARCH
ENV ARCH=${ARCH} DOCKER_ARCH=${DOCKER_ARCH} CMAKE_ARCH=${CMAKE_ARCH}

ENV DEBIAN_FRONTEND=noninteractive

RUN apt-get update && \
    apt-get install -y qtbase5-dev qttools5-dev-tools qtwayland5-dev-tools qtwayland5-private-dev \
        libgl1 libdrm-dev mesa-common-dev \
        build-essential libssl-dev autoconf automake libtool \
        wget vim-common desktop-file-utils pkgconf libgpgme-dev \
        libglib2.0-dev libcairo2-dev librsvg2-dev libfuse-dev git libcurl4-openssl-dev argagg-dev libgcrypt20-dev libboost-dev \
        liblzma-dev libzstd-dev zlib1g-dev libarchive-dev cmake googletest nlohmann-json3-dev

COPY pkgconfig/*.pc /
RUN mv /*.pc /usr/lib/*-linux-gnu*/pkgconfig/

ENV APPIMAGE_EXTRACT_AND_RUN=1

ENV DOCKER=1

# work around bug in FindCURL.cmake, which does not parse the pkg-config provided protocols and features into lists causing
# the comparison in the loop to yield false negative results
# this makes it use curl-config which works much better
RUN rm /usr/lib/*-linux-gnu*/pkgconfig/libcurl.pc
