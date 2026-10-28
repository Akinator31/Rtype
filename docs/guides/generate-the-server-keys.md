# How to manage the server key

This guide shows where the key the server proves its identity with comes from, how to give its fingerprint to
the players, and how to keep or replace it. It is for anyone who runs a server.

The client only talks to a server that has the **private key** matching a key it trusts: another server, or
an attacker between them, is refused
([ADR 0014](../decisions/0014-encryption.md#tcp-tls-13-server-key-pinned),
[TLS](../architecture/networking.md#tls)). Each server makes its own key at its first start, and each player
trusts it at the first connection, as with SSH
([ADR 0021](../decisions/0021-server-generated-key-trust-on-first-use.md)). Players never handle a key file.

## Prerequisites

- A server that runs: see [Run a server](run-a-server.md).

## Steps

1. **Start the server.** If the file of its `--key` option (`server.key` in the current directory by
   default) does not exist, the server generates a key there, and says so:

    ```
    [WARN] [master] No key in server.key: a new one was generated there. Keep this file: with another key, the players who trusted this server refuse it
    [INFO] [master] Server key 5b61c7a9ce1a44bfc64fec2013a4a09b6b4d5a8db4129eff890a1462c7ea1f1c
    ```

    The next starts load the same key, and only log its fingerprint. A file that exists is never replaced,
    even if it holds no key: the server stops and says why.

2. **Keep `server.key`** where the server finds it at each start. On Linux, the file is only readable by the
   user who started the server (`rw-------`): run the server as that user. On Windows, the file gets the
   access rights of its directory, so put it in a directory only the server's account can read.

    Never commit it, never send it, never put it in an environment variable or a Docker image: anyone who has
    it can pretend to be the server. Git ignores `*.key` files.

    !!! warning "In a container"

        Put the key in a **persistent volume** (`--key /data/server.key`). A container that starts without
        its key makes a new one, and every player who trusted the server refuses it. A server that logs
        *a new one was generated* at each start has lost its key each time.

3. **Give its fingerprint to the players**, by another way than the game (a message, a website):

    ```sh
    ./build/bin/r-type_server --fingerprint --key server.key
    ```

    At the first connection, the client shows the fingerprint of the server and asks the player to trust it:
    a player who checks that both are the same is sure to talk to your server. `--fingerprint` never
    generates a key. OpenSSL gives the same value:

    ```sh
    openssl pkey -in server.key -pubout -outform DER | sha256sum
    ```

## Result

The server has its key, and each player trusts its fingerprint after the first connection, kept in the settings
of the client. When a client connects, the server proves in the TLS handshake that it has the private key, and
the client checks that its public key is the one it trusts. Otherwise, the client refuses the server before
sending it anything, and a player who trusted another key for this server sees a warning naming both.

## Replacing the key

Replace the key only if it must change: stop the server, move `server.key` away, and start the server, which
makes a new one. The players who trusted the old key are warned that it changed, and must forget it
(**Forget the server key**, in the Online menu) before trusting the new one. Tell them beforehand, with the
new fingerprint.

**If `server.key` leaked** (committed, sent, copied to a machine you do not trust), replace it at once:
whoever has the old private key can pretend to be the server to the players who still trust it. Removing the
file from git is not enough, since it stays in the history.

## Reference

| Command | Effect |
|---|---|
| `r-type_server [--key <file>]` | Starts the master with the key of the file, generated there if the file does not exist. The directories of the path are created if missing. |
| `r-type_server --check [--key <file>]` | The same check as a start, key generation included, then exits. |
| `r-type_server --fingerprint [--key <file>]` | Shows the fingerprint of the key, and exits: `1` if the file cannot be read or holds no key. |

The key is an ECDSA P-256 private key in PEM, which OpenSSL and other tools read. Errors go to the logs, on
the error output ([Logging](../architecture/logging.md)).
