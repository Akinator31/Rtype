#!/usr/bin/env bash
# The smoke test of the Docker image (docs/guides/deploy-with-docker.md), run by the CI before it publishes one:
# the server starts with a test settings file, as a user that is not root, says it is healthy with --health,
# welcomes two real players who play a game over the published UDP ports, then stops on SIGTERM with code 0.
#
#   docker/smoke-test.sh <image of the server> <image of the players>
#
# The images: `docker build .` and `docker build --target smoke .`. The game ports, published with the same
# numbers inside and outside, are RTYPE_SMOKE_GAME_PORTS (default 47243-47245): change them if they are taken.
set -euo pipefail

server_image=${1:?usage: smoke-test.sh <server image> <players image>}
players_image=${2:?usage: smoke-test.sh <server image> <players image>}
game_ports=${RTYPE_SMOKE_GAME_PORTS:-47243-47245}
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

# The settings of the image, with the game ports of the test
sed "s/^game_ports = .*/game_ports = \"$game_ports\"/" "$(dirname "$0")/server.toml" > "$work/server.toml"

# Copied in rather than mounted: the daemon may not see the files of this machine (Docker Desktop, a remote
# daemon)
echo "Starting $server_image"
docker create --name "$name" \
    -p 127.0.0.1::4242/tcp -p "127.0.0.1:$game_ports:$game_ports/udp" \
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
    if docker logs "$name-players" 2>&1 | grep -q '^ready '; then
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
docker logs "$name" 2>&1 | grep -q "Alice (.*) joined the lobby" || fail "The lobby did not log the players"

# SIGTERM, as docker stop sends it: the games end, the master stops with code 0
docker rm -f "$name-players" >/dev/null
docker stop -t 30 "$name" >/dev/null
code=$(docker inspect -f '{{.State.ExitCode}}' "$name")
[ "$code" = 0 ] || fail "The server stopped with code $code on SIGTERM"
echo "Stopped on SIGTERM with code 0"
echo "Smoke test passed"
