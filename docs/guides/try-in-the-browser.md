# How to let visitors try the game in a browser

This guide serves R-Type with [Tryout](https://github.com/Akinator31/Tryout), which gives each visitor of a
portfolio a disposable container streamed into the page. It is for whoever runs a Tryout node and wants the
game in it.

The image holds the client and a server of its own: the visitor plays offline, or online against that server,
alone, in the same container. A Tryout session has no network, so it reaches no other server.

## Prerequisites

- A Tryout control plane and a node, set up as in the
  [Tryout documentation](https://akinator31.github.io/Tryout/deployment/). The node must be **linux/amd64**: the
  Discord Social SDK the client links exists for no other Linux, so the image exists for amd64 only.
- A node with room for it: the client draws its 3D scenes on the CPU, since a Tryout desktop has no GPU. See
  [Resources](#resources).
- The image: `ghcr.io/akinator31/rtype-tryout`, built and published by the CI (`.github/workflows/tryout.yml`),
  tagged `latest` and with its version for each release, `main` for the last commit of `main`. To build it
  yourself, from the root of the repository:

    ```sh
    docker build -f docker/tryout/Dockerfile -t rtype-tryout .
    ```

## What the image holds

The image is built on the Selkies desktop image of LinuxServer (`baseimage-selkies:debiantrixie`), which brings
the X server, the window manager, the stream and the web server Tryout talks to. Its files are in
`docker/tryout/`.

| Where | What |
|---|---|
| `/opt/rtype/` | `r-type_client`, its assets and the Discord SDK; `r-type_server` and its `config/` |
| `/etc/rtype/server.toml` | The [settings](configure-a-server.md) of the session's server: lobby on `127.0.0.1:4242`, at most 2 games, guests only, the panel on a free port of the loopback |
| `/var/lib/rtype/` | `server.key`, the key of the server, made when the image is built; the SQLite database, made at the start |
| `/etc/rtype/fingerprint` | The fingerprint of that key |

At the start of a session, the init of the desktop runs two services of the image
(`docker/tryout/s6-rc.d/`):

- `init-rtype` writes the settings of the client: the key of the local server already trusted, and 30 frames
  per second at most, the rate of the stream;
- `svc-rtype-server` starts the server, as the user of the desktop.

The desktop then starts the client, full screen and without a title bar; it starts again if the visitor quits.

Every session has the same server key. Nothing outside the container ever reaches that server, so the key only
spares the visitor the question of [trust](generate-the-server-keys.md) at the first connection.

## Steps

1. **Declare the project** in the `tryout.yaml` of the control plane, or in its admin interface, with the image
   pinned by digest (`docker buildx imagetools inspect ghcr.io/akinator31/rtype-tryout:latest` prints it):

    ```yaml
    projects:
      rtype:
        title: R-Type
        node: amd64-node
        image: ghcr.io/akinator31/rtype-tryout@sha256:...
        kind: desktop
        port: 3000
        max_sessions: 1
        max_age: 15m
        limits: { memory_mb: 3072, cpus: 3 }
    ```

2. **Pull the image on the node** beforehand (about 4.6 GB), on the daemon of its agent, so that the first
   visitor does not wait for it.

3. **Embed it** in the portfolio: `https://<control-plane>/embed/rtype`.

## Result

The visitor sees the asteroids and the logo, clicks to open the main menu, then:

- **Play** for the game offline;
- **Online**, then **Connect**: the server `127.0.0.1:4242` is already filled in and trusted. In the lobby,
  **Create a game**, choose a ship, then **Launch**.

The logs of the session's container show the server:

```
INFO  master   Lobby on TCP 0.0.0.0:4242, games on UDP 0.0.0.0:4242 (2 at most)
INFO  lobby    Player (1) joined the lobby from 127.0.0.1:38648
```

## Resources

Measured on Docker Desktop with runc, a session limited to 3 CPUs and 3 GB:

| | CPU | Memory |
|---|---|---|
| The client, menu or game | about 2.4 CPUs, every CPU it gets | about 2.2 GB |
| The server and a game | about 0.1 CPU | about 100 MB |
| The stream (Selkies) | about 0.1 CPU | about 150 MB |

The client renders in software (Mesa llvmpipe) and uses every CPU it is given, still below 30 frames per second
with 3 CPUs: the HUD then shows a ping of several hundred milliseconds, even against the local server. Give a
session at least 3 CPUs and 3 GB, and one session per node of that size. Under gVisor, a desktop costs 2 to 2.5
times the CPU of runc: run the project with `runtime: runc` on a node that allows it, or give it more CPUs.

## When it goes wrong

| Symptom | Fix |
|---|---|
| A click does not open the main menu | The game misses a press shorter than one frame when it renders slowly: press a little longer, or give the session more CPUs |
| The session ends at once, out of memory | Raise `limits.memory_mb` to 3072 or more |
| The node cannot pull the image: `no matching manifest for linux/arm64` | The image exists for amd64 only: put the project on an amd64 node |
| `Cannot reach 127.0.0.1:4242` in the client | The server did not start: read the logs of the session's container |
