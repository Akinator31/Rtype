# syntax=docker/dockerfile:1
#
# R-Type in a browser, for Tryout (https://github.com/Akinator31/Tryout): the client on a desktop streamed by
# Selkies, and a server of its own in the same container, so that the visitor plays offline and online
# (docs/guides/try-in-the-browser.md). Built from the root of the repository:
#
#   docker build -f docker/tryout/Dockerfile -t rtype-tryout .
#
# Linux x86-64 only: the Discord Social SDK the client links exists for no other Linux. Both programs are built
# on the Debian of the desktop image, so that they find the same libraries at run time. The downloads of CPM
# and the build folder are kept in cache mounts, as in the image of the server.

ARG DEBIAN=trixie

FROM debian:${DEBIAN} AS build
RUN apt-get update \
    && apt-get install -y --no-install-recommends \
        ca-certificates cmake g++ git ninja-build patch python3 procps libssl-dev libasound2t64 libpulse0 \
        libx11-dev libxrandr-dev libxinerama-dev libxcursor-dev libxi-dev libgl1-mesa-dev \
    && rm -rf /var/lib/apt/lists/*
WORKDIR /src
COPY . .
ENV CPM_SOURCE_CACHE=/cache/cpm
RUN --mount=type=cache,target=/cache/cpm,sharing=locked \
    --mount=type=cache,id=rtype-tryout-build,target=/src/build,sharing=locked \
    cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DRTYPE_BUILD_TESTS=OFF \
        -DRTYPE_OPENXR=OFF -DRTYPE_WITH_POSTGRESQL=OFF \
    && cmake --build build --target r-type_client r-type_server \
    && cmake --install build --component client --prefix /opt/rtype \
    && cmake --install build --component server --prefix /opt/rtype \
    && strip /opt/rtype/r-type_client /opt/rtype/r-type_server

# The key of the session's server, made by its first start, and its fingerprint, which the client trusts
RUN mkdir -p /var/lib/rtype /etc/rtype \
    && (/opt/rtype/r-type_server --key /var/lib/rtype/server.key --database sqlite:/tmp/key.db \
        >/tmp/key.log 2>&1 &) \
    && for _ in $(seq 50); do [ -s /var/lib/rtype/server.key ] && break; sleep 0.2; done \
    && pkill r-type_server; sleep 1 \
    && /opt/rtype/r-type_server --fingerprint --key /var/lib/rtype/server.key | tr -d '[:space:]' \
        > /etc/rtype/fingerprint \
    && test -s /etc/rtype/fingerprint || { cat /tmp/key.log; exit 1; }

# --- The desktop: X, openbox, the stream and the nginx that serves it under the base path of the session ---
FROM ghcr.io/linuxserver/baseimage-selkies:debian${DEBIAN}
LABEL org.opencontainers.image.title="R-Type for Tryout" \
      org.opencontainers.image.description="The client and a server of R-Type on a desktop streamed in the browser" \
      org.opencontainers.image.source="https://github.com/Akinator31/Rtype"
RUN apt-get update \
    && apt-get install -y --no-install-recommends libssl3t64 \
    && rm -rf /var/lib/apt/lists/*
COPY --from=build /opt/rtype /opt/rtype
COPY --from=build /var/lib/rtype/server.key /var/lib/rtype/server.key
COPY --from=build /etc/rtype/fingerprint /etc/rtype/fingerprint
COPY docker/tryout/server.toml /etc/rtype/server.toml
# The server as a service of the desktop's init, and the client's settings made before it starts
COPY docker/tryout/s6-rc.d/ /etc/s6-overlay/s6-rc.d/
COPY docker/tryout/r-type /usr/local/bin/r-type
# What the desktop starts, and what its right-click menu offers
COPY docker/tryout/autostart /defaults/autostart
COPY docker/tryout/menu.xml /defaults/menu.xml
# The game fills the screen with no title bar, starts again if closed, and no terminal is offered
ENV TITLE="R-Type" NO_DECOR=1 RESTART_APP=true DISABLE_TERMINALS=true
