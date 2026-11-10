#!/usr/bin/env bash
# The smoke test of the Docker image (docs/guides/deploy-with-docker.md), run by the CI before it publishes one:
# the server starts with a test settings file, as a user that is not root, says it is healthy with --health,
# welcomes two real players who play a game over the published UDP port, then stops on SIGTERM with code 0.
#
#   docker/smoke-test.sh <image of the server> <image of the players>
#
# The images: `docker build .` and `docker build --target smoke .`. The UDP port of the games, published once with
# the same number inside and outside, is RTYPE_SMOKE_GAME_PORT (default 47243): change it if it is taken.
set -euo pipefail
# After a pipe, grep without -q: -q stops at the first match, and the writer killed by SIGPIPE fails the pipe

server_image=${1:?usage: smoke-test.sh <server image> <players image>}
players_image=${2:?usage: smoke-test.sh <server image> <players image>}
game_port=${RTYPE_SMOKE_GAME_PORT:-47243}
name="rtype-smoke-$$-$RANDOM"
work=$(mktemp -d)

cleanup() {
    docker rm -f "$name" "$name-players" >/dev/null 2>&1 || true
    rm -rf "$work"
}
trap cleanup EXIT

fail() {
    echo "::error::$*"
    echo "--- Logs of the server ---"
    docker logs "$name" 2>&1 | tail -n 50 || true
    exit 1
}

# The settings of the image, with the game port of the test
sed "s/^game_port = .*/game_port = $game_port/" "$(dirname "$0")/server.toml" > "$work/server.toml"
grep -qx "game_port = $game_port" "$work/server.toml" || fail "docker/server.toml has no longer a game_port"

# Copied in rather than mounted: the daemon may not see the files of this machine (Docker Desktop, a remote
# daemon)
echo "Starting $server_image"
docker create --name "$name" \
    -p 127.0.0.1::4242/tcp -p "127.0.0.1:$game_port:$game_port/udp" \
    "$server_image" >/dev/null
docker cp "$work/server.toml" "$name:/config/server.toml" >/dev/null
docker start "$name" >/dev/null

# --health, as the health check of the image asks it
healthy=""
for _ in $(seq 1 60); do
    if docker exec "$name" /opt/rtype/r-type_server --health --settings /config/server.toml >/dev/null 2>&1; then
        healthy=yes
        break
    fi
    [ "$(docker inspect -f '{{.State.Running}}' "$name")" = true ] || fail "The server stopped at start"
    sleep 1
done
[ -n "$healthy" ] || fail "--health never passed"
docker exec "$name" /opt/rtype/r-type_server --health --settings /config/server.toml
for _ in $(seq 1 30); do
    status=$(docker inspect -f '{{.State.Health.Status}}' "$name")
    [ "$status" = healthy ] && break
    sleep 1
done
[ "$status" = healthy ] || fail "The health check of the image says $status"
echo "Health check of the image: $status"

[ "$(docker exec "$name" id -u)" != 0 ] || fail "The server runs as root"
docker exec "$name" test -s /data/admin-url.txt || fail "No admin-url.txt in /data"
docker exec "$name" test -s /data/rtype.db || fail "No database in /data"

# Two players, over the TLS of the lobby, pinning the key the server made in /data, then over UDP in a game
docker cp "$name:/data/server.key" "$work/server.key" >/dev/null
lobby_port=$(docker port "$name" 4242/tcp | head -n 1 | sed 's/.*://')
docker create --name "$name-players" --network host \
    "$players_image" "$lobby_port" /opt/smoke/server.key "Smoke test" Alice Bob >/dev/null
docker cp "$work/server.key" "$name-players:/opt/smoke/server.key" >/dev/null
docker start "$name-players" >/dev/null
ready=""
for _ in $(seq 1 60); do
    if docker logs "$name-players" 2>&1 | grep '^ready ' >/dev/null; then
        ready=yes
        break
    fi
    [ "$(docker inspect -f '{{.State.Running}}' "$name-players")" = true ] || break
    sleep 1
done
if [ -z "$ready" ]; then
    echo "--- Logs of the players ---"
    docker logs "$name-players" 2>&1 | tail -n 20
    fail "The players could not play a game"
fi
echo "Players in a game: $(docker logs "$name-players" 2>&1 | grep '^ready ')"
docker logs "$name" 2>&1 | grep "Alice (.*) joined the lobby" >/dev/null || fail "The lobby did not log the players"

# SIGTERM, as docker stop sends it: the games end, the master stops with code 0
docker rm -f "$name-players" >/dev/null
docker stop -t 30 "$name" >/dev/null
code=$(docker inspect -f '{{.State.ExitCode}}' "$name")
[ "$code" = 0 ] || fail "The server stopped with code $code on SIGTERM"
echo "Stopped on SIGTERM with code 0"
echo "Smoke test passed"
