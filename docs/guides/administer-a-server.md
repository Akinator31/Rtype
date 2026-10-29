# Administer a server

Open the panel of a running server, watch its games and its players live, and act on them: end a game, send a
message, remove a player, ban an address. Without accounts, the panel is opened from the machine of the server,
with the address the server writes at each start.

## Prerequisites

- A server running ([Run a server](run-a-server.md)), from the server archive of a release or of the CI: the
  panel is in it, `admin/` beside `r-type_server`. A server built from the sources has it once the panel is
  built and installed ([web/admin/README.md](https://github.com/Akinator31/Rtype/blob/main/web/admin/README.md#build));
  without it, the page says the panel is not built.
- A browser on the machine of the server, or an SSH access to it.

## Steps

1. **Find the address of the panel**: at each start, the server writes it with a new token to `admin-url.txt`,
   beside its key, readable by its owner only. Its log says where:

    ```
    INFO  master   Panel at http://127.0.0.1:8080: open the address written in /data/admin-url.txt
    ```

    ```sh
    cat /data/admin-url.txt
    # http://127.0.0.1:8080/?token=9b47…
    ```

2. **Open it** in a browser. The token is traded for a session of 12 hours, then leaves the address bar: the
   panel shows its overview, and **Live** at the top right.

    From another machine, the panel is not reachable: it listens on the loopback of the server only. Go through
    an SSH tunnel, then open the same address:

    ```sh
    ssh -L 8080:127.0.0.1:8080 <server>
    ```

3. **Watch**. The pages update by themselves, without a reload:

    | Page | What it shows |
    |---|---|
    | Overview | The players, those in a game, the games, the banned addresses; the version, uptime and ports; the games over time since the panel opened; the latest events |
    | Games | The running games: their players over their seats, their wave, their age, their state. **Stale** means the instance sent no status for 3 seconds: it, or its game, is stuck |
    | A game | Its players with their score, whether alive, their latency; its wave and tick |
    | Players | Every player of the lobby, its address, its game |
    | Actions | What the administrators did, newest first, and the addresses banned now |

4. **Act**. Each action asks for a confirmation and an optional reason, recorded with it; the panel then says
   what happened at the top of the page, and the action shows in **Actions**:

    | To | Where | What happens |
    |---|---|---|
    | End a game | A game, **End the game** | The game stops; its players are back in the lobby, told why |
    | Send a message | The overview, **Message everyone**; a game, **Message the game** | Each player sees it in the game. One line, 200 bytes at most |
    | Remove a player | Players, or a game, **Remove** | The player leaves its game and the server. It may connect again |
    | Ban an address | Players, **Ban** | For an hour, a day or seven days, across restarts: the server refuses the address, and removes the players connected from it |

    A ban is temporary: an address is shared, by a household or a network, and changes.

## Result

The panel shows the server as it is, and every action is in the record, with its reason and `local` as its
actor: the token of the machine.

| Message | What to do |
|---|---|
| The panel is not built | The server has no `admin/` beside it: use the server archive, or install the panel ([web/admin/README.md](https://github.com/Akinator31/Rtype/blob/main/web/admin/README.md#build)) |
| This link is not valid | The address is of a previous start: the server writes a new one at each start, read `admin-url.txt` again |
| Session expired, at the top of the page | The session ended, or the server restarted: open the address of `admin-url.txt` again |
| Connection lost, at the top of the page | The server does not answer: the panel tries again by itself and shows its last state meanwhile. If it lasts, check that the server runs |

Without a browser, the same session drives the [administration API](../protocol/admin-api.md) with `curl`
([With curl](../protocol/admin-api.md#6-with-curl)). How the panel works: [The panel](../architecture/panel.md).
