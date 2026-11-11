# How to deploy with Docker

This guide runs the R-Type server from its Docker image, in [Dokploy](https://dokploy.com) or with Docker
Compose alone. It is for whoever hosts the official server, or a server of their own on a machine with Docker.

It has two parts. [Part one](#steps), **guests only**: players reach the lobby and the games, the panel stays
inside the container. [Part two](#with-accounts), the official server: **accounts**, the mail server and
Discord configured, their secrets as files, and the panel on its domain over HTTPS.

## Prerequisites

- A Linux machine with Docker 25 or newer, and Dokploy on it if you deploy with Dokploy.
- Its firewall open for **4242/tcp** (the lobby) and **4242/udp** (every game), as for a server without
  Docker ([Run a server](run-a-server.md), step 2).
- The image: `ghcr.io/akinator31/rtype-server`, built and published by the CI, tagged `latest` and with its version
  for each release, `main` for the last commit of `main`. It exists for **linux/amd64** and **linux/arm64**
  (an ARM server, a Raspberry Pi 4 or 5 with a 64-bit system): Docker pulls the one of the machine. To build
  it yourself, from the root of the repository: `docker build -t rtype-server .`.

## What the image holds

| Where | What |
|---|---|
| `/opt/rtype/` | `r-type_server`, its `config/` (the game) and `admin/` (the panel) |
| `/config/server.toml` | The [settings file](configure-a-server.md) of the image ([docker/server.toml](https://github.com/Akinator31/Rtype/blob/main/docker/server.toml)): lobby on 4242/tcp, games on 4242/udp, panel on `127.0.0.1:8080`, everything kept in `/data`, guests only. Part two mounts another one over it |
| `/data` | A volume: `server.key`, the key the players trust; `rtype.db`, the SQLite database; `admin-url.txt`, the address of the panel |

The server runs as the user `rtype` (uid 10001), not root. Its health check runs `r-type_server --health`, which
asks `/api/health` of the master on the loopback of the container: the image needs no curl, and a master whose
event loop is stuck turns *unhealthy*. `docker stop` sends `SIGTERM`: the master ends its games, then exits
with 0, as without Docker.

## Steps

1. **Turn off the userland proxy of Docker on the host.** Each published port otherwise costs a
   `docker-proxy` process, which copies its traffic in user space, two of them here. In
   `/etc/docker/daemon.json`:

    ```json
    { "userland-proxy": false }
    ```

    ```sh
    sudo systemctl restart docker
    ```

    Docker then publishes the ports with the firewall rules of the kernel alone. The container keeps its own
    network, not `network_mode: host`: part two puts the panel on the network of Traefik, which host mode cannot
    join.

2. **Create the service.** In Dokploy: a new *Compose* service, with this file pasted in. Without Dokploy, the
   same file, as `docker-compose.yml`, then `docker compose up -d`:

    ```yaml
    services:
      server:
        image: ${RTYPE_IMAGE:-ghcr.io/akinator31/rtype-server:latest}
        restart: unless-stopped
        init: true
        ports:
          - "4242:4242/tcp"
          - "4242:4242/udp"
        volumes:
          - data:/data
        stop_grace_period: 30s

    volumes:
      data:
    ```

    The file publishes 4242/tcp and 4242/udp **with the same numbers** on the host: the lobby gives each
    player the UDP port of the games, which must be the same outside the container. Every game is on that one
    port: the master forwards each datagram to its game ([The relay](../architecture/server.md#the-relay)). It keeps `/data` in the named
    volume `data`, and starts a small init as PID 1 (`init: true`), which hands `SIGTERM` to the master. Give
    no domain to the service in Dokploy: there is no web page to publish without accounts.

    It is the [docker-compose.yml](https://github.com/Akinator31/Rtype/blob/main/docker-compose.yml) of the
    repository without its settings and secrets, which part two adds. To change the settings, mount a file of
    your own over those of the image, `./server.toml:/config/server.toml:ro` in `volumes`. Keep `key` and
    `database` in `/data`, and the ports the file publishes. Another version than `latest`:
    `RTYPE_IMAGE=ghcr.io/akinator31/rtype-server:0.1.0`, in the environment of the service.

3. **Check that it runs.** After a few seconds, the service is *healthy*:

    ```sh
    docker compose ps
    docker compose logs server
    ```

    ```
    INFO  master   Lobby on TCP 0.0.0.0:4242, games on UDP 0.0.0.0:4242 (57 at most)
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

## With accounts

The official server: players create accounts with their email or Discord, and the administrators reach the
panel at its domain, over HTTPS, through the Traefik of Dokploy. Everything of part one stays: the ports, the
volume, the image.

### Prerequisites

- Part one done: the userland proxy turned off, the ports open.
- A domain for the panel, `admin.rtype.example.org`, whose DNS points at the host, and the Traefik of Dokploy
  serving HTTPS with Let's Encrypt.
- A mail account the server sends the codes from, over STARTTLS or TLS ([Set up
  accounts](set-up-accounts.md), steps 1 to 2).
- A Discord application, its redirects set ([Sign in with Discord](set-up-accounts.md#sign-in-with-discord)),
  among them `https://admin.rtype.example.org/auth/discord/callback` for the panel.

### Steps

1. **Write the settings.** Copy [docker/server-accounts.toml](https://github.com/Akinator31/Rtype/blob/main/docker/server-accounts.toml)
   and change the values marked *yours*: `public_url`, the domain of the panel; `[mail]`, its server and
   account; `client_id`, the id of the Discord application; `admins`, the first administrators, by the email
   of their account or `discord:<id>`. The panel listens on `0.0.0.0:8080`, every address of the container,
   which the master accepts with accounts only.

    `trusted_proxies` is the subnet of the network of Traefik, whose `X-Forwarded-For` gives the address of
    the visitor, for the logs and the limits on signing in. On the host:

    ```sh
    docker network inspect dokploy-network --format '{{range .IPAM.Config}}{{.Subnet}}{{end}}'
    # 10.0.1.0/24
    ```

2. **Give the service its files.** In Dokploy, in *Advanced*, *Mounts*, add three *File mounts*, which
   Dokploy keeps in `../files/` beside the compose file, on the host in
   `/etc/dokploy/compose/<service>/files/`:

    | File path | Content |
    |---|---|
    | `server.toml` | The settings of step 1 |
    | `mail_password` | The password of the mail account, on one line |
    | `discord_secret` | The client secret of the Discord application, on one line |

    The compose file mounts `server.toml` over the settings of the image, and hands the two secrets to the
    server as Docker secrets, in `/run/secrets/`, where the settings name them: never in the settings, never in
    the environment. Docker Compose mounts them as they are on the host: make them readable by the user of the
    server, 10001, and nobody else, or the server warns at start, or cannot read them:

    ```sh
    cd /etc/dokploy/compose/<service>/files
    sudo chown 10001:10001 mail_password discord_secret
    sudo chmod 600 mail_password discord_secret
    ```

    Without Dokploy: the same three files in a folder, `files/` beside `docker-compose.yml`, and
    `RTYPE_FILES=./files` in the environment of `docker compose`.

3. **Create the service** from [docker-compose.yml](https://github.com/Akinator31/Rtype/blob/main/docker-compose.yml)
   of the repository: a new *Compose* service in Dokploy, from the repository or with the file pasted in. It
   publishes the lobby and the games as part one, and the panel on no port of the host: `expose: 8080` only.

4. **Give the panel its domain.** In Dokploy, in *Domains*, add `admin.rtype.example.org`: service `server`,
   container port `8080`, HTTPS on, certificate Let's Encrypt. Dokploy adds the labels of Traefik to the
   service and joins it to `dokploy-network`: Traefik reaches the panel over that network, nothing else does.
   Then *Deploy*.

5. **Check that it runs.** The service turns *healthy*; its logs show the mail server and Discord checked:

    ```
    INFO  master   Mail server mail.example.org:587, mails from R-Type <rtype@example.org>
    INFO  master   Discord application 123456789012345678
    INFO  master   Lobby on TCP 0.0.0.0:4242, games on UDP 0.0.0.0:4242 (57 at most)
    INFO  master   Panel on HTTP 0.0.0.0:8080
    INFO  master   Accounts on, guests allowed, Discord sign-in
    INFO  master   Master started: Ctrl+C to stop
    ```

6. **Sign in to the panel.** The first administrators create their account from the game, and verify their
   email, or sign in with Discord: at the next start of the server, or at once if they had one, they are
   administrators. Then `https://admin.rtype.example.org`, with their password or **Sign in with Discord**
   ([Administer a server](administer-a-server.md#from-anywhere-with-an-account)).

To change a secret: change its file, then *Redeploy*, or `docker compose up -d --force-recreate`; the server
reads its secrets at start only.

!!! warning "The mail server and Discord, at every start"

    The server checks its mail server and Discord at every start, and does not start without them. With
    `restart: unless-stopped`, the container then restarts again and again, a few seconds apart, until they
    answer: the service is *Restarting*, the players cannot connect, and the logs say why at each start
    ([Set up accounts](set-up-accounts.md#result)). It starts by itself once they answer again; a wrong
    setting or secret, fixed, then *Redeploy*.

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

The server runs in its container, restarts with the machine (`restart: unless-stopped`), and players connect,
as guests, or with their accounts after part two, whose panel answers at its domain over HTTPS. If it does not
start, `docker compose logs server` says why, as in [Run a server](run-a-server.md#result):

| Message | Fix |
|---|---|
| `Cannot write the key /data/server.key: Permission denied` | The volume is a folder of the host the user 10001 cannot write: `sudo chown -R 10001:10001 <folder>`, or a named volume, as in the file |
| `Cannot listen on 0.0.0.0:4242: Address already in use` | Inside the container, never: the port of the host is taken by another program, which `docker compose up` names |
| `[web] listen must be a loopback address (127.0.0.1, [::1]) without accounts` | Your settings file gives the panel another address: keep `127.0.0.1:8080` until the accounts |
| `cannot read /run/secrets/…` | The file of the secret on the host is not readable by 10001: `sudo chown 10001:10001` it, step 2 of part two |
| `… can be read by every user of this machine` | A warning: the file of the secret on the host is readable by everyone, `sudo chmod 600` it |
| `Cannot use the mail server …`, `Cannot use the Discord application …`, the service *Restarting* | The mail server or Discord does not answer, or refuses the settings: see the warning of part two |
| `Bad Gateway` or `404 page not found` at the domain of the panel | Traefik does not reach the panel: the domain names the service `server` and the port `8080`, and the settings `listen = "0.0.0.0:8080"` |
| *This account is not an administrator*, in the panel | Name it in `[admin] admins`, its email verified, then *Redeploy*; or make it one from an administrator ([Administer a server](administer-a-server.md#result)) |
| *unhealthy*, `Not healthy: the master did not answer within 5s` | The event loop of the master is stuck: its logs before say on what; `docker compose restart server` |
| Players reach the lobby, then no game | 4242/udp is not open in the firewall of the host, or not published with the same number (`- "4242:4242/udp"`) |
| `game_ports is gone`, the service *Restarting* | A settings file of an older server: replace `game_ports` with `game_port = 4242` and `max_games = 57`, and publish `4242:4242/udp` instead of the range |
