# Play online

Run a server and play the same game from several machines, or from several clients on one machine.

## Steps

1. **Start the server** ([Run a server](run-a-server.md)). At its first start, it makes its key, in
   `server.key` ([Manage the server key](generate-the-server-keys.md)):

    ```sh
    ./build/bin/r-type_server
    ```

    For players on other machines, open 4242 over TCP and over UDP.

2. **Connect.** Start the client, choose **Online**, type a name and the server, then **Connect**, or pick one
   of the **Servers met** on the right, which shows the account you will be signed in to (**Forget**, beside a
   server, forgets its key):

    | Field | Example | Meaning |
    |---|---|---|
    | Name | `Pavel` | 1 to 16 characters, shown to the other players |
    | Server | `127.0.0.1:4242` | The address of the server and its port; the port is 4242 if omitted |

3. **Trust the server**, the first time only. A server the client does not know yet shows its fingerprint:
   compare it with the one its owner gives (`r-type_server --fingerprint`), group by group, then **Trust**. The
   client remembers it in its settings, and connects at once the next times.

4. **Do not trust a key that changed.** A known server that presents another key is refused, with a warning
   that shows both: someone may be pretending to be it. If its owner did change its key, **Forget the old
   key** on the warning: the client connects again and shows the new key, to check and trust as the first time.

5. **Sign in**, on a server with accounts ([Set up accounts](set-up-accounts.md)). After **Connect**, the
   client offers what the server offers: **Continue with Discord** first, then **Sign in** (a username or an
   email, and the password), **Create an
   account** (a username, an email, a password of 10 to 128 characters), and **Play as a guest** if the server
   accepts guests. A new account, or one whose email is not verified, asks for the code of 6 digits sent by
   email, valid 15 minutes: **Send a new code** if it did not come. **Forgot password** sends a code to choose a
   new one. A field the server refuses says why under it, in red. **Continue with Discord**, on a server that
   offers it, asks in the Discord client, or in the browser: accept, then come back to the game; the first time,
   your Discord picture and name show, and you choose the username of the account, your Discord name proposed.
   Once signed in, the client comes back signed in at the next connections, even after a restart. Your avatar,
   at the top right of the lobby, opens **My account**: your Discord profile, your email, the date of the
   account; sign out, link Discord (or refresh your picture from it), or delete the account, with its password
   or, for an account made with Discord, with **Confirm with Discord**. A server without accounts goes straight
   to the lobby.

6. **Play.** In the lobby, **Create a game**, or join one of the list (**Refresh** to see the new ones; a full
   game is dim). A
   second client, on this machine or another, joins the same game: each player has a ship of its colour.
   Escape opens the pause menu, where **Leave the game** goes back to the list.

7. **Talk.** In a game, the players hear each other through the game. Hold ++v++ (LT on a gamepad) to talk:
   the others hear you while it is held. ++m++ (Y) mutes and unmutes you, ++n++ (View) cuts the voices of the
   others, and your microphone with them. **Settings → Voice chat** chooses the mode (push-to-talk, voice
   activity, off), the threshold of the voice activity over the level of your microphone, the microphone, the
   volumes and these keys. A headset keeps the sound of the game out of the microphone. Without a microphone
   you only listen; without an audio output, you have no voice ([Voice](../architecture/voice.md)).
   In the HUD, the name of a player who talks lights up, and a microphone shows above its ship; an icon beside
   each name says when it is muted, muted by you, or without a microphone. In the pause menu, **Player voices**
   sets the volume of each player for you (Left, Right), and Enter mutes or unmutes it: the choice is kept for
   its account, and found again in the next game.

8. **Play with your Discord friends.** Signed in with Discord on a server that gives the token (its
   `[discord] social = true`), the lobby lists your Discord friends right of the games: those playing R-Type
   first, with what they do. **Ask to join** a friend whose game has room: it accepts in its game, or in
   Discord, then you get an invitation. An invitation shows a toast, then **Join** in the lobby: the game
   connects to its server, asks you to trust it the first time, then opens the hangar before the game. In a
   game, the pause menu lists your friends online: **Invite** one, or **Accept** a friend who asks to join.
   In the lobby, **Invite** a friend online: it comes to your server. From Discord too, without any of this:
   **Join** on the profile of a friend in the lobby of a server or in a game online with room, or **+** →
   *Invite to play* in a conversation while you are in the lobby or in a game online; Discord starts the game if it
   is closed.

## Result

The players see each other's ships move. The server shows them in its logs:

```
INFO  lobby    Pavel (1) joined the lobby from 127.0.0.1:51234
INFO  games    Pavel creates game 1 "Pavel's game" for 4 players: instance 48213
INFO  games    game 1 "Pavel's game" ready
INFO  instance Pavel (1) connected from 127.0.0.1:40102, slot 0
```

| Toast in the client | Fix |
|---|---|
| `Cannot reach ...: Connection refused` | The server does not run, or the address or the TCP port is wrong, or blocked |
| `The game cannot be reached: its UDP port may be blocked` | The lobby works but not the games: open the UDP port of the server, 4242 by default |
| `The server cannot host another game` | The server runs its most games at once: wait for a game to end, or raise its `--max-games` |
| `Your session expired: sign in again` | The session ended (30 days, a new password, a ban, signed out elsewhere): the client connected again as a guest |
| `If this email has an account, a code was sent to it` but no email | Look in the spam folder, then **Send a new code** (once a minute); the server may not reach its mail server |
| `This name belongs to an account` | A guest cannot take the username of an account: choose another name, or sign in |
| `Discord sign-in cancelled` | Refused or closed in Discord: **Continue with Discord** again |
| `This Discord account belongs to another account` | That Discord account signs in to another account of the server: sign in with it, or link another |
| `The server cannot reach Discord now` | Try again in a moment; the server may not reach `discord.com` |
