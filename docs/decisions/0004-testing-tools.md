# 0004 — Testing tools

| | |
|---|---|
| **Status** | Accepted |
| **Date** | 2026-10-01 |

## Context

The project needs automated tests on Linux and Windows (MSVC), run by the CI. Some subject requirements can
only be demonstrated with dedicated tools: robustness against malformed packets, a multithreaded server
that survives client crashes, and cross-platform binary compatibility. See the
[testing policy](../contributing/testing.md).

## Options

### Test framework

| | Catch2 v3 | GoogleTest | doctest |
|---|---|---|---|
| Test style | `TEST_CASE` + `SECTION`, BDD macros | Fixtures (`TEST_F`), xUnit style | Close to Catch2 |
| Mocking | None built in | GMock | None built in |
| Benchmarks | Built in | No (separate Google Benchmark) | No |
| CMake / CTest integration | `catch_discover_tests` | `gtest_discover_tests` | Script provided |
| MSVC support | Yes | Yes | Yes |
| Maintenance | Active | Active | Slower |

### Other tools

| Need | Chosen | Alternatives considered |
|---|---|---|
| Robustness of the packet decoder | libFuzzer | AFL++ (more setup), hand-written random tests (much weaker) |
| Memory and threading bugs | ASan, UBSan, TSan | Valgrind (slower, no data-race detection as precise as TSan) |
| Formatting | clang-format, pinned through pip | Editor-specific settings (not enforceable) |
| Applying the formatting | Bot committing fixes on pull requests (GitHub App) | Local pre-commit hook (requires an installation on every machine), check-only CI job (manual fix), pre-commit.ci (free for public repositories only) |

## Decision

- **Catch2 v3** for unit and integration tests, run through **CTest** on both platforms. Readable tests with
  sections, and built-in benchmarks for the measurements expected in part 2. Mocking is done with hand-written
  fakes behind interfaces, which also enforces the decoupling of the engine.
- **libFuzzer** and the **sanitizers** on Linux (Clang) in CI, added when the code they target exists.
- **clang-format 23**, pinned in `requirements-dev.txt` so that every developer and the CI format the same
  way. It is **applied automatically** on pull requests by a format bot, so no developer has to install
  anything; the bot is a GitHub App so that its commits trigger the other workflows.

## Consequences

- Tests are built by default (`RTYPE_BUILD_TESTS=ON`); Catch2 is fetched by CPM like other dependencies.
- No mocking library: interfaces must be designed so that a fake implementation is easy to write.
- Fuzzing and sanitizers run on Linux only; Windows is covered by the regular test suite.
- When the bot pushes a formatting commit, the author must `git pull --rebase` before pushing again.
  Formatting on save in the editor avoids it.
- The repository administrator has to create the GitHub App once, see
  [Repository setup](../contributing/repository-setup.md).
