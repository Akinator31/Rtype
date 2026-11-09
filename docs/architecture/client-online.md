# The client online

How the client joins the lobby of a master and plays a game of it: the same game scene as offline, fed by a
`RemoteSession` instead of a `LocalSession` ([A game in the client](client-game.md),
[ADR 0015](../decisions/0015-one-game-scene-local-or-remote-session.md)). To set up a server and play, see
[Play online](../guides/play-online.md).

## Code layout

```
apps/client/src/
  Online/
    LobbyClient.cpp    the connection to the lobby: TLS with the key pinned, HELLO, the requests
    OnlineScenes.cpp   the Online menu, the servers met, their keys, the list of the games, the profile
    AccountScenes.cpp  the scenes of the accounts: sign in, create an account, the code, Discord, my account
    OnlineScreen.cpp   the look the Online screens share: header, fields, notes, keys, Discord buttons
    FormErrors.cpp     the field of a form each error of the master is about
    SessionTokens.cpp  the session token of the account signed in on each server, kept in the settings
    KnownProfiles.cpp  the account remembered for each server, and the pictures kept on disk
  main.cpp             the discord::Session of libs/discord, polled every frame, and its SocialSdkAuthorizer
  Game/
    RemoteSession.cpp  a game online: the UDP session with the instance, inputs out, world in
    Interpolation.cpp  the world drawn a little in the past, between the snapshots around that time
    Prediction.cpp     the ship of the player, moved at once by its inputs, then reconciled with the game
    InputClock.cpp     the pace of the inputs, kept just in time by the lead the snapshots report
    ShotPrediction.cpp the shots of the player, drawn from the moment it fires, in the time of its ship
```

The client has one `EventLoop` for its network, polled by the game loop once per frame before the engine
updates: everything network runs on the thread of the game, without locks.

## From the menu to the game

```mermaid
sequenceDiagram
    participant P as Player
    participant C as Client
    participant M as Master
    participant I as Instance
    P->>C: Online, server and name, Connect
    C->>M: TCP, TLS (keys trusted for the server pinned), HELLO (name, language)
    M->>C: WELCOME (features of the server)
    C->>M: LIST_GAMES
    M->>C: GAME_LIST: the lobby is shown
    P->>C: Create a game, or Join
    C->>M: CREATE_GAME or JOIN_GAME
    M->>C: GAME_JOINED (UDP port, session, keys)
    C->>I: CONNECT, then INPUT 60 times per second
    I->>C: ACCEPTED, then SNAPSHOT 30 times per second
```

*The client goes through the master to get a game, then talks to its instance directly.*

1. **Online** asks for the name of the player and the server (`host:port`, the port 4242 by default,
   `[::1]:4242` for IPv6), or offers the servers met before, on the right: those whose key the player trusted,
   each with the account remembered for it ([Profiles](#profiles)). An IP address is used as it is; a host name
   is looked up when Connect is pressed.
2. **The key of the server** is the one `KnownServers` trusts for it ([Trusted servers](#trusted-servers)). A
   server that does not hold a matching private key is refused during the handshake, before `HELLO` leaves: the
   player is asked to trust it if it was never met, warned otherwise.
3. **The lobby** lists the games of the server in rows: a full game is dim and cannot be joined; Create a game
   (for 4 players, named after the player), Refresh. Escape disconnects. The pilot is at the top right, with
   its avatar: My account, or, for a guest, the way to sign in. Creating or joining a game first opens the
   hangar: the game is asked for when the player launches it with a ship.
4. **The game** plays a `RemoteSession` with what `GAME_JOINED` gave. When it ends, by the pause menu or because
   the game ended, the client leaves it (`LEAVE_GAME`) and shows the list again.

On a server with accounts, `WELCOME` leads to the scenes of the [accounts](#accounts) first, unless the player
comes back signed in.

What goes wrong with a field of a form is shown under it, in red (`showError()`, `fieldOf()` in
`Online/FormErrors.hpp`): a wrong password under the password, a username taken under the username, a wrong
code under the code. Everything else is shown in a toast, in the language of the game: a server unreachable or
not the expected one, a game full, a game that crashed, the connection lost. A lobby connection lost brings the
player back to the main menu.

The screens follow the Série 14 of the design: the widgets of the engine for forms, lists and avatars
([Forms](scenes-and-interface.md#forms), [Lists and avatars](scenes-and-interface.md#lists-and-avatars)),
a header with its back arrow, the keys of the screen at the bottom left. The Online menu opens over the main
menu; the screens after Connect have the starry sky of the main menu behind them; the questions (a new key, a
key changed, Discord, the deletion) open over the screen that asks them, under a veil.

What the administrators of the server do is shown in a toast too ([Administration](administration.md)): a
message they send (`Server: <text>`), a game they end (back to the list), a removal from the game then from the
server, an address banned (back to the main menu, with the reason).

## Trusted servers

Players never handle a key file: each server makes its key at its first start, and the player trusts it at
the first connection, as with SSH ([ADR 0021](../decisions/0021-server-generated-key-trust-on-first-use.md)).
`KnownServers` (`Online/KnownServers.hpp`) gives the key the client trusts for a server: the one the player
trusted, kept in the [settings](settings.md) under `servers.known.<address>`, or none for a server never met. A server is known by
its address in lowercase (`serverAddress()`: `rtype.example.org:4242`, `[::1]:4242`): the same server under
another name or IP address is a new server.

When the server presents a key it does not trust, the LobbyClient calls `onUntrusted` with it, and
`KnownServers::judge()` tells what it means:

| Verdict | The client |
|---|---|
| `FirstUse`: a server never met | Shows its fingerprint (`fingerprintLines()`, in groups of 4 digits) in the scene `online_trust`. **Trust and connect** remembers it (`trust()`, which writes the settings) and connects again |
| `Changed`: a server trusted with another key | Refuses it, and shows the key expected and the key presented in the scene `online_changed_key`. No button accepts the new key: the player must **Forget the server key** in the Online menu first (`forget()`) |

The settings are in the data directory of the player (`rtype::render::userDataDirectory()`: `%APPDATA%\R-Type` on
Windows, `~/.local/share/r-type` elsewhere), read when the client starts. Settings that cannot be written keep
the key trusted until the client closes, and the client says so.

## LobbyClient

```cpp
rtype::client::LobbyClient lobby(loop, server, known.trusted(address), "Pavel",
    {.onWelcome = [&](std::uint32_t player) { lobby.listGames(showList); },
     .onLeftGame = [&](const rtype::lobby::LeftGameMessage &left) { /* game over, crash, timeout, admin */ },
     .onServerMessage = [&](const std::string &text) { /* toast it */ },
     .onClosed = [&](const std::string &reason) { /* toast it */ },
     .onUntrusted = [&](const rtype::network::KeyFingerprint &presented) { /* ask the player, or warn */ },
     .onSessionExpired = [&] { /* forget the token, connect again as a guest */ },
     .describe = [&](rtype::lobby::ErrorCode code) { return translations.text(rtype::client::errorKey(code)); }},
    "fr", sessions.token(address));
lobby.joinGame(id, [&](const rtype::client::LobbyClient::JoinResult &result) { /* GAME_JOINED or an error */ });
lobby.signIn("Pilot_7", password, [&](const rtype::client::LobbyClient::SignInResult &result) { /* SIGNED_IN */ });
```

- **One callback per request.** The master answers in order ([lobby protocol](../protocol/lobby.md)), so each
  reply goes to the oldest request waiting. An `ERROR` goes to the callback of its request:
  `describe(code)` gives the message for the player.
- **`LEFT_GAME`** is the reply to `LEAVE_GAME`, or a notification when the player did not ask (game over,
  crash, timeout, removed by an administrator, game ended by one): the latter goes to `onLeftGame`.
- **`SERVER_MESSAGE`** is a notification: it goes to `onServerMessage`, never to a request.
- **`ERROR (KICKED)` or `ERROR (BANNED)` with the request 0** answers no request: the master closes the
  connection after it, and `onClosed` gets its `describe()`. `BANNED` in reply to `HELLO` ends the same way.
- **`PING`** is sent after 10 seconds without sending anything, so that the master, which closes a connection
  silent for 30 seconds, keeps it.
- **A name and a locale are checked before connecting** (`std::invalid_argument`): `HELLO` is written in a
  handler of the loop, where nothing may throw. The locale is the language of the game (`en` or `fr`), so that
  the emails of the master come in it.
- **`WELCOME` tells what the master offers** (`features()`): guests, password accounts, Discord; and the
  account the token of `HELLO` signed in (`account()`), whose username is then the name of the player.
- **The requests of the accounts** ([lobby protocol 4](../protocol/lobby.md#6-accounts)): `registerAccount()`,
  `verifyEmail()`, `resendCode()`, `signIn()`, `forgotPassword()`, `resetPassword()`, `signOut()`,
  `deleteAccount()`, and those of Discord, `discordSignIn()` (the application of the master),
  `discordConfirm()` (`SIGNED_IN`, `DiscordConfirmed` for `DONE`, with the profile of a new user for `PROFILE`,
  or the error) and `chooseUsername()`; `profile()` (`GET_PROFILE`) and `picture()`, which asks `GET_AVATAR` until
  it has every part of the picture. Those
  that sign in give `SIGNED_IN` or the error, and sign the connection in; the others give nothing or the error. A field the protocol cannot carry (an empty password, a code that is not 6 digits,
  a username of 17 bytes) gets at once the error the master would give it, without being sent.
- **`DONE` to `SIGN_OUT` or `DELETE_ACCOUNT`**: the master closes the connection, so the client closes it first,
  and `onClosed` is not called: it is not a connection lost.
- **`SESSION_EXPIRED` in reply to `HELLO`** goes to `onSessionExpired`, instead of `onClosed`. Any other error
  after which the master closes the connection (`BANNED` after a `SIGN_IN`, `MALFORMED`) goes to `onClosed`.
- **The messages of the errors**: `describe(code)` in English, for the logs and the tests; `errorKey(code)` the
  key of its message in `en.lang` and `fr.lang` (`error.game_full`), which the scenes show, and give the
  `LobbyClient` through `Handlers::describe`.
- **A password, a code, a code verifier and a token are never logged**, at any level.

## Accounts

On a server with accounts ([lobby protocol 4, section 6](../protocol/lobby.md#6-accounts)), the client offers
what `WELCOME` says the server offers, Discord first. A server without accounts looks as it did before them.

```mermaid
flowchart LR
    W[WELCOME] -->|signed in by its token, or no accounts| L[Lobby]
    W -->|accounts, not signed in| A[online_accounts]
    A -->|Sign in| S[online_sign_in] -->|SIGNED_IN| L
    S -->|EMAIL_NOT_VERIFIED| C[online_code]
    S -->|Forgot password| F[online_forgot_password] -->|CODE_SENT| R[online_reset_password] -->|SIGNED_IN| L
    A -->|Create an account| G[online_register] -->|CODE_SENT| C -->|SIGNED_IN| L
    A -->|Play as a guest, if allowed| L
    A -->|Sign in with Discord| D[online_discord] -->|SIGNED_IN| L
    D -->|PROFILE: a new Discord user| U[online_choose_username] -->|SIGNED_IN| L
    L -->|My account| M[online_my_account] -->|Sign out, Delete| A
    M -->|Link Discord| D
```

*Each scene goes back (Escape) to the one before it; the choice of the accounts disconnects.*

- **The session token** of `SIGNED_IN` is kept for the server by `SessionTokens` (`Online/SessionTokens.hpp`), in
  the [settings](settings.md) under `servers.session.<address>`, the address as `KnownServers` knows it. The
  next **Connect** sends it in `HELLO`, without a name: the player comes back signed in, even after a restart.
- **It is forgotten** at sign-out and at the deletion of the account (`DONE`), then the client connects again as
  a guest; and on `SESSION_EXPIRED` (a session older than 30 days, a new password, a ban, a sign-out from
  another machine), after which the client connects again without it and says so in a toast.
- **The password fields** are `InterfaceBuilder::passwordField()`: drawn as `*`, never copied to the clipboard
  ([Interface](scenes-and-interface.md#forms)). Creating an account shows the strength of the password in a
  gauge, full at the 10 characters the master asks for. A password lives in its field only: it is read when the button is
  pressed, and never kept by the scenes.
- **The settings file** is readable by the player only on Unix (mode `0600`), since it holds the tokens.
- **The language of the game** is sent in `HELLO`, so that the emails of the master come in it; every error
  code has its message in `en.lang` and `fr.lang`.
- **The code of the email** is typed in six cells (`InterfaceBuilder::codeField()`), and sent as soon as they
  are full.
- **My account** shows the avatar of the pilot, its username in chrome, its Discord name, then a sheet: the
  server, the date the account was made, its email (masked: `k***@example.org`) and whether it is verified, its
  password (set, or none for an account made with Discord), its Discord name, its role. **Link Discord**
  (**Refresh from Discord** once linked) on a server that offers it, **Sign out**, and **Delete the account** in
  red, which asks over the sheet, No by default, for the password once more, or **Confirm with Discord** for an
  account made with Discord.

### Discord

On a server whose `WELCOME` offers Discord ([ADR 0026](../decisions/0026-discord-social-sdk.md)):

1. **Continue with Discord** sends `DISCORD_SIGN_IN`; `DISCORD_AUTHORIZE` gives the id of the Discord
   application of the master, and the client opens `online_discord` over the screen: the ring of the logo, and
   the three steps (Discord opened, the answer of the player, the server checking), with **Cancel**.
2. `discord::Authorizer::authorize()` asks the player: the Discord client if it runs, the browser otherwise.
   `SocialSdkAuthorizer` (`libs/discord`) does it with `Client::Authorize` of the Discord Social SDK, scope
   `identify`, and the challenge of a code verifier of PKCE it makes; the SDK receives the code on the loopback
   of the player. Its callbacks run in `Session::poll()`, which the game loop calls every frame, after the
   network.
3. The code, the verifier and the redirect address go in `DISCORD_CONFIRM`. `SIGNED_IN` keeps the token as any
   sign-in does; `PROFILE` leads a new Discord user to `online_choose_username`, its Discord picture, display
   name and `@username` above the username it chooses, proposed from its Discord name; `DONE` brings a player
   signed in back to **My account** (the identity linked, its profile read again), or, for **Confirm with
   Discord**, straight to `DELETE_ACCOUNT` with an empty password.

The player who refuses in Discord, or a Discord that cannot be reached, gets a toast and the scene before. The
SDK starts at the first sign-in with Discord only; its logs go to the logger `discord`, warnings and errors.
Cancel, or the connection lost, gives up the authorization running (`cancel()`): what Discord answers then goes
nowhere.

### Profiles

Signed in, by a token or by a sign-in, the client asks `GET_PROFILE` for its account: its Discord names and the
version of its picture. A picture it does not keep already comes with `GET_AVATAR`, in parts
(`LobbyClient::picture()`); it is kept on disk under its version, in the folder `pictures` of the user data, and
made a round texture (`ResourceManager::loadRoundPicture()`). The lobby and My account show it as soon as it is
there.

`KnownProfiles` remembers the account of each server under `servers.profile.<address>` in the
[settings](settings.md) (its username, whether it is linked to Discord, the version of its picture), so that the
servers met in the Online menu show who will be signed in, with the picture, before connecting. It is forgotten
at sign-out and at the deletion of the account; the pictures stay, as another server may show them.

`libs/discord` is two targets, so that the tests never load the library of Discord: `rtype::discord`, the
interfaces, one per feature, which the client and its tests know, and `rtype::discord_sdk`, their
implementations, linked by the executable `r-type_client` alone.

| Interface | Feature | Implementation |
|---|---|---|
| `Authorizer` | the sign-in, above | `SocialSdkAuthorizer` |
| `Presence` | the activity of the player in its Discord profile, without a token | to come |
| `Connection` | the connection with the token of the player, which lobbies and voice need | to come |

The implementations share **one `discordpp::Client`**, owned by `discord::Session`: its log callback (the logger
`discord`, warnings and errors only), and `poll()`, the `RunCallbacks()` of the SDK, called once per frame by the
game loop. No thread of ours touches the SDK, and every answer of Discord comes from that poll. The client is made
at the first feature that needs it: a player who never uses Discord never starts the SDK. The features are
destroyed before the session. The tests use `tests/helpers/FakeSocialSdk.hpp`, a fake of each interface on one
`poll()` that delivers what the test made Discord answer, never inside the call that asked. The SDK itself is kept in `libs/discord/sdk/` (its `README.md`), for Linux
x86-64 and Windows x64; its library is copied beside the executable, in the build and in the package. On
another platform (Linux aarch64, the image of the server for linux/arm64), `rtype::discord_sdk` does not exist:
CMake warns and leaves `r-type_client` out, the server and the tests are built.

## RemoteSession

| Direction | What | How often |
|---|---|---|
| Client → instance | `INPUT`: the input of the player, stamped with a tick, with the inputs of the 7 ticks before | 60 per second, whatever the frame rate |
| Instance → client | `SNAPSHOT` parts and `ENTITIES_DESTROYED`, into a `RemoteWorld` | 30 and as they come |
| Instance → client | `PLAYER_JOINED`, `PLAYER_LEFT`: the names of the players | When they join and leave |
| Instance → client | `GAME_OVER`, `DISCONNECT`, or nothing for 5 seconds: the end | Once |

- **The tick of an input** is the one at which the game should apply it (section 9.2 of the
  [game protocol](../protocol/game.md#92-inputs)). The first ones are estimated: the tick of the latest
  snapshot, plus the time since it came, plus the round-trip time measured by the session, plus 2 ticks of
  margin. Once a snapshot reports how early they arrive, each input is stamped with the tick after the previous
  one, and the [input clock](#input-clock) keeps them just in time.
- **`takeSnapshots()`** gives, at every frame, the world of the [interpolation](#interpolation): the
  `GameSessionSystem` draws it as it draws the offline world, and the entities move at the frame rate.
- **Pausing** does nothing online: the game goes on, and the input sent under the pause menu is empty.
- **Leaving** sends `DISCONNECT` to the game, when the session is destroyed.

## Input clock

An estimate of the tick drifts, and the game ignores the inputs more than 60 ticks ahead without a word: a
client 1% fast ended with every input ignored after about 100 s, the ship coming back to where it was as soon
as the player moved ([ADR 0028](../decisions/0028-input-stamps-follow-the-lead.md)). So each `SNAPSHOT` tells
the client the **lead** of its inputs: the smallest, since the previous snapshot, of their tick minus the last
tick the game simulated when they came. 1 is just in time, 0 or less lost.

`InputClock` turns the leads into a pace:

| The smallest lead of the last 0.5 s | What the client does |
|---|---|
| 2 or 3 | One input per tick |
| 1: no margin left | A little faster: 3% per tick missing, 10% at most |
| Above 3: latency for nothing | A little slower: 3% per tick too many, half as fast at most |
| 0 or less: inputs lost (one report is enough) | The next stamp jumps forward at once, to a lead of 2; the ticks skipped carry the previous input, and the leads of the next RTT + 2 ticks are ignored |

```cpp
clock.advance(elapsed);                     // each frame
pending += elapsed * clock.rate();          // one INPUT each time a tick of pending is there
stamp += 1 + clock.takeJump();              // each INPUT sent
clock.receive(part.inputLead, rtt + 0.033); // each snapshot with a lead, once
```

The stamps stay consecutive: no input is lost, none is stamped twice, and stamps too far ahead come back down
by sending fewer of them. A frame too long (more than 5 ticks at once are not sent) or a scene that stops
updating leaves the stamps behind: the next lead is 0 or less, and they jump.

## Interpolation

The snapshots come 30 times per second, some late, some lost; the client draws 60, 144 times per second. Drawing
the newest snapshot moves the entities in jumps: still for 33 ms, then 20 units at once. `Interpolation` draws
the world **a little in the past** instead, between the two snapshots around that time
([game protocol, 9.3](../protocol/game.md#93-snapshots)):

```
snapshots received:   tick 40 ──────────── tick 42 ──────────── tick 44 (newest)
                               ▲
                       drawn tick 41 = newest − 3 ticks (50 ms): halfway from 40 to 42
```

**The delay** is as short as the network allows: each world that arrives tells the delay it needed not to be
waited for, and the largest of the last 3 s is kept, from 50 to 250 ms (100 ms before 8 worlds came):

| The network | The delay |
|---|---|
| Steady, a snapshot every 2 ticks | 50 ms: the 2 ticks to the next snapshot and a tick of margin |
| A snapshot lost now and then | 83 ms: 4 ticks without a world, and the margin |
| Snapshots 20 to 70 ms late, at random | About 100 ms: the 50 ms of jitter on top |
| Steady again | Back to 50 ms after 3 s, the drawn tick catching up a little faster, without a leap |

`needed = ticks since the previous world / 60 + (how late it came − the earliest of 3 s) + 1 / 60`, the
lateness being its arrival time minus its tick / 60: the earliest is the latency without jitter, whatever the
offset between the clocks.

| What happens | What is drawn |
|---|---|
| Snapshots on time | Each entity between its positions in the two snapshots around the drawn tick |
| A snapshot lost | The same, between the snapshots that arrived: 4 ticks apart instead of 2 |
| No snapshot for a while | Each entity goes on with its velocity for 250 ms at most, then waits |
| An entity created | It appears when the drawn tick reaches its first snapshot |
| An entity destroyed | It disappears when the drawn tick reaches its destruction, not when `ENTITIES_DESTROYED` arrives, the delay earlier |

**The drawn tick** advances with the time of the frames, 60 ticks per second, and is nudged towards its target
(newest tick − the delay): a little faster when it falls behind, a little slower when it gets ahead, never
backwards. The jitter of the snapshots, the drift between the clocks of the client and of the server and a
change of the delay are absorbed without a jump; after a pause of more than half a second (a window dragged, a debugger), it jumps to its
target.

`Interpolation` is pure, without a clock nor a network: the `RemoteSession` gives it each world received
(`receive()`), the destructions (`destroyed()`) and the time of each frame (`advance()`), and draws `view()`.

## Prediction

The interpolation draws the world 50 ms or more in the past, on top of the round trip: fine for the others, whose
keys the player does not see, but the ship of the player itself would answer late. `Prediction` moves it at
once instead, and the game stays the one that decides:

1. **Each input sent** moves the predicted ship by one tick, with `game::moveShip()`, the movement of the game
   itself: speed, diagonals, walls. The inputs are kept with their tick.
2. **Each snapshot** says where the game had the ship at its tick, every input up to that tick applied. The
   prediction starts again from there and replays the inputs sent for the later ticks (**reconciliation**).
   When nothing was lost, it lands exactly where it was: no correction at all.
3. **A wrong guess** (an input lost without its copies, something the game saw first) moves the prediction:
   the ship is not teleported, the gap is drawn and fades out in a few frames. A gap of more than 120 units
   (the ship came back elsewhere) is applied at once.
4. **Without a ship** (destroyed, waiting to come back), nothing is predicted: the game decides.
5. **Between two inputs**, 60 per second, the screen draws 144 frames: the ship is drawn on the way to the next
   tick, by the fraction of the time to the next input, moved with the input held now
   (`position(fraction, held)`). Not between the last two ticks, which would draw it a tick late: a key
   pressed shows at the next frame, and the next tick lands where the drawing was going, unless the keys change.

```
inputs sent:   12  13  14  15  16   ◄── applied at once: the ship is drawn there
snapshot of 13 from the game ──► start from its position, replay 14, 15, 16 ──► the same place, if nothing was lost
```

`game::moveShip()` (`libs/game/include/rtype/game/ShipMovement.hpp`) is shared with the systems of the game,
and a test checks that both give the same position to the bit, over a minute of random inputs against the walls.
A rule that changes how a ship moves must change there, or every snapshot would correct the prediction.

The movement of the ship and its shots are predicted ([below](#shots-of-the-player)): the hits and its
destruction are drawn as the game sends them, through the interpolation.

## Shots of the player

The ship of the player is drawn at the tick of its newest input, the world at the drawn tick: a round trip,
the delay of the interpolation and 2 ticks in the past. Its shots, drawn with the world, came out late and
trailed behind it: they appeared that long after Fire was pressed, far from its nose. They are predicted as the ship is,
from the moment it fires ([ADR 0029](../decisions/0029-predicted-shots.md)):

1. **`Prediction` runs the weapon too.** Each input sent steps it with `game::stepWeapon()`, and a tick it
   fires gives a shot, from where the ship was, with `game::playerShot()`: the steps of the game
   (`WeaponFiring.hpp`). Each snapshot gives the state of the weapon at its tick (cooldown, ammo, reload): the
   shots of the later ticks are replayed from there, like the ship. `prediction.shots()` holds those after the
   latest snapshot, by tick.
2. **`ShotPrediction` draws them at once**, under an id of its own (from `0xF0000000`), along the path of the
   game (`game::moveShot()`), in the time of the ship, the fraction of a tick included: between two ticks, on
   the way to the next one.
3. **The shot of the game takes its place.** A shot of the player in a snapshot of tick `T` with the age `a`
   was fired at `T − a`: it is matched with the predicted shot of that tick, and drawn under the id of the
   predicted one, along the same path. The drawing goes on without a jump, without a new entity, without a
   second sound; the destructions given to the drawing take that id too (`idOf()`).
4. **A wrong guess** is dropped: a shot the prediction no longer gives once a snapshot came (the weapon was not
   ready), or that no snapshot brought 6 ticks after its tick (lost, or destroyed before a snapshot had it).

```cpp
shots.arm(weapon.mechanic, rules);                 // once the slot of the player is known
shots.received(entity, part.tick);                 // each shot of the player in a SNAPSHOT
shots.destroyed(id, tick);                         // ENTITIES_DESTROYED of one of them
shots.predict(prediction.shots(), world.tick());   // each frame
shots.apply(snapshot, slot, drawnTick, inputTick); // then, on the world of the interpolation
```

| The shot | Where it is drawn |
|---|---|
| Predicted, or the game's in its place | On the path of the game, at the tick of the ship |
| The game's, not predicted (its age is 255, or no prediction at its tick) | Its position at the drawn tick, moved on by `velocity × (ship tick − drawn tick) / 60` |
| Destroyed, the ship past its destruction | Once where the game destroyed it, for its impact; then no more, though the interpolation still has it |
| Destroyed, the ship not there yet | As flying, until the ship reaches it |

- `RemoteSession` gets the definitions of the client (`loadGameConfig()`, through `OnlineScenes`): the weapon
  of the players and its projectile. Without them, the shots are only moved on, drawn when the game sends them.
- Drawn ahead, a shot would fly through what it hit until the drawn tick reaches its destruction: it is drawn
  where the game destroyed it as soon as `ENTITIES_DESTROYED` comes, a round trip after the ship reached that
  tick, then removed.
- The shots of the others, the Bydos and their shots stay in the past, with the world they hit.

## Tests

`tests/client_online.cpp` (tag `[online]`) runs the client against a real master in the same process: the
`Lobby` and the `Games` of `r-type_server`, with real instances.

- the address of a server, and a message for every error of the master;
- two clients welcomed, a game created, listed and joined, an error given to its request, a player who leaves;
- a server without the pinned key refused, a server unreachable, a name refused before connecting;
- a client trusting no key told the key of the server instead of closed, then welcomed once it trusts it;
- two clients playing the same game, each moving its own ship and seeing the other's, with the names of the
  players, the world drawn at every frame behind the newest one, and inputs stamped ahead of the world;
- a game that crashes reported to its player, who can create another one, and a game that stops ending the
  session of its player.

The accounts (tag `[online][accounts]`) run against a master with accounts, a mailer of the tests that keeps
the codes, on SQLite and PostgreSQL:

- an account created, verified with the code of its email, kept in the settings, and signed in again from its
  token alone after a restart of the client;
- an expired token told through `onSessionExpired`, not as a connection lost, forgotten, then a guest welcomed;
- a sign-out closing the connection without an error, its token refused after;
- a wrong password and its message, fields refused before sending, then the right password by email;
- a forgotten password changed with its code, which signs in; an account deleted with its password;
- Discord against a fake Discord (tag `[discord]`): the application of the master, a new Discord user signed in
  once it chose its username, a code verifier refused before sending, and the account, without password,
  deleted after a confirmation of Discord;
- the token kept for its server only, in a file readable by its owner only;
- every error code with a message in `en.lang` and `fr.lang`.

`tests/known_servers.cpp` (tag `[online]`) checks the key trusted for each server: a server trusted on first
use, kept in the settings, then refused with another key until forgotten; settings holding something else than a
fingerprint, which trusts nothing; a fingerprint shown in groups of 4 digits.

`tests/client_interpolation.cpp` (tag `[interpolation]`) plays a server sending 30 snapshots per second to a
client drawing 144 frames per second: the world drawn 50 ms behind on a steady network, an entity moving a
little at every frame, with snapshots on time, irregular or lost; the delay longer with jitter or losses, never
waiting for a snapshot, back down without a leap once steady, within its bounds; extrapolation then waiting
without snapshots; entities appearing and disappearing when the drawn tick reaches them; a server clock
running faster; a pause; and the drawn tick never going back.

`tests/client_input_clock.cpp` (tag `[input-clock]`) checks the pace alone, then plays it against the
`InputBuffer` of the server, at 240 frames per second, with delays on the way up and down: no input lost with a
client clock 1% fast or 1% slow for 5 minutes, the lead staying between 1 and 5; stamps 50 ticks ahead
coming back down without losing an input; a game that stalls 300 ms and skips the ticks it missed; a client that
starts late; and a network that jitters by 40 ms, a few inputs late at the start, none after.

`tests/client_shot_prediction.cpp` (tag `[shot-prediction]`) checks the shots of the player moved on to the
tick of the ship, the rest of the world left alone, never moved back, a shot destroyed drawn once where it
ended then gone; a predicted shot drawn from its tick, dropped when no longer predicted or never sent, replaced
by the shot of the game under its id, done once it is gone. Then the client plays against the real game, for
every weapon of `config/weapons/`, fire held, moving and aiming: every shot drawn was predicted, at the tick its
input was sent, at the position the game has at that tick, to the bit; with inputs lost, the wrong guesses
are dropped and nothing is drawn twice; and a shot drawn between two ticks.

`tests/client_prediction.cpp` (tag `[prediction]`) plays the client against the real rules, a `Simulation` with
the `InputBuffer` of the server, the inputs and the snapshots delayed: never corrected when nothing is lost, nor
when an `INPUT` is lost (the next ones carry its inputs); the ship answering before the game; the game ending
where the prediction was; the walls; without redundancy, a lost input corrected smoothly; no ship, then back
elsewhere; a correction too large applied at once; and the ticks skipped when stamping; the ship drawn between two ticks with the input held, and moving at every
frame at 144 frames per second.

```sh
./build/bin/rtype_tests "[online]"
./build/bin/rtype_tests "[online][accounts]"
./build/bin/rtype_tests "[interpolation]"
./build/bin/rtype_tests "[prediction]"
./build/bin/rtype_tests "[input-clock]"
./build/bin/rtype_tests "[shot-prediction]"
```

## Limits and next steps

- **A shot that hits at once**, within a round trip of the ship, flies through its target for that long before
  its destruction comes: the Bydos are drawn in the past.
- **The player sees the others in the past**, 50 ms or more on top of the round trip, and its own ship in the
  present: a Bydo shot can seem to miss and still hit, as in every game that predicts.
- **The names of the players** are known (`RemoteSession::names()`) but not shown in the HUD yet.
- **One server at a time**, typed by hand: no list of servers, no address remembered.
