# How to generate the server keys

This guide makes the key pair the server proves its identity with, and puts each key where it belongs. It is
for anyone who runs a server, for development or for players.

The client only talks to a server that has the **private key** matching the **public key** it was given:
another server, or an attacker between them, is refused
([ADR 0014](../decisions/0014-encryption.md#tcp-tls-13-server-key-pinned),
[TLS](../architecture/networking.md#tls)).

## Prerequisites

- The project builds: see [Build](../getting-started/build.md). The tool, `r-type_keygen`, is built with the
  server and packaged with it.

## Steps

1. **Generate the keys**, in the directory of your choice (created if missing):

    === "Linux"

        ```sh
        ./build/bin/r-type_keygen --out keys
        ```

    === "Windows"

        ```powershell
        .\build\bin\Release\r-type_keygen.exe --out keys
        ```

    ```
    Private key: keys/server.key  (keep it on the server only, never share it)
    Public key:  keys/server.pub  (give it with the client)
    Fingerprint: 5b61c7a9ce1a44bfc64fec2013a4a09b6b4d5a8db4129eff890a1462c7ea1f1c
    ```

    The tool does not replace keys that already exist: see [Replacing the keys](#replacing-the-keys).

2. **Keep `server.key` on the server**, which loads it with its `--key` option
   ([Run a server](run-a-server.md)). On Linux, the file is only readable by the user who made it (`rw-------`): run the server as
   that user. On Windows, the file gets the access rights of its directory, so put it in a directory only the
   server's account can read.

    Never commit it, never send it: anyone who has it can pretend to be the server. Git ignores `*.key` files.

3. **Give `server.pub` with the client**, next to its executable. It is public: it can be committed, sent and
   published freely.

4. **Check the fingerprint** if the public key went through someone else's hands. The fingerprint shown by
   the tool must be the same as the one of the file the clients have:

    ```sh
    r-type_keygen --fingerprint server.pub   # or server.key, which gives the same fingerprint
    ```

    Without the tool, OpenSSL gives the same value:

    ```sh
    openssl pkey -pubin -in server.pub -outform DER | sha256sum
    ```

## Result

The server has `server.key`, the clients have `server.pub`. When a client connects, the server proves in the
TLS handshake that it has the private key, and the client checks that its public key is the pinned one. If
they do not match, the client refuses the server, with `NetworkError::UntrustedServer` and an error in the
logs naming both fingerprints.

## Replacing the keys

Replace the keys only if they must change: every client keeps refusing the server until it gets the new
`server.pub`.

```sh
r-type_keygen --out keys --force
```

**If `server.key` leaked** (committed, sent, copied to a machine you do not trust), replace the keys at once
and give the new `server.pub` to every client: whoever has the old private key can pretend to be the server
to the clients that still pin the old key. Removing the file from git is not enough, since it stays in the
history.

## Reference

```
r-type_keygen [--out <directory>] [--force]
r-type_keygen --fingerprint <file>
```

| Option | Effect |
|---|---|
| `-o`, `--out <directory>` | Where to write `server.key` and `server.pub`, created if missing. The current directory by default. |
| `-f`, `--force` | Replace the keys already there. Without it, nothing is written if either file exists. |
| `--fingerprint <file>` | Show the fingerprint of `server.key` or `server.pub`, and write nothing. |
| `-h`, `--help` | Show the usage. |

| Exit code | Meaning |
|---|---|
| `0` | Done |
| `1` | The keys could not be written (they exist, or the directory cannot be written) or read |
| `2` | Invalid arguments |

Errors go to the logs, on the error output; set `RTYPE_LOG_LEVEL=debug` for more detail
([Logging](../architecture/logging.md)). The keys are ECDSA P-256 keys in PEM, which OpenSSL and other tools
read.
