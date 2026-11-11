# Building the project

The project is built with CMake. Third-party libraries are downloaded and built automatically by
[CPM.cmake](https://github.com/cpm-cmake/CPM.cmake) during the first configure: nothing has to be installed
system-wide besides a compiler and, on Linux, the X11/OpenGL and OpenSSL development packages.

## Requirements

- CMake ≥ 3.25
- A C++20 compiler: GCC or Clang on Linux, Microsoft Visual C++ (Visual Studio 2022) on Windows
- Internet access for the first configure

=== "Debian / Ubuntu"

    ```sh
    sudo apt install build-essential cmake \
        libx11-dev libxrandr-dev libxinerama-dev libxcursor-dev libxi-dev libgl1-mesa-dev libssl-dev libpq-dev \
        libpulse0 libasound2t64
    ```

=== "Arch Linux"

    ```sh
    sudo pacman -S base-devel cmake libx11 libxrandr libxinerama libxcursor libxi mesa openssl postgresql-libs \
        libpulse alsa-lib
    ```

=== "Windows"

    Install [Visual Studio 2022](https://visualstudio.microsoft.com/) with the
    **Desktop development with C++** workload, which includes CMake.

OpenSSL (`libssl-dev`, `openssl`) is required on Linux: libcurl, which sends the mails of the server and is the
HTTP client of the tests, takes its TLS from it, and so does the fake mail server of the tests. Windows uses its
own (Schannel).

PulseAudio and ALSA (`libpulse0`, `libasound2t64`) are required on Linux by the Discord Social SDK the client
signs in to Discord with ([ADR 0026](../decisions/0026-discord-social-sdk.md)); the SDK itself is in the
repository, in `libs/discord/sdk/`, for Linux x86-64 and Windows x64, the only platforms the client builds on. Elsewhere, Linux aarch64 for
instance, the configure warns and builds the server only.

libpq (`libpq-dev`, `postgresql-libs`) is optional: with it, the server can keep its data in a PostgreSQL
database; without it, in SQLite only. The configure says which: `Database engines: SQLite, PostgreSQL`. On
Windows, PostgreSQL is left out unless asked: it comes with the
[PostgreSQL installer](https://www.postgresql.org/download/windows/), then `-DRTYPE_WITH_POSTGRESQL=ON`
([Database](../architecture/database.md#building-with-postgresql)).

## Configure and build

=== "Linux"

    ```sh
    cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
    cmake --build build --parallel
    ```

    Binaries are written to `build/bin/`.

=== "Windows"

    From a *Developer PowerShell for VS 2022*:

    ```powershell
    cmake -S . -B build
    cmake --build build --config Release --parallel
    ```

    Binaries are written to `build\bin\Release\`.

## Running the tests

```sh
ctest --test-dir build -C Release --output-on-failure
```

See the [testing policy](../contributing/testing.md) for how tests are organised and written.

## Sharing downloaded dependencies

By default, each build directory downloads its own copy of the dependencies. Point CPM to a shared cache
to download them only once:

=== "Linux"

    ```sh
    export CPM_SOURCE_CACHE="$HOME/.cache/CPM"
    ```

=== "Windows"

    ```powershell
    setx CPM_SOURCE_CACHE "$env:LOCALAPPDATA\CPM"
    ```

The variable is read when a build directory is configured for the first time.

## Packaging

```sh
cd build
cpack -C Release
```

This produces one archive per program: `r-type-<version>-Linux-client.tar.gz` and
`r-type-<version>-Linux-server.tar.gz` on Linux, `.zip` files on Windows. Published versions are built the
same way, see [Releasing](../contributing/releasing.md).

The server archive carries the administration panel, as `admin/` beside the server, when it is given at the
configure: the panel is built by Node, apart from the game
([web/admin/README.md](https://github.com/Akinator31/Rtype/blob/main/web/admin/README.md)), and the CI does it.
Without it, the server runs, and its panel says it is not built.

```sh
(cd web/admin && npm ci && npm run build)
cmake -S . -B build -DRTYPE_ADMIN_PANEL_DIR=$PWD/web/admin/dist
```

The client shows the activity of the player in its Discord profile as the one of an application of Discord, given
at the configure by its id (from the [developer portal](https://discord.com/developers/applications)): the official
application of R-Type by default, `1565673565163102250`. Another one, or none with an empty id (unless the
settings of the client give one, [Rich presence](../architecture/client-online.md#rich-presence)):

```sh
cmake -S . -B build -DRTYPE_DISCORD_APPLICATION_ID=<id of the application>
```

The client is built with VR on Linux and Windows ([VR](../architecture/vr.md)): the configure says
`VR: OpenXR 1.1.60`. The OpenXR loader is downloaded and built with the game, and linked into the client, so no
OpenXR package is needed, to build or to play; on Linux it uses the GLX of Mesa, already listed above. Without
VR, for instance on a computer that cannot build it:

```sh
cmake -S . -B build -DRTYPE_OPENXR=OFF
```

## Troubleshooting

??? question "CMake reports a dependency path that no longer exists"

    The dependency cache location is stored in the build directory when it is first configured. Delete the
    build directory (or `build/CMakeCache.txt`) and configure again.

??? question "Missing X11 headers on Linux"

    Install the development packages listed in [Requirements](#requirements); raylib needs them to create
    its window.
