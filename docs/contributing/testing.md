# Testing policy

This page defines how R-Type is tested: which tools are used, what must be tested, how the code has to be
designed to be testable, and which steps are still to be set up. The choice of tools is explained in
[ADR 0004](../decisions/0004-testing-tools.md).

## Goals

The subject sets requirements that are hard to prove without automated tests:

| Requirement | How we prove it |
|---|---|
| Malformed packets must not crash the client or the server, nor consume excessive memory | Unit tests for every rejection case, and **fuzzing** of the packet decoder |
| The server is multithreaded and keeps running when a client crashes | **ThreadSanitizer** and an integration test that kills a client |
| Clients and servers on Linux and Windows play together | Unit tests of the binary encoding, run on **both platforms** in CI |
| Time flow does not depend on CPU speed | Game logic tested with an injected, fixed time step |

## Tools

| Purpose | Tool | Status |
|---|---|---|
| Unit and integration tests | [Catch2 v3](https://github.com/catchorg/Catch2), fetched by CPM | :white_check_mark: In place |
| Test runner | CTest | :white_check_mark: In place |
| Code formatting | clang-format 23 (`.clang-format`, `requirements-dev.txt`), applied by the format bot | :white_check_mark: In place |
| Memory errors and undefined behaviour | AddressSanitizer + UndefinedBehaviorSanitizer, `RTYPE_SANITIZERS` | :white_check_mark: In place |
| Packet decoder robustness | libFuzzer (Clang) | :hourglass: Step 2 |
| Data races in the server | ThreadSanitizer, `RTYPE_SANITIZERS` | :white_check_mark: In place |
| Static analysis | clang-tidy | :hourglass: Step 3 |
| Coverage report | gcovr, published with this documentation | :hourglass: Step 4 |
| Performance measurements | Catch2 benchmarks | :hourglass: Step 4 |

## Test levels

Unit tests
:   Most of the tests. One behaviour per test, no network, no window, no sleeping. Targets: ECS, serialization,
    protocol messages (valid and invalid), game rules (movement, collisions, spawning).

Integration tests
:   The server and two to four headless "bot" clients in the same test, over UDP on `127.0.0.1`: connection,
    propagation of a move, abrupt disconnection of a client.

Fuzzing
:   The packet decoder receives millions of random or mutated datagrams. Any crash, sanitizer report or
    excessive allocation is a bug.

Smoke tests
:   The binaries start and exit cleanly (`smoke.*` tests, registered with `add_test` in `tests/CMakeLists.txt`).

## Designing testable code

These rules are what make the tests above possible. They also match the decoupling expected from the
engine.

- **The engine is a library** linked by the client, the server and the tests. Tests never link the client
  or server executables.
- **Game logic does not depend on raylib**: it runs without a window, in the server and in tests.
- **Time and randomness are injected**: systems receive the elapsed time instead of reading a clock, and
  random generators take a seed. A test always replays the same way.
- **Networking is behind an interface**: tests use a fake socket, which can also drop, duplicate or reorder
  packets on purpose.

## Rules for contributions

- A pull request that adds or changes behaviour comes with tests for it.
- A bug fix comes with a test that fails without the fix.
- Every pull request must pass the *Build* (both platforms, tests included), *Sanitizers* and *Format* workflows.
  Formatting is fixed by the format bot, see [Formatting](#formatting).
- Tests are deterministic: no dependency on time, network outside `127.0.0.1`, or execution order.

## Writing a test

Tests live in `tests/`. Add the file to `rtype_tests` in `tests/CMakeLists.txt`; each `TEST_CASE` then appears
as a separate CTest test.

```cpp
#include <catch2/catch_test_macros.hpp>

TEST_CASE("A packet shorter than the header is rejected", "[protocol]")
{
    const std::array<std::byte, 2> data{};
    REQUIRE_FALSE(decodePacket(data).has_value());
}
```

- Name the test after the expected behaviour, as a sentence.
- Tag it with its subsystem, for instance `[ecs]`, `[scenes]`, `[engine]`, `[render]`, `[input]`, `[audio]`,
  `[log]`, `[network]`, `[platform]`, and later `[protocol]`, `[game]`. Run one subsystem with
  `./build/bin/rtype_tests "[scenes]"`.
- Use `SECTION`s to share a setup between related checks.

## Running the tests

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
ctest --test-dir build -C Release --output-on-failure
```

Run a subset with Catch2 tags, or with a CTest name filter:

```sh
./build/bin/rtype_tests "[protocol]"
ctest --test-dir build -C Release -R smoke
```

Tests are built by default; configure with `-DRTYPE_BUILD_TESTS=OFF` to skip them.

The tests of the database run on SQLite, and on PostgreSQL too when `RTYPE_TEST_POSTGRESQL_URL` gives a server
they can create schemas on, for instance in Docker:

```sh
docker run -d --name rtype-test-postgres -e POSTGRES_USER=rtype -e POSTGRES_PASSWORD=rtype \
    -e POSTGRES_DB=rtype -p 127.0.0.1:55432:5432 postgres:18
export RTYPE_TEST_POSTGRESQL_URL=postgresql://rtype:rtype@127.0.0.1:55432/rtype
ctest --test-dir build -C Release --output-on-failure
```

### Sanitizers

`RTYPE_SANITIZERS` builds everything, the libraries, the programs, the tests and the dependencies, under the
sanitizers, with GCC or Clang:

| Value | Finds |
|---|---|
| `address,undefined` | Memory errors (use after free, overflows), leaks, undefined behaviour |
| `thread` | Data races and lock order inversions between threads |

The two cannot be combined: use one build directory for each.

```sh
cmake -S . -B build-asan -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo -DRTYPE_SANITIZERS=address,undefined
cmake --build build-asan --parallel
ctest --test-dir build-asan --output-on-failure
```

Any report stops the program and fails its test, undefined behaviour included. The `sanitizers.*` tests run a
program with a bug made on purpose (`tests/helpers/sanitizer_canary.cpp`) and pass only when the sanitizers
report it: a build where they would stay silent fails there.

The options of the sanitizers and their suppressions are in one file, `cmake/Sanitizers.cpp`, linked into every
program of these builds: the tests, the programs started by hand and those the tests start all run the same
way. A suppression comes with a comment that says why the report is not our bug. The variables `ASAN_OPTIONS`,
`UBSAN_OPTIONS`, `LSAN_OPTIONS` and `TSAN_OPTIONS` still override the options for one run.

??? question "ThreadSanitizer: unexpected memory mapping"
    Recent kernels place the programs where older versions of ThreadSanitizer refuse to run. Run the tests
    without the randomization of the addresses, as the CI does: `setarch "$(uname -m)" -R ctest ...`.

### Formatting

Nothing to install: on pull requests, the *Format* workflow **formats the code automatically**. When a file
needs it, the format bot pushes a `style: apply clang-format` commit to your branch and leaves a comment.
Get its commit before pushing again:

```sh
git pull --rebase
```

To avoid this round trip, enable *format on save* in your editor: VS Code (C/C++ extension), CLion and
Visual Studio read the repository's `.clang-format` without extra setup.

To format by hand with the exact version used by the CI:

```sh
pip install -r requirements-dev.txt
clang-format -i path/to/file.cpp
```

## Continuous integration

| Workflow | Runs | Checks |
|---|---|---|
| *Build* | Every push and pull request, Linux and Windows | Compilation, all CTest tests, packaging. On Linux, the tests of the database also run on a PostgreSQL in Docker |
| *Sanitizers* | Every pull request, and pushes to `main` | All CTest tests under ASan and UBSan, on SQLite and PostgreSQL (Linux) |
| *Thread sanitizer* | Every night on `main`, and on demand | All CTest tests under TSan. A failed run blocks the next release until a run on `main` passes again, see [Releasing](releasing.md) |
| *Format* | Every pull request, and pushes to `main` that change C++ files | Formats tracked C++ files; on pull requests the bot commits the fixes, elsewhere unformatted code fails the job |

### Runners

The workflows run on the team's self-hosted runners. The Linux runner needs:

- **Docker**, usable by the user of the runner: the tests of the database start their PostgreSQL in it. Without
  Docker, *Build* and *Sanitizers* fail on Linux instead of testing on SQLite only.
- `sudo` without a password for `apt-get`, which installs the system libraries (X11, OpenGL, libpq).
- GCC or Clang with their sanitizer runtimes (`libasan`, `libubsan`, `libtsan`), Ninja and CMake 3.25 or newer.

The Windows runner needs Visual Studio 2022 and CMake; it tests the database on SQLite only.

## Roadmap

Steps still to set up, and what triggers each of them. Tick the box and move the tool to *In place* above
when done.

### Step 2 — when the protocol decoder and the ECS exist

- [x] CI job `sanitizers`: build with `-fsanitize=address,undefined`, runs all tests (Linux).
- [ ] Fuzz target `tests/fuzz/packet_decoder.cpp` (`LLVMFuzzerTestOneInput`) calling the packet decoder.
- [ ] CI job `fuzz`: runs the fuzzer for a few minutes on pull requests that touch the network code, and
      longer every night; crashing inputs are kept as regression tests.
- [ ] Unit tests for every message of the [protocol](../protocol/index.md), including each rejection case.

### Step 3 — when the server runs several threads

- [x] CI job `tsan`: `-fsanitize=thread` build running all the tests, every night.
- [ ] Integration test: server and bot clients over `127.0.0.1`, including a client killed mid-game.
- [ ] clang-tidy with a small set of checks (`bugprone-*`, `performance-*`), extended progressively.

### Step 4 — part 2 of the project

- [ ] Coverage report (gcovr) published under `/coverage/` of this site.
- [ ] Catch2 benchmarks for serialization and ECS iteration.
- [ ] Network condition simulation (loss, latency, reordering) in the fake socket, with measurements of
      bandwidth and responsiveness.
