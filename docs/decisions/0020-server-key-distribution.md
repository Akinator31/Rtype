# 0020 — Distribution of the server keys

| | |
|---|---|
| **Status** | Accepted |
| **Date** | 2026-10-25 |

## Context

[ADR 0014](0014-encryption.md) protects the lobby with TLS 1.3 and **pins** the key of the server: the client
only accepts a server whose public key has the expected SHA-256 fingerprint. It leaves open how the client
gets that fingerprint, and settles for a `server.pub` file next to the executable. In practice:

- **Players must not handle key files.** A player downloads the client and plays: copying a `server.pub`, or
  changing it to play on a friend's server, is not something a player should do, nor understand.
- **There is one official server**, hosted by the team on its own infrastructure, under a domain name. Most
  players only ever play there.
- **Anyone can run a server**: classmates, the evaluators, a LAN. The client must reach them too, without a
  file from each of them, and without giving up the protection against a man-in-the-middle.
- **Development** needs a server and a client that trust each other out of the box, on any machine and in the
  CI, with no key to generate first.
- **The private key of the official server** must stay secret: anyone who has it can impersonate the server
  to every client.

See [Networking concepts](../architecture/networking-concepts.md#security) for the vocabulary.

## Options

### How the client knows the key of a server

| | A. Built into the client | B. A list of servers shipped in a file | C. Trust on first use (TOFU) | D. Certificates from an authority (Let's Encrypt) | E. `server.pub` next to the client (today) |
|---|---|---|---|---|---|
| What the client trusts | The fingerprints compiled in | The fingerprints of the file | The key seen at the first connection, then that one only | Any key an authority certified for the domain | The key of the file |
| The player handles | Nothing | Nothing | One confirmation per new server | Nothing | A file per server |
| Man-in-the-middle | Prevented | Prevented, unless the file is changed | Prevented after the first connection; possible **at** the first one, if the player confirms without checking | Prevented, if none of the ~150 authorities trusted is fooled | Prevented |
| A new key on the server | A new client | A new file | The player is warned, and must forget the old key | Transparent | A new file |
| Other servers | No | Edit the file | Yes | Only with a domain and a certificate | Replace the file |
| Work | A header | A file format, its parsing | A confirmation screen, storage of the keys | Renewal every 90 days, a store of authorities in the client, checking the chain, the dates and the host name | None |
| Precedent | Mobile apps pinning their API | Game clients with a server list | SSH (`known_hosts`) | The web | — |

A domain name does not call for D: it only finds the server, through DNS. The pinned key is what proves its
identity, and a forged DNS answer leads to a server that cannot present the key.

### Where the private key of the official server lives

| | In git | In an environment variable | Generated at each start | A file given with `--key` | A Docker secret |
|---|---|---|---|---|---|
| Who can read it | Anyone with the repository, forever (history) | Anyone who can run `docker inspect` or read `/proc/<pid>/environ` | — | The server's user | The container only, in `/run/secrets/` |
| Stable across restarts | Yes | Yes | **No**: every client refuses the new key | Yes | Yes |

## Decision

**A for the official server, with a backup key pinned too; C (trust on first use) for every other server; a
committed development key pair, trusted by Debug builds only. The private key of the official server is a file
on its host, given with `--key`, never in git nor in an environment variable.** The client no longer reads
`server.pub`.

### The official server: built into the client

- Its address (domain name and port) and **two fingerprints** are compiled into the client: the key the server
  uses, and a **backup key**. The client accepts either.
- The Online menu offers it first, in one click. The player never sees a key.
- The backup key pair is made at the same time as the main one. Its private key is **never installed** on the
  server: it is kept offline (an encrypted USB key, a password manager), with a copy of the main one. If the
  main key leaks or is lost, the server switches to the backup key, and the clients already out keep working;
  the next release pins a new backup key and drops the old one.
- The fingerprints are public: they live in the source code, and changing them is a commit.

### Other servers: trust on first use

The player types the address of the server, as today.

1. **A known server** (an official one, or one already trusted) is connected to with its fingerprints, as
   pinned servers are.
2. **An unknown server**: the client connects with no accepted key. The TLS handshake fails, as with any
   unexpected key, and reports the fingerprint the server presented. Nothing is ever sent to a server that is
   not trusted: there is no "accept any key" mode.
3. The client shows that fingerprint and asks the player to trust it. The player can compare it with the one
   the owner of the server gives (`r-type_keygen --fingerprint`).
4. If the player trusts it, the client stores it in the save, under the address of the server
   (`servers.known.<host>:<port>`, see [Save](../architecture/save.md)), and connects again, pinning it.
5. **A known server that presents another key** is refused, with a warning that names both fingerprints: it
   is either a new key or an attack, and the client cannot tell. The player must explicitly forget the server
   before trusting it again; the warning itself offers no way to accept the new key.

The keys are stored by the address the player typed: the same server under another name or IP address is a
new server.

### Development: a committed key pair

- A key pair for development is committed, in `keys/development/`. Its private key is **public**, by design:
  it only protects local games and tests.
- **Debug builds** trust its fingerprint for any address, and a Debug server uses it when it is given no key
  and finds no `server.key`, with a warning in its logs. A clone builds and plays on its own, the CI too.
- **Release builds** never trust it: a release client refuses a server that presents it, even through trust
  on first use, and a release server refuses to start with it. A server deployed with the development key by
  mistake is caught at once instead of being open to anyone.

### The private key of the official server

| Where | What |
|---|---|
| The host | Generated there, with `r-type_keygen`, readable only by the server's user, given with `--key`. Later, in Docker, mounted as a secret (`/run/secrets/server_key`). |
| Backup | The main and the backup private keys, kept offline. The backup key is never on the host. |
| Never | In git (`*.key` stays ignored, with the development key as the only exception), in an environment variable, in a Docker image, sent in a message. |

The procedure goes in the [server keys guide](../guides/generate-the-server-keys.md).

## Consequences

- Players never handle a key: one click for the official server, one confirmation for any other.
- Nothing is lost compared with ADR 0014 for the official server, which stays pinned. Other servers are
  protected from their second connection on; the first one is as safe as the check the player makes.
- No certificate to renew, no authority to trust, no change to the handshake: the self-signed certificate of
  ADR 0014 stays.
- `TlsConnection` accepts a **set** of fingerprints instead of one (main and backup keys), and reports the
  fingerprint a refused server presented.
- Changing the key of the official server without its backup key means a new release of the client: the
  backup key is what avoids it, so it must be made and kept before the first release.
- The development key is in the repository: the Release checks above are what keeps it out of production, and
  they are tested.
- If the team ever runs many official servers, D can be reconsidered in a new record; the list of official
  servers already makes adding a second one a matter of one entry.
