# Play online

Run a server, give its key to the clients, and play the same game from several machines, or from several
clients on one machine.

## Steps

1. **Make the keys of the server**, once ([Generate the server keys](generate-the-server-keys.md)):

    ```sh
    ./build/bin/r-type_keygen --out keys
    ```

2. **Start the server** with its private key ([Run a server](run-a-server.md)):

    ```sh
    ./build/bin/r-type_server --key keys/server.key
    ```

    For players on other machines, open the TCP port 4242 and the UDP ports 4243 to 4299.

3. **Give the public key to each client**: copy `keys/server.pub` next to the executable of the client, as
   `server.pub`.

    ```sh
    cp keys/server.pub build/bin/server.pub
    ```

    On another machine, copy it next to its `r-type_client`. The key is public: it can be sent by any means.

4. **Connect.** Start the client, choose **Online**, type the server and a name, then **Connect**:

    | Field | Example | Meaning |
    |---|---|---|
    | Server | `127.0.0.1:4242` | The address of the server and its port; the port is 4242 if omitted |
    | Name | `Pavel` | 1 to 16 characters, shown to the other players |

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
| `server.pub is missing next to the client` | Copy the public key of the server next to the executable (step 3) |
| `Cannot reach ...: Connection refused` | The server does not run, or the address or the TCP port is wrong, or blocked |
| `This is not the expected server` | `server.pub` is not the key of this server: copy the right one |
| `The game cannot be reached: its UDP port may be blocked` | The lobby works but not the games: open the UDP range of the server |
| `The server cannot host another game` | Every game port is taken: wait for a game to end, or give the server more ports |
