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
| [0005](0005-world-in-ecs-interface-in-ui-layer.md) | Indexed vectors for the ECS component caches             | Accepted |
| [0006](0006-ecs-component-storage.md)              | World in the ECS, interface in a separate layer          | Accepted |

To record a new decision, copy the [ADR template](../contributing/templates.md#architecture-decision-record)
into a new file `NNNN-short-title.md` in this folder, add it to the navigation and to this table.

!!! note "Changing a decision"

    Accepted records are not rewritten. When a decision changes, write a new record and set the status of
    the old one to *Superseded by NNNN*.
