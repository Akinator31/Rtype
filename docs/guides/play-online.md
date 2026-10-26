# Play online

Run a server and play the same game from several machines, or from several clients on one machine.

## Steps

1. **Start the server** ([Run a server](run-a-server.md)). At its first start, it makes its key, in
   `server.key` ([Manage the server key](generate-the-server-keys.md)):

    ```sh
    ./build/bin/r-type_server
    ```

    For players on other machines, open the TCP port 4242 and the UDP ports 4243 to 4299.

2. **Connect.** Start the client, choose **Online**, type a name and the server, then **Connect**:

    | Field | Example | Meaning |
    |---|---|---|
    | Name | `Pavel` | 1 to 16 characters, shown to the other players |
    | Server | `127.0.0.1:4242` | The address of the server and its port; the port is 4242 if omitted |

3. **Trust the server**, the first time only. A server the client does not know yet shows its fingerprint:
   compare it with the one its owner gives (`r-type_server --fingerprint`), then **Trust and connect**. The
   client remembers it in its save, and connects at once the next times.

4. **Do not trust a key that changed.** A known server that presents another key is refused, with a warning
   that shows both: someone may be pretending to be it. If its owner did change its key, **Forget the server
   key** in the Online menu, then connect again.

5. **Play.** In the lobby, **Create a game**, or **Join** one of the list (**Refresh** to see the new ones). A
   second client, on this machine or another, joins the same game: each player has a ship of its colour.
   Escape opens the pause menu, where **Leave the game** goes back to the list.

## Result

The players see each other's ships move. The server shows them in its logs:

```
INFO  lobby    Pavel (1) joined the lobby from 127.0.0.1:51234
INFO  games    Pavel creates game 1 "Pavel's game" for 4 players: instance 48213 on UDP 4243
INFO  games    game 1 "Pavel's game" ready on UDP 4243
INFO  instance Pavel (1) connected from 127.0.0.1:40102, slot 0
```

| Toast in the client | Fix |
|---|---|
| `Cannot reach ...: Connection refused` | The server does not run, or the address or the TCP port is wrong, or blocked |
| `The game cannot be reached: its UDP port may be blocked` | The lobby works but not the games: open the UDP range of the server |
| `The server cannot host another game` | Every game port is taken: wait for a game to end, or give the server more ports |
