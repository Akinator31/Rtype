# Technical decisions

Each significant technical choice is recorded as an **Architecture Decision Record** (ADR): a short page
explaining the context, the options that were compared, and the decision. Together, these records form the
technical and comparative study of the project.

| #                                                       | Decision                                                    | Status                                                                                 |
| ------------------------------------------------------- | ----------------------------------------------------------- | -------------------------------------------------------------------------------------- |
| [0001](0001-cpm-package-manager.md)                     | CPM.cmake as package manager                                | Accepted                                                                               |
| [0002](0002-raylib-graphics-library.md)                 | raylib for the client                                       | Accepted                                                                               |
| [0003](0003-documentation-tooling.md)                   | Zensical and Doxygen for the documentation                  | Accepted                                                                               |
| [0004](0004-testing-tools.md)                           | Catch2, sanitizers, fuzzing and clang-format for testing    | Accepted                                                                               |
| [0005](0005-world-in-ecs-interface-in-ui-layer.md)      | World in the ECS, interface in a separate layer             | Superseded by [0007](0007-client-in-the-ecs.md)                                        |
| [0006](0006-ecs-component-storage.md)                   | Indexed vectors for the ECS component caches                | Accepted                                                                               |
| [0007](0007-client-in-the-ecs.md)                       | The whole client in the ECS, with scenes and a hierarchy    | Accepted                                                                               |
| [0008](0008-events-switch-systems.md)                   | Events switch systems on and off                            | Accepted                                                                               |
| [0009](0009-named-scenes-in-the-engine.md)              | Named scenes in the engine                                  | Accepted                                                                               |
| [0010](0010-physics-engine.md)                          | A custom kinematic physics engine                           | Accepted                                                                               |
| [0011](0011-networking-library.md)                      | Asio as the networking library                              | Accepted                                                                               |
| [0012](0012-tcp-lobby-udp-game.md)                      | TCP for the lobby, UDP for the game                         | Accepted                                                                               |
| [0013](0013-one-process-per-game.md)                    | One process per game                                        | Accepted, its ports amended by [0027](0027-one-udp-port-relay.md)                      |
| [0014](0014-encryption.md)                              | Encryption of the client/server traffic                     | Accepted, partly superseded by [0021](0021-server-generated-key-trust-on-first-use.md) |
| [0015](0015-one-game-scene-local-or-remote-session.md)  | One game scene, fed by a local or a remote session          | Accepted                                                                               |
| [0016](0016-replication.md)                             | Full snapshots, explicit destructions and redundant inputs  | Proposed                                                                               |
| [0017](0017-asset-loading-threads.md)                   | Asset preparation on a background thread                    | Superseded by [0022](0022-engine-thread-manager.md)                                    |
| [0018](0018-lua-game-config.md)                         | Lua for the configuration of enemies and weapons            | Accepted, partly superseded by [0019](0019-luabridge3-scripting-library.md)            |
| [0019](0019-luabridge3-scripting-library.md)            | LuaBridge3 and a scripting library for Lua behaviors        | Accepted                                                                               |
| [0020](0020-server-key-distribution.md)                 | Official server built in, trust on first use for the others | Superseded by [0021](0021-server-generated-key-trust-on-first-use.md)                  |
| [0021](0021-server-generated-key-trust-on-first-use.md) | A key generated by the server, trusted on first use         | Accepted                                                                               |
| [0022](0022-engine-thread-manager.md)                   | Engine owned asset worker                                   | Accepted                                                                               |
| [0023](0023-server-database.md)                         | SOCI over SQLite and PostgreSQL for the server data         | Accepted                                                                               |
| [0024](0024-player-accounts.md)                         | Player accounts, per server, all or nothing                 | Accepted; its Discord sign-in superseded by [0026](0026-discord-social-sdk.md)         |
| [0025](0025-server-administration.md)                   | Administration through a web panel served by the master     | Accepted                                                                               |
| [0026](0026-discord-social-sdk.md)                      | Discord sign-in through the Discord Social SDK in the client | Accepted                                                                               |
| [0027](0027-one-udp-port-relay.md)                      | One UDP port for every game, through a relay in the master  | Accepted                                                                               |
| [0028](0028-input-stamps-follow-the-lead.md)            | The stamps of the inputs follow the lead the game reports   | Accepted                                                                               |
| [0029](0029-predicted-shots.md)                         | The shots of the player are predicted, then matched by their age | Accepted                                                                          |
| [0030](0030-measured-delays.md)                         | A round-trip time without the delay of the acknowledgements, an interpolation delay measured | Accepted                                                              |
| [0031](0031-destructions-sent-again.md)                 | The destructions, unreliable, sent again with the snapshots | Accepted                                                                               |
| [0032](0032-voice-chat-in-the-game-session.md)          | A voice chat of our own in the game session, and Discord beside it in the same game | Accepted                                                       |

To record a new decision, copy the [ADR template](../contributing/templates.md#architecture-decision-record)
into a new file `NNNN-short-title.md` in this folder, add it to the navigation and to this table.

!!! note "Changing a decision"

    Accepted records are not rewritten. When a decision changes, write a new record and set the status of
    the old one to *Superseded by NNNN*.
