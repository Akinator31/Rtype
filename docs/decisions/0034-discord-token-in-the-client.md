# 0034 — The Discord token of the player given to its client

| | |
|---|---|
| **Status** | Accepted |
| **Date** | 2026-11-09 |

## Context

[ADR 0026](0026-discord-social-sdk.md) signs the players in with the Discord Social SDK of the client: the
client asks the player with the scope `identify`, the master exchanges the code with the secret of its
application, asks who the player is, and **drops the token**.

The rest of the SDK (lobbies and their voice, relationships, invitations) needs the client to be connected to
Discord as the player: `Client::UpdateToken` then `Client::Connect`, with an access token of the scopes
`openid sdk.social_layer`. The application of a host is confidential: only the master holds its secret, so only
the master can exchange a code, or renew a token from its refresh token.

## Options

| | A. The master keeps the tokens | B. The master gives them to the client, keeps neither | C. A public client, the SDK exchanges the code itself |
|---|---|---|---|
| Who holds the access token | The master, which acts for the client | The client of the player | The client of the player |
| The secret of the application | In the master | In the master | None: the application is public, any client can claim it |
| What a stolen database gives | Every player's Discord | Nothing of Discord | Nothing of Discord |
| Renewing the token | The master, alone | The client sends its refresh token, the master renews it | The client, alone |
| The sign-in of ADR 0026 | Unchanged | Unchanged | Rewritten: the master has nothing to exchange |

## Decision

**B: the master gives the Discord tokens to the client, and keeps neither.** Lobby protocol 6:

1. `[discord] social = true` in the settings of the master asks `identify openid sdk.social_layer` in
   `DISCORD_AUTHORIZE` (its new field `scopes`); without it, `identify` alone, as before.
2. Once the exchange of `DISCORD_CONFIRM` signs the connection in or links the identity, the master sends
   `DISCORD_TOKEN` (access token, refresh token, expiry) before the reply. A new Discord user gets it only once
   its account exists, before the `SIGNED_IN` of `CHOOSE_USERNAME`: a token always goes to the account of its
   identity.
3. The client keeps the refresh token per server, beside its session token, and connects the SDK with the access
   token. At its next connection, signed in by its session token, or when the token expires, it sends
   `DISCORD_REFRESH`: the master renews it with its secret, checks that the token is the one of the identity
   of the account signed in, and answers `DISCORD_TOKEN`, or `DISCORD_TOKEN_REFUSED`, after which the client
   forgets it.

## Consequences

- The point 3 of ADR 0026, "drops the token", is superseded on a master with `social`; the rest of ADR 0026
  stands.
- The tokens cross the TLS connection of the lobby and live in the client only: never logged, never in the
  database. The settings file of the client holds the refresh token (`servers.discord.<address>`), readable by
  the player only, forgotten at the sign-out and the deletion of the account.
- A player signed in with a password only, or a guest, gets no token: no social feature of Discord, the voice of
  the game instead.
- At most five `DISCORD_REFRESH` per connection, which the master counts.
- The communication features of the Social SDK are in limited access: an application not approved by Discord
  can use them only for its own team (the members of the application on the developer portal). The guide of
  the hosts says so.
- Once connected, the client of the SDK is the one of the application of the master: the rich presence is shown
  as that application while the player is on that server.
