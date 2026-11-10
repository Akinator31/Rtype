#!/usr/bin/env bash
# The smoke test of the Docker image with its accounts (docs/guides/deploy-with-docker.md, part two), run by the
# CI after docker/smoke-test.sh: the server starts with the settings of the official server
# (docker/server-accounts.toml), its Discord secret in /run/secrets, beside a fake Discord and a fake mail
# server. It says it is healthy with --health, checks the mail server and Discord at start, signs in a player
# in a game, then an administrator in the panel, and stops on SIGTERM with code 0.
#
#   docker/smoke-test-accounts.sh <image of the server> <image of the smoke test>
#
# The images: `docker build .` and `docker build --target smoke .`. The fake services
# (tests/helpers/fake_services.cpp) run in a container of their own, named discord.com on a network shared with
# the server, whose image is given their certificate to trust: the master reaches them at the address of
# Discord it always uses. The UDP port of the games, published once with the same number inside and outside, is
# RTYPE_SMOKE_GAME_PORT (default 47253): change it if it is taken.
set -euo pipefail
# After a pipe, grep without -q: -q stops at the first match, and the writer killed by SIGPIPE fails the pipe

server_image=${1:?usage: smoke-test-accounts.sh <server image> <smoke image>}
smoke_image=${2:?usage: smoke-test-accounts.sh <server image> <smoke image>}
game_port=${RTYPE_SMOKE_GAME_PORT:-47253}
name="rtype-smoke-accounts-$$-$RANDOM"
work=$(mktemp -d)

# What tests/helpers/FakeDiscord.hpp accepts, and the accounts written before the start
discord_client_id=1234567890123456789
discord_secret=fake-discord-secret
password="correct horse battery"
admin=admin@example.org

cleanup() {
    docker rm -f "$name" "$name-services" "$name-players" >/dev/null 2>&1 || true
    docker volume rm "$name-data" >/dev/null 2>&1 || true
    docker network rm "$name-net" >/dev/null 2>&1 || true
    rm -rf "$work"
}
trap cleanup EXIT

fail() {
    echo "::error::$*"
    echo "--- Logs of the server ---"
    docker logs "$name" 2>&1 | tail -n 50 || true
    echo "--- Logs of the fake services ---"
    docker logs "$name-services" 2>&1 | tail -n 20 || true
    exit 1
}

command -v curl >/dev/null || fail "curl is missing on this machine: it asks the panel"

# The settings of the official server, with the fake services and the game port of the test. A key the file
# leaves out, or that changed name, fails the test here rather than in production
settings=$(dirname "$0")/server-accounts.toml
sed -e "s/^game_port = .*/game_port = $game_port/" \
    -e 's|^public_url = .*|public_url = "https://admin.rtype.test"|' \
    -e 's/^host = .*/host = "mail.test"/' \
    -e 's/^port = 587$/port = 25/' \
    -e 's/^security = .*/security = "none"/' \
    -e '/^username = /d' -e '/^password_file = /d' \
    -e "s/^client_id = .*/client_id = \"$discord_client_id\"/" \
    -e "s/^admins = .*/admins = [\"$admin\"]/" \
    "$settings" > "$work/server.toml"
for key in "game_port = $game_port" 'listen = "0.0.0.0:8080"' 'client_secret_file = "/run/secrets/discord_secret"' 'host = "mail.test"' \
    "client_id = \"$discord_client_id\""; do
    grep -qF "$key" "$work/server.toml" || fail "docker/server-accounts.toml has no longer the line of: $key"
done

docker network create "$name-net" >/dev/null

# Discord and the mail server, then the certificate they serve
echo "Starting the fake services"
docker create --name "$name-services" --network "$name-net" \
    --network-alias discord.com --network-alias mail.test \
    --entrypoint /opt/smoke/rtype_fake_services "$smoke_image" /tmp >/dev/null
docker start "$name-services" >/dev/null
ready=""
for _ in $(seq 1 30); do
    if docker logs "$name-services" 2>&1 | grep '^ready$' >/dev/null; then
        ready=yes
        break
    fi
    [ "$(docker inspect -f '{{.State.Running}}' "$name-services")" = true ] || break
    sleep 1
done
[ -n "$ready" ] || fail "The fake services did not start"
docker cp "$name-services:/tmp/certificate.pem" "$work/certificate.pem" >/dev/null

# An administrator of [admin] admins and a player, their emails verified, in the database of the volume, which
# the user of the server owns
docker volume create "$name-data" >/dev/null
docker run --rm -v "$name-data:/data" --entrypoint sh "$smoke_image" -c \
    "/opt/smoke/rtype_panel_accounts sqlite:/data/rtype.db 'Admin:$admin:$password:admin' \
        'Dana:dana@example.org:$password' && chown -R 10001:10001 /data" \
    || fail "Cannot write the accounts"

# Copied in rather than mounted: the daemon may not see the files of this machine (Docker Desktop, a remote
# daemon). The secret as Docker mounts its secrets, readable by the user of the server only
echo "Starting $server_image"
docker create --name "$name" --network "$name-net" -v "$name-data:/data" \
    -p 127.0.0.1::4242/tcp -p "127.0.0.1:$game_port:$game_port/udp" -p 127.0.0.1::8080/tcp \
    "$server_image" >/dev/null
docker cp "$work/server.toml" "$name:/config/server.toml" >/dev/null
mkdir "$work/secrets"
printf '%s\n' "$discord_secret" > "$work/secrets/discord_secret"
tar -C "$work" --owner=10001 --group=10001 --numeric-owner --mode=u=rwX,go= -cf - secrets \
    | docker cp -a - "$name:/run/" >/dev/null
# The certificate of the fake Discord among the authorities the image trusts
docker cp "$name:/etc/ssl/certs/ca-certificates.crt" "$work/ca-certificates.crt" >/dev/null
cat "$work/certificate.pem" >> "$work/ca-certificates.crt"
chmod 644 "$work/ca-certificates.crt"
docker cp "$work/ca-certificates.crt" "$name:/etc/ssl/certs/ca-certificates.crt" >/dev/null
docker start "$name" >/dev/null

# --health, as the health check of the image asks it: once the mail server and Discord are checked
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
echo "Healthy"
logs=$(docker logs "$name" 2>&1)
grep -q "Mail server mail.test:25" <<< "$logs" || fail "The master did not check the mail server"
grep -q "Discord application $discord_client_id" <<< "$logs" || fail "The master did not check Discord"
grep -q "Accounts on, guests allowed, Discord sign-in" <<< "$logs" || fail "The master started without its accounts"
! grep -q "can be read by every user" <<< "$logs" || fail "A secret file can be read by every user"

# A player signs in to its account and plays with a guest, over the TLS of the lobby, then over UDP
docker cp "$name:/data/server.key" "$work/server.key" >/dev/null
lobby_port=$(docker port "$name" 4242/tcp | head -n 1 | sed 's/.*://')
docker create --name "$name-players" --network host \
    "$smoke_image" "$lobby_port" /opt/smoke/server.key "Smoke test" "Dana:$password" Bob >/dev/null
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
    fail "The player could not sign in and play"
fi
echo "Signed in and in a game: $(docker logs "$name-players" 2>&1 | grep '^ready ')"

# The panel, as Traefik reaches it: the administrator signs in with its password, the player cannot
panel="http://127.0.0.1:$(docker port "$name" 8080/tcp | head -n 1 | sed 's/.*://')"
curl -sf "$panel/api/health" >/dev/null || fail "The panel does not answer /api/health"
offered=$(curl -sf "$panel/api/sign-in") || fail "The panel offers no sign-in"
[[ "$offered" == *'"password":true'* && "$offered" == *'"discord":true'* ]] \
    || fail "The panel offers $offered, not the password and Discord"
code=$(curl -s -o /dev/null -w '%{http_code}' -H 'Content-Type: application/json' \
    -d "{\"login\":\"dana@example.org\",\"password\":\"$password\"}" "$panel/api/sign-in")
[ "$code" = 403 ] || fail "A player signs in to the panel: $code"
curl -s -D "$work/headers" -o "$work/signed-in" -H 'Content-Type: application/json' \
    -d "{\"login\":\"$admin\",\"password\":\"$password\"}" "$panel/api/sign-in"
grep -qi '^set-cookie: rtype_session=.*; Secure' "$work/headers" \
    || fail "The administrator got no Secure session cookie: $(head -n 1 "$work/headers")"
# The cookie is Secure, which curl keeps for HTTPS: given by hand, as the browser behind Traefik would
cookie=$(grep -i '^set-cookie: rtype_session=' "$work/headers" | sed 's/^[^:]*: \(rtype_session=[^;]*\);.*/\1/')
accounts=$(curl -sf -H "Cookie: $cookie" "$panel/api/accounts") || fail "The administrator cannot list the accounts"
[[ "$accounts" == *'"username":"Dana"'* && "$accounts" == *'"locked":true'* ]] \
    || fail "The accounts in the panel: $accounts"
echo "Administrator signed in to the panel"

# SIGTERM, as docker stop sends it: the games end, the master stops with code 0
docker rm -f "$name-players" >/dev/null
docker stop -t 30 "$name" >/dev/null
code=$(docker inspect -f '{{.State.ExitCode}}' "$name")
[ "$code" = 0 ] || fail "The server stopped with code $code on SIGTERM"
echo "Stopped on SIGTERM with code 0"
echo "Smoke test with accounts passed"
