# Technical decisions

Each significant technical choice is recorded as an **Architecture Decision Record** (ADR): a short page
explaining the context, the options that were compared, and the decision. Together, these records form the
technical and comparative study of the project.

| #                                                  | Decision                                                 | Status   |
| -------------------------------------------------- | -------------------------------------------------------- | -------- |
| [0001](0001-cpm-package-manager.md)                | CPM.cmake as package manager                             | Accepted |
| [0002](0002-raylib-graphics-library.md)            | raylib for the client                                    | Accepted |
| [0003](0003-documentation-tooling.md)              | Zensical and Doxygen for the documentation               | Accepted |
| [0004](0004-testing-tools.md)                      | Catch2, sanitizers, fuzzing and clang-format for testing | Accepted |
| [0005](0005-world-in-ecs-interface-in-ui-layer.md) | World in the ECS, interface in a separate layer          | Superseded by [0007](0007-client-in-the-ecs.md) |
| [0006](0006-ecs-component-storage.md)              | Indexed vectors for the ECS component caches             | Accepted |
| [0007](0007-client-in-the-ecs.md)                  | The whole client in the ECS, with scenes and a hierarchy | Accepted |
| [0008](0008-events-switch-systems.md)              | Events switch systems on and off                         | Accepted |
| [0009](0009-named-scenes-in-the-engine.md)         | Named scenes in the engine                               | Accepted |
| [0010](0010-physics-engine.md)                     | A custom kinematic physics engine                        | Accepted |
| [0011](0011-networking-library.md)                 | Asio as the networking library                           | Accepted |
| [0012](0012-tcp-lobby-udp-game.md)                 | TCP for the lobby, UDP for the game                      | Accepted |
| [0013](0013-one-process-per-game.md)               | One process per game                                     | Accepted |
| [0014](0014-encryption.md)                         | Encryption of the client/server traffic                  | Accepted |
| [0015](0015-one-game-scene-local-or-remote-session.md) | One game scene, fed by a local or a remote session | Proposed |

To record a new decision, copy the [ADR template](../contributing/templates.md#architecture-decision-record)
into a new file `NNNN-short-title.md` in this folder, add it to the navigation and to this table.

!!! note "Changing a decision"

    Accepted records are not rewritten. When a decision changes, write a new record and set the status of
    the old one to *Superseded by NNNN*.
