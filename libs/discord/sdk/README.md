# Discord Social SDK

The files of the Discord Social SDK the client needs to sign in with Discord
([ADR 0026](../../../docs/decisions/0026-discord-social-sdk.md)), kept as Discord ships them: it is downloaded
only from the [developer portal of Discord](https://discord.com/developers/applications), with an account, so
CPM cannot fetch it.

| | |
|---|---|
| **Version** | 1.10.19337, archive `DiscordSocialSdk-1.10.19337.zip` |
| **Kept** | `include/` (the headers), `lib/release/libdiscord_partner_sdk.so` (Linux x86-64), `lib/release/discord_partner_sdk.lib` and `bin/release/discord_partner_sdk.dll` (Windows x64), `License-Notices.txt` |
| **Left out** | The debug builds, macOS, iOS, Android, Windows ARM64, and the noise suppression of the voice (Krisp), which the game does not use |

`libs/discord/CMakeLists.txt` imports the library; the `.clang-format` of this folder keeps the formatter of the
repository away from the headers. The Linux library needs `libpulse.so.0` and `libasound.so.2` at run time.

## Update it

1. Download the new archive from the developer portal (*Social SDK*, *Downloads*).
2. Replace the files above with those of the archive, at the same paths.
3. Write the new version in the table above, build the client on Linux and Windows, and sign in with Discord on
   a test server once.
