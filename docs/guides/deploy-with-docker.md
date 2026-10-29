# How to deploy with Docker

This guide runs the R-Type server from its Docker image, in [Dokploy](https://dokploy.com) or with Docker
Compose alone: players reach the lobby and the games, the panel stays inside the container. It is for whoever
hosts the official server, or a server of their own on a machine with Docker.

This is part one: **guests only**, no accounts. The accounts, the mail server, Discord and the panel on its
domain come with part two.

## Prerequisites

- A Linux machine with Docker 25 or newer, and Dokploy on it if you deploy with Dokploy.
- Its firewall open for **4242/tcp** (the lobby) and **4243–4299/udp** (the games), as for a server without
  Docker ([Run a server](run-a-server.md), step 2).
- The image: `ghcr.io/akinator31/rtype`, built and published by the CI, tagged `latest` and with its version
  for each release, `main` for the last commit of `main`. To build it yourself, from the root of the
  repository: `docker build -t rtype-server .`.

## What the image holds

| Where | What |
|---|---|
| `/opt/rtype/` | `r-type_server`, its `config/` (the game) and `admin/` (the panel) |
| `/config/server.toml` | The [settings file](configure-a-server.md) of the image ([docker/server.toml](https://github.com/Akinator31/Rtype/blob/main/docker/server.toml)): lobby on 4242, games on 4243–4299, panel on `127.0.0.1:8080`, everything kept in `/data` |
| `/data` | A volume: `server.key`, the key the players trust; `rtype.db`, the SQLite database; `admin-url.txt`, the address of the panel |

The server runs as the user `rtype` (uid 10001), not root. Its health check runs `r-type_server --health`, which
asks `/api/health` of the master on the loopback of the container: the image needs no curl, and a master whose
event loop is stuck turns *unhealthy*. `docker stop` sends `SIGTERM`: the master ends its games, then exits
with 0, as without Docker.

## Steps

1. **Turn off the userland proxy of Docker on the host.** Each published port otherwise costs a
   `docker-proxy` process, 58 of them here. In `/etc/docker/daemon.json`:

    ```json
    { "userland-proxy": false }
    ```

    ```sh
    sudo systemctl restart docker
    ```

    Docker then publishes the ports with the firewall rules of the kernel alone. The container keeps its own
    network, not `network_mode: host`: part two puts the panel on the network of Traefik, which host mode cannot
    join.

2. **Create the service.** In Dokploy: a new *Compose* service, from the repository or with the content of
   [docker-compose.yml](https://github.com/Akinator31/Rtype/blob/main/docker-compose.yml) pasted in. Without
   Dokploy, the same file:

    ```sh
    docker compose up -d
    ```

    The file publishes 4242/tcp and 4243–4299/udp **with the same numbers** on the host: the lobby gives each
    player the port of their game, which must be the same outside the container. It keeps `/data` in the named
    volume `data`, and starts a small init as PID 1 (`init: true`), which hands `SIGTERM` to the master. Give
    no domain to the service in Dokploy: there is no web page to publish yet.

    To change the settings, mount a file of your own over those of the image: uncomment the line
    `./server.toml:/config/server.toml:ro` of the file. Keep `key` and `database` in `/data`, and the ports the
    file publishes. Another version than `latest`: `RTYPE_IMAGE=ghcr.io/akinator31/rtype:0.1.0`, in the
    environment of the service.

3. **Check that it runs.** After a few seconds, the service is *healthy*:

    ```sh
    docker compose ps
    docker compose logs server
    ```

    ```
    INFO  master   Lobby on TCP 0.0.0.0:4242, games on UDP 4243-4299 (57 at most)
    INFO  master   Panel on HTTP 127.0.0.1:8080
    INFO  master   Server key 5b61c7a9ce1a44bfc64fec2013a4a09b6b4d5a8db4129eff890a1462c7ea1f1c
    INFO  master   Master started: Ctrl+C to stop
    ```

    The server key is the fingerprint the players are shown at their first connection: give it to them
    ([Manage the server key](generate-the-server-keys.md)). It is also shown by
    `docker compose exec server /opt/rtype/r-type_server --fingerprint --key /data/server.key`. The same
    check as the health check, by hand:

    ```sh
    docker compose exec server /opt/rtype/r-type_server --health --settings /config/server.toml
    # healthy
    ```

4. **Connect** from the game: the address of the machine, port 4242 ([Play online](play-online.md)).

## Open the panel

Without accounts, the panel listens on the loopback **of the container**: neither the host nor the Internet
reaches it, and the master refuses to start if it is told to listen elsewhere. To open it, a small `socat`
joins the network of the container and hands its port to a Unix socket on the host, which an SSH tunnel
reaches from your machine.

1. On the host, start the relay, beside the container of the server (`docker compose ps` names it):

    ```sh
    sudo mkdir -p /run/rtype-panel
    docker run -d --rm --name rtype-panel --network container:<container of the server> \
        -v /run/rtype-panel:/run/rtype-panel alpine/socat:1.8.0.3 \
        UNIX-LISTEN:/run/rtype-panel/panel.sock,fork,mode=600,user=$(id -u) TCP:127.0.0.1:8080
    ```

2. From your machine, the tunnel, then the address of `admin-url.txt`:

    ```sh
    ssh -L 8080:/run/rtype-panel/panel.sock <user>@<host>
    docker compose exec server cat /data/admin-url.txt     # on the host
    # http://127.0.0.1:8080/?token=9b47…
    ```

    Open that address in a browser on your machine: the panel opens, as in
    [Administer a server](administer-a-server.md). The token is new at each start of the server.

3. Once done: `docker stop rtype-panel`. The relay is gone, and the panel unreachable again.

## Back up the database

Everything the server keeps is in the volume: the key, without which the players refuse the server, and the
database. With SQLite, stop the server for the time of the copy, a few seconds, so that the copy is whole:

```sh
docker compose stop server
docker run --rm -v <project>_data:/data:ro -v "$PWD":/backup debian:trixie-slim \
    tar -C /data -czf /backup/rtype-data-$(date +%F).tar.gz .
docker compose start server
```

`<project>_data` is the volume: `docker volume ls` names it. To restore, stop the server, empty the volume, and
extract the archive into it the same way (`tar -C /data -xzf /backup/…`, the volume mounted without `:ro`), then
`chown -R 10001:10001 /data` in that container. With a PostgreSQL database
([Use an external database](use-an-external-database.md)), back it up with `pg_dump` instead, and the volume
for the key.

## Result

The server runs in its container, restarts with the machine (`restart: unless-stopped`), and players connect
as guests. If it does not start, `docker compose logs server` says why, as in [Run a server](run-a-server.md#result):

| Message | Fix |
|---|---|
| `Cannot write the key /data/server.key: Permission denied` | The volume is a folder of the host the user 10001 cannot write: `sudo chown -R 10001:10001 <folder>`, or a named volume, as in the file |
| `Cannot listen on 0.0.0.0:4242: Address already in use` | Inside the container, never: the port of the host is taken by another program, which `docker compose up` names |
| `[web] listen must be a loopback address (127.0.0.1, [::1]) without accounts` | Your settings file gives the panel another address: keep `127.0.0.1:8080` until the accounts |
| *unhealthy*, `Not healthy: the master did not answer within 5s` | The event loop of the master is stuck: its logs before say on what; `docker compose restart server` |
| Players reach the lobby, then no game | The UDP range is not open in the firewall of the host, or not published with the same numbers |
