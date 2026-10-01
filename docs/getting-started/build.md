# Building the project

The project is built with CMake. Third-party libraries are downloaded and built automatically by
[CPM.cmake](https://github.com/cpm-cmake/CPM.cmake) during the first configure: nothing has to be installed
system-wide besides a compiler and, on Linux, the X11/OpenGL development packages.

## Requirements

- CMake ≥ 3.25
- A C++20 compiler: GCC or Clang on Linux, Microsoft Visual C++ (Visual Studio 2022) on Windows
- Internet access for the first configure

=== "Debian / Ubuntu"

    ```sh
    sudo apt install build-essential cmake \
        libx11-dev libxrandr-dev libxinerama-dev libxcursor-dev libxi-dev libgl1-mesa-dev
    ```

=== "Arch Linux"

    ```sh
    sudo pacman -S base-devel cmake libx11 libxrandr libxinerama libxcursor libxi mesa
    ```

=== "Windows"

    Install [Visual Studio 2022](https://visualstudio.microsoft.com/) with the
    **Desktop development with C++** workload, which includes CMake.

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

## Troubleshooting

??? question "CMake reports a dependency path that no longer exists"

    The dependency cache location is stored in the build directory when it is first configured. Delete the
    build directory (or `build/CMakeCache.txt`) and configure again.

??? question "Missing X11 headers on Linux"

    Install the development packages listed in [Requirements](#requirements); raylib needs them to create
    its window.
