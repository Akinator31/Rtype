# Administer a server

Open the panel of a running server, watch its games and its players live, and act on them: end a game, send a
message, remove a player, ban an address; with accounts, ban accounts and name the administrators. The panel is
opened from the machine of the server, with the address the server writes at each start; with accounts, an
administrator also signs in from anywhere ([From anywhere, with an account](#from-anywhere-with-an-account)).

## Prerequisites

- A server running ([Run a server](run-a-server.md)), from the server archive of a release or of the CI: the
  panel is in it, `admin/` beside `r-type_server`. A server built from the sources has it once the panel is
  built and installed ([web/admin/README.md](https://github.com/Akinator31/Rtype/blob/main/web/admin/README.md#build));
  without it, the page says the panel is not built.
- A browser on the machine of the server, or an SSH access to it.

## Steps

1. **Find the address of the panel**: at each start, the server writes it with a new token to `admin-url.txt`,
   beside its key, readable by its owner only. Its log says where:

    ```
    INFO  master   Panel at http://127.0.0.1:8080: open the address written in /data/admin-url.txt
    ```

    ```sh
    cat /data/admin-url.txt
    # http://127.0.0.1:8080/?token=9b47…
    ```

2. **Open it** in a browser. The token is traded for a session of 12 hours, then leaves the address bar: the
   panel shows its overview, and **Live** at the top right. A page of the panel opened without a session asks
   for the token instead: paste the address of `admin-url.txt`, or **Read admin-url.txt** to choose the file.

    From another machine, the panel is not reachable: it listens on the loopback of the server only. Go through
    an SSH tunnel, then open the same address:

    ```sh
    ssh -L 8080:127.0.0.1:8080 <server>
    ```

    In a container, the panel listens on the loopback of the container, which this tunnel does not reach:
    see [Open the panel](deploy-with-docker.md#open-the-panel) of Deploy with Docker.

3. **Watch**. The pages update by themselves, without a reload:

    | Page | What it shows |
    |---|---|
    | Overview | The players connected, the games running and those stuck, the banned addresses, the uptime, each opening its page; the games; the live events; the players of each minute of the last hour since the panel opened; the latest actions |
    | Games | The running games: their seats, wave, time of play, state; searched and filtered. **Stuck** means the instance sent no status for 3 seconds: it, or its game, is stuck, and a red strip offers to end it. Click a line to open the game |
    | A game | Its pilots with their address, score, state and latency; its wave and time of play; its own events |
    | Players | Every player of the lobby, its address, its game; click a line for the card of its player, and the other pilots of its address |
    | Bans | The addresses banned now and the time left of each; those lifted or expired lately |
    | Messages | The messages sent |
    | History | What the administrators did, newest first, 100 at a time; click a line for its details |
    | Accounts | With accounts: every account, searched by username, email or Discord id; its role and whether it is banned |
    | Settings | The version and the ports of the server, and the preferences of the panel |

4. **Act**. Removing a player, ending a game, banning and lifting a ban ask for a confirmation and an optional
   reason, recorded with it; a toast then says what happened at the top right, and the action shows in
   **History**:

    | To | Where | What happens |
    |---|---|---|
    | End a game | A game, **End the game**; Games, **End** | The game stops; its players are back in the lobby, told why |
    | Send a message | **Messages**, to the whole lobby or to a game; a game, **Message to the game** | Each player sees it in the game. One line, 200 bytes at most |
    | Remove a player | Players, or a game, **Remove** | The player leaves its game and the server. It may connect again |
    | Ban an address | Players, **Ban**; a game, **Ban the address**; Bans, **New ban** | From 1 second to 7 days, across restarts: the server refuses the address, and removes the players connected from it, whom the panel lists first |
    | Lift a ban | Bans, **Lift** | The server welcomes the address again at once |
    | Ban an account | Accounts, **Ban** | The account can no longer sign in, its sessions end, its pilots leave the server at once. Until it is unbanned |
    | Unban an account | Accounts, **Unban** | It signs in again |
    | Name an administrator | Accounts, **Make admin**, **Make player** | It signs in to the panel, or loses it at once |
    | Delete an account | Accounts, **Delete** | The account is deleted for good; its actions stay, anonymized |

    A ban is temporary: an address is shared, by a household or a network, and changes. In **Settings**, turn
    **Confirm the actions** off to remove and end without the window, and **Sound alert** on to hear an
    instance become stuck; both are kept in this browser only.

## From anywhere, with an account

With accounts ([Set up accounts](set-up-accounts.md)), the panel may leave the loopback, behind the proxy of the
host that serves it over HTTPS, and its administrators sign in with their own account.

1. **Name the first administrators** in the settings, by the email of their account, verified, or
   `discord:<id>`; never by username, which anyone can take:

    ```toml
    [admin]
    admins = ["chief@example.org", "discord:80351110224678912"]
    ```

    They are administrators whatever their role, at every start: the panel can neither demote, ban nor delete
    them, and shows them **Settings** in Accounts. A name of the list that has no account yet, or not a verified
    email, is a warning at start: it becomes an administrator once it has, from the game. The others are named
    from the panel, **Make admin**.

2. **Put the panel behind the proxy** of the host, Traefik, nginx or Caddy, which serves it over HTTPS:

    ```toml
    [web]
    listen = "0.0.0.0:8080"
    public_url = "https://admin.example.org"
    trusted_proxies = ["172.16.0.0/12"]
    ```

    `public_url` makes every cookie `Secure`, and is where Discord sends the administrators back.
    `trusted_proxies` are the addresses of the proxy, whose `X-Forwarded-For` gives the address of the visitor,
    for the logs and the limits on signing in. In Dokploy, the domain of the service does it, with Traefik:
    see [With accounts](deploy-with-docker.md#with-accounts) of Deploy with Docker.

3. **Sign in** at the address of the panel: a username or an email and its password, or **Sign in with
   Discord** when the server has `[discord]` and `public_url`. With Discord, add
   `https://admin.example.org/auth/discord/callback` to the redirects of the Discord application
   ([Set up accounts](set-up-accounts.md#sign-in-with-discord)); the Discord account must have signed in to the
   game once, which made its account.

    The session lasts 12 hours after the last request. **Sign out** at the foot of the side bar ends it. 10
    sign-ins a minute from one address, and 10 wrong passwords in a row for one account, pause it for a while.

## Result

The panel shows the server as it is, and every action is in the record, with its reason and its actor: `local`
for the token of the machine, or the id of the account of the administrator.

| Message | What to do |
|---|---|
| The panel is not built | The server has no `admin/` beside it: use the server archive, or install the panel ([web/admin/README.md](https://github.com/Akinator31/Rtype/blob/main/web/admin/README.md#build)) |
| This link is not valid | The address is of a previous start: the server writes a new one at each start, read `admin-url.txt` again |
| Session expired, in a window over the page | The session ended, or the server restarted: paste the new address of `admin-url.txt`, or open it; with accounts, sign in again |
| This account is not an administrator | Make it one from the panel, opened with `admin-url.txt`, or name it in `[admin] admins` |
| The email of this account is not verified | Verify it from the game, with the code it received |
| Sign-in expired, after Discord | The sign-in took more than 10 minutes, or came back in another browser: start again from the panel |
| Discord did not sign in an account of this server | Sign in to the game with Discord first: that makes the account |
| `409`, an administrator of the settings | It is in `[admin] admins`: take it out of the settings, then restart |
| Connection lost, at the top of the page | The server does not answer: the panel tries again by itself, **Retry** at once, and shows its last state meanwhile. If it lasts, check that the server runs |

Without a browser, the same session drives the [administration API](../protocol/admin-api.md) with `curl`
([With curl](../protocol/admin-api.md#7-with-curl)). How the panel works: [The panel](../architecture/panel.md).
