# Getting started

This section gets you from a fresh clone to a running client and server.

1. [Build the project](build.md) on Linux or Windows.
2. Start the client, and play alone: **Play → Solo**.

    ```sh
    ./build/bin/r-type_client
    ```

3. To play together, start a server ([Run a server](../guides/run-a-server.md)), then **Online** in one client
   per player ([Play online](../guides/play-online.md)).

    ```sh
    ./build/bin/r-type_server --key keys/server.key
    ```

4. [Play the game](play.md): the controls, the modes, and everything the game offers.

!!! tip "Prebuilt binaries"

    Released versions of the client and the server, for Linux and Windows, are available on the
    [Releases page](https://github.com/Akinator31/Rtype/releases). Every push also builds the project; its
    archives can be downloaded from the **Artifacts** section of the corresponding run in the
    [Build workflow](https://github.com/Akinator31/Rtype/actions/workflows/build.yml).
