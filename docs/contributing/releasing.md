# Releasing

Releases are published automatically by the *Release* workflow when a version tag is pushed. Each release
contains the client and the server for Linux and Windows, and release notes generated from the pull requests
merged since the previous release.

## Versioning

Versions follow [Semantic Versioning](https://semver.org/): `MAJOR.MINOR.PATCH`.

| Release | Example | Typical use |
|---|---|---|
| Final | `v1.0.0` | Delivery of a project part, defense |
| Prerelease | `v1.0.0-beta.1`, `v1.0.0-rc.1` | Version to test before a delivery; marked as *prerelease* on GitHub |

The version is defined once, in `project(RType VERSION ...)` of the root `CMakeLists.txt`, and the tag must
match it: the workflow refuses a tag `v1.2.0` if the project version is `1.1.0`. A prerelease suffix is
ignored for this check (`v1.2.0-beta.1` matches `1.2.0`).

## Publishing a release

1. **Update the version.** In a pull request, change `VERSION` in the root `CMakeLists.txt`, for example:

    ```cmake
    project(RType
        VERSION 1.0.0
    ```

    Merge the pull request into `main`.

2. **Tag the merged commit** and push the tag:

    ```sh
    git switch main
    git pull
    git tag -a v1.0.0 -m "R-Type 1.0.0"
    git push origin v1.0.0
    ```

3. **Follow the workflow** in the *Actions* tab: *Check version*, then *Build* on Linux and Windows (tests
   included), then *Publish release*. The release appears in the *Releases* section of the repository.

## Release contents

| File | Content |
|---|---|
| `r-type-<version>-Linux-client.tar.gz` | `r-type_client` for Linux |
| `r-type-<version>-Linux-server.tar.gz` | `r-type_server` for Linux |
| `r-type-<version>-Windows-client.zip` | `r-type_client.exe` for Windows |
| `r-type-<version>-Windows-server.zip` | `r-type_server.exe` for Windows |
| `SHA256SUMS.txt` | Checksums of the archives |

To check a download:

```sh
sha256sum --check --ignore-missing SHA256SUMS.txt
```

## When something goes wrong

??? question "The workflow fails at *Check version*"

    The tag does not match the version in `CMakeLists.txt`, or is not of the form `vX.Y.Z`. Delete the tag,
    fix the version through a pull request, then tag again:

    ```sh
    git push origin --delete v1.0.0
    git tag -d v1.0.0
    ```

??? question "The build or the tests fail"

    No release is created. Fix the problem through a pull request, then delete and recreate the tag on the
    new commit of `main` (same commands as above).

??? question "A release was published by mistake"

    Delete it from the *Releases* page (or with `gh release delete v1.0.0`), then delete the tag. Prefer
    publishing a new patch version over replacing the files of a published release.
