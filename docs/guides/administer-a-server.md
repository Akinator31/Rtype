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
   panel shows its overview, and **Live** at the top right. A page of the panel opened without a session asks
   for the token instead: paste the address of `admin-url.txt`, or **Read admin-url.txt** to choose the file.

    From another machine, the panel is not reachable: it listens on the loopback of the server only. Go through
    an SSH tunnel, then open the same address:

    ```sh
    ssh -L 8080:127.0.0.1:8080 <server>
    ```

3. **Watch**. The pages update by themselves, without a reload:

    | Page | What it shows |
    |---|---|
    | Overview | The players connected, the games running and those stuck, the banned addresses, the uptime, each opening its page; the games; the live events; the players of each minute of the last hour since the panel opened; the latest actions |
    | Games | The running games: their seats, wave, time of play, state; searched and filtered. **Stuck** means the instance sent no status for 3 seconds: it, or its game, is stuck, and a red strip offers to end it. Click a line to open the game |
    | A game | Its pilots with their address, score, state and latency; its wave and time of play; its own events |
    | Players | Every player of the lobby, its address, its game; click a line for the card of its player, and the other pilots of its address |
    | Bans | The addresses banned now and the time left of each; those lifted or expired lately |
    | Messages | The messages sent |
    | History | What the administrators did, newest first, 100 at a time; click a line for its details |
    | Settings | The version and the ports of the server, and the preferences of the panel |

4. **Act**. Removing a player, ending a game, banning and lifting a ban ask for a confirmation and an optional
   reason, recorded with it; a toast then says what happened at the top right, and the action shows in
   **History**:

    | To | Where | What happens |
    |---|---|---|
    | End a game | A game, **End the game**; Games, **End** | The game stops; its players are back in the lobby, told why |
    | Send a message | **Messages**, to the whole lobby or to a game; a game, **Message to the game** | Each player sees it in the game. One line, 200 bytes at most |
    | Remove a player | Players, or a game, **Remove** | The player leaves its game and the server. It may connect again |
    | Ban an address | Players, **Ban**; a game, **Ban the address**; Bans, **New ban** | From 1 second to 7 days, across restarts: the server refuses the address, and removes the players connected from it, whom the panel lists first |
    | Lift a ban | Bans, **Lift** | The server welcomes the address again at once |

    A ban is temporary: an address is shared, by a household or a network, and changes. In **Settings**, turn
    **Confirm the actions** off to remove and end without the window, and **Sound alert** on to hear an
    instance become stuck; both are kept in this browser only.

## Result

The panel shows the server as it is, and every action is in the record, with its reason and `local` as its
actor: the token of the machine.

| Message | What to do |
|---|---|
| The panel is not built | The server has no `admin/` beside it: use the server archive, or install the panel ([web/admin/README.md](https://github.com/Akinator31/Rtype/blob/main/web/admin/README.md#build)) |
| This link is not valid | The address is of a previous start: the server writes a new one at each start, read `admin-url.txt` again |
| Session expired, in a window over the page | The session ended, or the server restarted: paste the new address of `admin-url.txt`, or open it |
| Connection lost, at the top of the page | The server does not answer: the panel tries again by itself, **Retry** at once, and shows its last state meanwhile. If it lasts, check that the server runs |

Without a browser, the same session drives the [administration API](../protocol/admin-api.md) with `curl`
([With curl](../protocol/admin-api.md#6-with-curl)). How the panel works: [The panel](../architecture/panel.md).
