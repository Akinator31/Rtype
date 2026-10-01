# Contributing

## Git workflow

- `main` always builds and is protected: changes go through **pull requests**.
- One branch per feature or fix, named after its purpose: `feature/star-field`, `fix/udp-buffer-overflow`,
  `docs/protocol-header`.
- Every pull request is reviewed by at least one other team member and must pass the CI
  (*Build*, *Format* and, when relevant, *Documentation*).
- Work is tracked in GitHub issues; reference the issue in the pull request (`Closes #12`).
- Important milestones (deliveries, defenses) are tagged: `v1.0.0`, `v2.0.0`.

## Commit messages

Short summary line in English, in the imperative mood, followed by a blank line and details if needed:

```
Add UDP socket abstraction

Wraps BSD sockets and Winsock behind a single interface so that the
networking layer does not depend on the platform.
```

## Coding conventions

- C++20, compiled without warnings (`-Wall -Wextra -Wpedantic` / `/W4`).
- Formatted with clang-format using the repository's `.clang-format`. On pull requests, the format bot
  fixes unformatted code automatically. See [Formatting](testing.md#formatting).
- New behaviour comes with tests, following the [testing policy](testing.md).
- Public headers of the engine are documented with Doxygen comments, see
  [Documenting C++ code](documentation.md#documenting-c-code).

## Repository administration

The one-time configuration of the repository (format bot, GitHub Pages, protection of `main`) is described
in [Repository setup](repository-setup.md).

## Documentation

Any change visible to users or other developers comes with its documentation in the same pull request. See
[Writing documentation](documentation.md).
