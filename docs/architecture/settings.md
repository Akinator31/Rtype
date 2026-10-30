# Settings

`rtype::engine::Settings` lives in `libs/engine` (`rtype/engine/Settings/Settings.hpp`) and uses neither the ECS nor raylib. It keeps values from one run of the client to the next, each under a key: the settings of the player, and the servers it trusts ([Playing online](client-online.md#trusted-servers)). The `Engine` owns one, reached with `engine.getSettings()`.

The values live in memory. `readFile()` replaces them with the content of the file given to the constructor or to `setPath()`; a missing file leaves no value, as on a first run. `writeFile()` writes them, creating the directories of the file. It writes to `<file>.tmp` first and then renames it, so a crash while writing keeps the previous settings. On Unix, the file is readable and writable by its owner only (mode `0600`), set before anything is written: it holds the session tokens of the accounts. On Windows, the data directory of the player is already its own.

```cpp
rtype::engine::Settings &settings = engine.getSettings();
settings.setInt("audio.volume", 80);
const int volume = settings.getInt("audio.volume").value_or(100);
settings.writeFile();
```

Values are set and read as `String`, `Int`, `Float` or `Bool`. They are stored as text, so a getter returns `std::nullopt` when the key is missing or when its value is not of the requested type: `value_or()` gives the default. `hasKey()` and `removeKey()` complete the API.

## The file

The client reads `settings.txt` from the data directory of the player when it starts (`rtype::render::userDataDirectory()`: `%APPDATA%\R-Type` on Windows, `$XDG_DATA_HOME/r-type` or `~/.local/share/r-type` elsewhere). A file that cannot be read is logged, and the client starts without it.

The file is text, one `key=value` line per value, sorted by key. A backslash, `=` and line breaks in a key or a value are escaped with a backslash. Lines without `=` are ignored, and Windows line ends are accepted.

Name keys by domain with dots, so that the file stays readable:

| Keys | Written by |
|---|---|
| `servers.known.<host>:<port>` | `KnownServers`: the fingerprint of the key the player trusted for that server |
| `servers.session.<host>:<port>` | `SessionTokens`: the session token of the account signed in on that server, 64 hexadecimal digits, sent in the next `HELLO` ([Accounts](client-online.md#accounts)). A secret: never logged |

## Not the data of the server

The settings are the client's, on the machine of the player. The data a server keeps is a separate system, with
its own library, and shares no code with them.
