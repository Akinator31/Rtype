# 0026 — Discord sign-in through the Discord Social SDK in the client

| | |
|---|---|
| **Status** | Accepted |
| **Date** | 2026-10-30 |

## Context

[ADR 0024](0024-player-accounts.md) chose to let the web server of the master receive the answer of Discord:
the master gave the client the address of the authorization page, the browser came back to
`/auth/discord/callback`, and the page showed a code of 6 characters the player typed in the game. The code was
there so that someone who opens the link of another signs nobody in.

That flow asks a lot of the player (a code to copy from a page to the game) and of the host (the web server of
the master reachable from the Internet before any player can sign in with Discord). Discord publishes a
**Social SDK** for games (C++, Linux, Windows, macOS), whose `Client::Authorize` asks the player in the Discord
client when it runs, in the browser otherwise, and receives the code itself on the loopback of the player.

The SDK is proprietary: a shared library (14 MB on Linux, 10 MB on Windows) and its headers, downloaded from the
developer portal of Discord with an account, so not through CPM.

## Options

| | A. Callback on the master, code typed (ADR 0024) | B. Social SDK in the client, code exchanged by the master |
|---|---|---|
| What the player does | Browser, copies a code from a page, types it | Accepts in Discord or the browser, comes back: nothing to type |
| Who receives the code of Discord | The web server of the master, from the Internet | The SDK, on the loopback of the player |
| Someone opening the link of another | Learns a code they must give to the other | Sends the code to the loopback of the other, which they do not reach; PKCE binds it to a verifier only the client of the other holds |
| The secret of the application | In the master | In the master: the client sends the code and the verifier, the master exchanges them |
| Dependencies | libcurl in the master | libcurl in the master, the SDK in the client |
| The panel of the administrators (step 12) | Same callback | Still needs the callback: a browser has no SDK |

## Decision

**B: the Discord Social SDK in the client, the code exchanged by the master.**

1. `DISCORD_SIGN_IN`: the master answers `DISCORD_AUTHORIZE` with the id of its Discord application, and
   starts an attempt on that connection, valid ten minutes.
2. The client calls `Client::Authorize` with that id, the scope `identify` only, and the challenge of a code
   verifier of PKCE it keeps.
3. `DISCORD_CONFIRM` carries the authorization code, the verifier and the redirect address Discord used. The
   master exchanges them for a token with the secret of the application (libcurl), asks `/users/@me`, and drops
   the token: it keeps the id of the user only.
4. The master signs the connection in, links the identity to the account signed in, or asks for the username of
   a new account (`CHOOSE_USERNAME`).

Nothing signs in before `DISCORD_CONFIRM`, which needs a `DISCORD_SIGN_IN` of the same connection: a code given
to another connection is refused. Five codes refused by Discord need a new `DISCORD_SIGN_IN`.

- **The SDK is kept in the repository**, in `libs/discord/sdk/`: the headers, the library for Linux x86-64 and
  Windows x64, and its notices. The repository is private; the CI runners and every teammate build the client
  without an account on the developer portal. The other files of the archive (debug builds, macOS, mobile,
  voice) are left out.
- **The tests never load it**: the client knows an interface (`rtype::discord::Authorizer`), which the
  executable implements with the SDK (`rtype::discord::SocialSdkAuthorizer`); the tests give what the SDK would
  give, and a fake Discord answers the master.
- **Lobby protocol 3 changes** before it is released: `DISCORD_URL` becomes `DISCORD_AUTHORIZE`, `DISCORD_CONFIRM`
  carries the code, the verifier and the redirect address, and `DISCORD_UNAVAILABLE` tells that the master
  cannot reach Discord. The version stays 3: no client or master of version 3 was released.

## Consequences

- Players sign in with Discord in two clicks; a host enables Discord with its application id and secret, the
  application set up as a confidential client with the redirect `http://127.0.0.1/callback`.
- The master no longer needs to be reachable from the Internet for players to sign in with Discord. The panel
  still does, for its administrators, in step 12.
- The client is built for Linux x86-64 and Windows x64 only, the platforms the SDK is kept for. On the
  others, CMake warns and leaves the client out, so that the server still builds there (its image for
  linux/arm64). The Linux library needs PulseAudio and ALSA at run time (`libpulse0`, `libasound2`), even though
  the voice of the SDK is not used.
- A new version of the SDK replaces the files of `libs/discord/sdk/` in one commit, with its version in
  `libs/discord/sdk/README.md`.
- The sign-in with Discord of [ADR 0024](0024-player-accounts.md) (its options table and its four steps) is
  superseded by this record; the rest of ADR 0024 stands.
