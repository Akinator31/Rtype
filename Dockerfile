# syntax=docker/dockerfile:1
#
# The image of the server (docs/guides/deploy-with-docker.md): the master, the instances it starts, the panel and
# the configuration of the game, guests only.
#
#   docker build -t rtype-server .                 the server, the default target
#   docker build --target smoke -t rtype-smoke .   the players of docker/smoke-test.sh, for the CI
#
# The panel is built by Node, the server by CMake, then both go into a small runtime, run as a user that is not
# root. The downloads of CPM and the build folder are kept in cache mounts: a change of the code rebuilds what
# it changed only.

ARG DEBIAN=trixie

# --- The panel (web/admin): its lock file, then its sources ---
FROM node:lts-${DEBIAN}-slim AS panel
WORKDIR /src/web/admin
COPY web/admin/package.json web/admin/package-lock.json ./
RUN --mount=type=cache,target=/root/.npm npm ci
COPY web/admin/ ./
RUN npm run build

# --- The server: CMake and Ninja, patch for the patches of Crow, libpq for PostgreSQL. raylib, which the server
# never uses, needs the headers of X11 and OpenGL to configure the project ---
FROM debian:${DEBIAN} AS build
RUN apt-get update \
    && apt-get install -y --no-install-recommends \
        ca-certificates cmake g++ git ninja-build patch python3 libpq-dev libssl-dev \
        libx11-dev libxrandr-dev libxinerama-dev libxcursor-dev libxi-dev libgl1-mesa-dev \
    && rm -rf /var/lib/apt/lists/*
WORKDIR /src
COPY . .
COPY --from=panel /src/web/admin/dist web/admin/dist
ENV CPM_SOURCE_CACHE=/cache/cpm
RUN --mount=type=cache,target=/cache/cpm,sharing=locked \
    --mount=type=cache,target=/src/build,sharing=locked \
    cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DRTYPE_BUILD_TESTS=OFF \
        -DRTYPE_WITH_POSTGRESQL=ON -DRTYPE_ADMIN_PANEL_DIR=/src/web/admin/dist \
    && cmake --build build --target r-type_server \
    && cmake --install build --component server --prefix /opt/rtype

# --- The players of the smoke test (tests/helpers/panel_players.cpp), built with the tests ---
FROM build AS players
RUN --mount=type=cache,target=/cache/cpm,sharing=locked \
    --mount=type=cache,target=/src/build,sharing=locked \
    cmake -S . -B build -DRTYPE_BUILD_TESTS=ON \
    && cmake --build build --target rtype_panel_players \
    && install -D build/bin/rtype_panel_players /opt/smoke/rtype_panel_players

FROM debian:${DEBIAN}-slim AS smoke
COPY --from=players /opt/smoke /opt/smoke
ENTRYPOINT ["/opt/smoke/rtype_panel_players"]

# --- The server, the default target ---
FROM debian:${DEBIAN}-slim AS server
LABEL org.opencontainers.image.title="R-Type server" \
      org.opencontainers.image.description="The master of R-Type, its games and its panel, guests only" \
      org.opencontainers.image.source="https://github.com/Akinator31/Rtype"
RUN apt-get update \
    && apt-get install -y --no-install-recommends ca-certificates libpq5 libssl3t64 \
    && rm -rf /var/lib/apt/lists/* \
    && useradd --system --uid 10001 --user-group --home-dir /data --no-create-home --shell /usr/sbin/nologin rtype \
    && mkdir -p /data /config \
    && chown rtype:rtype /data
COPY --from=build /opt/rtype /opt/rtype
COPY docker/server.toml /config/server.toml

USER rtype
WORKDIR /data
# The key, the SQLite database, admin-url.txt: lose it, and the players refuse the new key of the server
VOLUME /data
EXPOSE 4242/tcp 4243-4299/udp
# Asks /api/health of the master, on the loopback of the container: its event loop answers it
HEALTHCHECK --interval=30s --timeout=10s --start-period=60s --start-interval=2s --retries=3 \
    CMD ["/opt/rtype/r-type_server", "--health", "--settings", "/config/server.toml"]
ENTRYPOINT ["/opt/rtype/r-type_server"]
CMD ["--settings", "/config/server.toml"]
