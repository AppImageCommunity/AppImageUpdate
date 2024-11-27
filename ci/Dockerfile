# current Debian oldstable as of Dec 2023
# we use Debian because they still provide i386 builds
# (also, they ship Qt 5.15.2, so we no longer have to rely on third-party builds)
# and it provides a C++17 compatible compiler out of the box!
FROM debian:bullseye

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
        liblzma-dev libzstd-dev zlib1g-dev libarchive-dev

RUN wget -qO- https://artifacts.assassinate-you.net/prebuilt-cmake/cmake-v3.28.0-debian-bullseye-"${CMAKE_ARCH}".tar.gz | \
    tar xzv -C/usr --strip-components=1

COPY ./install-gtest.sh /
RUN bash /install-gtest.sh

COPY pkgconfig/*.pc /
RUN mv /*.pc /usr/lib/*-linux-gnu*/pkgconfig/

ENV APPIMAGE_EXTRACT_AND_RUN=1

ENV DOCKER=1

#RUN git clone https://github.com/nlohmann/json.git -b v3.11.2 --depth=1 && \
#    cd json && \
#    mkdir build && \
#    cd build && \
#    cmake .. -DCMAKE_INSTALL_PREFIX=/usr/local && \
#    make -j "$(nproc --ignore=1)" install && \
#    cd ../.. && \
#    rm -rf json/

RUN apt-get update && apt-get install -y nlohmann-json3-dev

# work around bug in FindCURL.cmake, which does not parse the pkg-config provided protocols and features into lists causing
# the comparison in the loop to yield false negative results
# this makes it use curl-config which works much better
RUN rm /usr/lib/*-linux-gnu*/pkgconfig/libcurl.pc
