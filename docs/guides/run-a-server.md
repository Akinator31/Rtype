# How to run a server

This guide starts an R-Type server that players can reach, then stops it. It is for anyone who hosts a server,
on their own machine for development or on a machine on the Internet.

## Prerequisites

- The project builds: see [Build](../getting-started/build.md). The server is `build/bin/r-type_server`
  (`build\bin\Release\r-type_server.exe` on Windows), or the one of the server archive of a release.
- Somewhere to keep the key of the server, which it makes at its first start: `server.key` in the current
  directory by default, or `--key <file>`. See [Manage the server key](generate-the-server-keys.md).

## Steps

1. **Check the configuration.** `--check` loads the key, or makes it the first time, and the game configuration (the `config/` folder next
   to the server, or `--config <dir>`, see [Configure enemies and weapons](configure-enemies-and-weapons.md))
   and opens the port, then exits, without running the server:

    ```sh
    ./build/bin/r-type_server --key keys/server.key --check
    ```

    ```
    INFO  master   Game configuration /home/me/Rtype/build/bin/config: 2 projectiles, 2 weapons, 1 ships, 2 enemies, 2 waves
    INFO  master   Lobby on TCP 0.0.0.0:4242, games on UDP 4243-4299 (57 at most)
    INFO  master   Server key 5b61c7a9ce1a44bfc64fec2013a4a09b6b4d5a8db4129eff890a1462c7ea1f1c
    INFO  master   Check passed: the server can start
    ```

    The server key is the fingerprint the players are shown at their first connection, and trust. Keep
    `keys/server.key`: with another key, they refuse the server.

2. **Open the ports** if players come from another machine: the TCP port of the lobby (4242), and the UDP
   range of the games (4243 to 4299), in the firewall of the machine and, at home, as port forwarding on the
   router. Without the UDP range, players reach the lobby but no game: their client gives up after 5 seconds.

    === "Linux (ufw)"

        ```sh
        sudo ufw allow 4242/tcp
        sudo ufw allow 4243:4299/udp
        ```

    === "Windows"

        ```powershell
        New-NetFirewallRule -DisplayName "R-Type lobby" -Direction Inbound -Protocol TCP -LocalPort 4242 -Action Allow
        New-NetFirewallRule -DisplayName "R-Type games" -Direction Inbound -Protocol UDP -LocalPort 4243-4299 -Action Allow
        ```

3. **Start the server**:

    ```sh
    ./build/bin/r-type_server --key keys/server.key
    ```

    Other ports, fewer games, more logs:

    ```sh
    ./build/bin/r-type_server --key keys/server.key --port 5000 --game-ports 5001-5010 --log-level debug
    ```

4. **Stop it** with Ctrl+C, or `kill <pid>` (SIGTERM), or `systemctl stop` / `docker stop` when it runs as a
   service. It stops properly, which may take a few seconds once games run; a second Ctrl+C stops it at once.

## Result

The server listens for the players until it is stopped, and exits with code 0. If it cannot start, it says why
and exits with code 1:

| Message | Fix |
|---|---|
| `Cannot write the key keys/server.key: Permission denied` | The server cannot write its first key there: give a path it can write with `--key` |
| `Cannot read the key keys/server.key: Permission denied` | Run the server as the user who owns the key |
| `... is not a key of the server` | The file is not an ECDSA P-256 private key: give the right file, or move it away and the server makes a new key |
| `Configuration: enemies/x.lua: ...`, then `The game configuration in ... is not valid` | Fix the file and the field it names ([fields](configure-enemies-and-weapons.md#fields)), or give another folder with `--config` |
| `Cannot listen on 0.0.0.0:4242: Address already in use` | Another server runs on that port: stop it, or choose another with `--port` |

Once it runs, the games it starts show in its logs. Players who create a game and get `GAME_START_FAILED` point
to its instances: `Cannot open UDP port` means another program uses the game ports, choose others with
`--game-ports`.

Every option is listed by `r-type_server --help` and in [Server](../architecture/server.md).
