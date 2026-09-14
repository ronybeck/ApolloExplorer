# ApolloExplorer Packaging

Scripts and metadata to build all three PC-side ApolloExplorer programmes —
the `ApolloExplorer` GUI client, the `acp` command-line copy tool, and the
`ash` command-line remote shell — into a single, native, installable package
per platform:

| Platform | Distro/Version   | Format | Script                        |
|----------|------------------|--------|--------------------------------|
| Linux    | Ubuntu (Debian)  | .deb   | `linux/build-deb.sh`          |
| Linux    | Fedora           | .rpm   | `linux/build-rpm.sh`          |
| macOS    | 12+ (Intel/ARM)  | .pkg   | `macos/build-pkg.sh`          |
| Windows  | Windows 11       | .msi   | `windows/build-msi.ps1`       |

Every package installs `acp` and `ash` onto the system `PATH`, so both are
runnable from any terminal/command prompt immediately after installing:

- **Ubuntu/Fedora**: installed to `/usr/bin`, already on `PATH`.
- **macOS**: installed to `/usr/local/bin`, already on `PATH`.
- **Windows**: installed alongside `ApolloExplorer.exe` under
  `Program Files\ApolloExplorer`, which the MSI adds to the machine `PATH`
  (a new shell is required to pick it up; no reboot needed).

Each script builds the project from source and produces a package under that
platform's `dist/` folder. They are independent of, and complement, the
existing `build_linux_client.sh` / `build_macos_client.sh` /
`build_windows_client.ps1` scripts at the repository root, which produce
loose (unpackaged) build folders and, on macOS, a signed/notarized `.dmg`.

## Linux

### Ubuntu / Debian (.deb)

```
packaging/linux/build-deb.sh [version]
```

Installs `build-essential` and the Qt6 dev packages if `qmake6` isn't found,
builds with qmake6/make, then stages and packages a `.deb` with `dpkg-deb`
into `packaging/linux/dist/`. Installs `ApolloExplorer`, `acp` and `ash` to
`/usr/bin`, with a desktop entry and icon under `/usr/share`.

Install the result with:

```
sudo apt install packaging/linux/dist/apolloexplorer_<version>_<arch>.deb
```

### Fedora (.rpm)

```
packaging/linux/build-rpm.sh [version]
```

Archives the current git `HEAD` (commit local changes first), builds it with
`rpmbuild` using `apolloexplorer.spec`, and drops the resulting `.rpm` into
`packaging/linux/dist/`.

Install the result with:

```
sudo dnf install packaging/linux/dist/apolloexplorer-<version>-1.*.rpm
```

## macOS (.pkg)

```
packaging/macos/build-pkg.sh [version]
```

Run on macOS with Qt 6 installed (e.g. `brew install qt@6`, with `qmake` on
`PATH`). Builds `ApolloExplorer.app`, deploys Qt via `macdeployqt`, and packs
it together with `acp` and `ash` into a `.pkg` under `packaging/macos/dist/`
using `pkgbuild`. `ApolloExplorer.app` is installed to `/Applications`; `acp`
and `ash` to `/usr/local/bin`.

To sign the package, export `APPLE_INSTALLER_IDENTITY` with a
"Developer ID Installer" certificate name before running the script.

For a signed/notarized `.app` + `.dmg` instead, use
`build_macos_client.sh` at the repository root.

## Windows 11 (.msi)

```
powershell -ExecutionPolicy Bypass -File packaging\windows\build-msi.ps1 -Version 1.4.0
```

Requires the same Qt6/MinGW setup as `build_windows_client.ps1`, plus the
[WiX Toolset](https://wixtoolset.org/) CLI (`dotnet tool install --global
wix`, which requires the .NET SDK). Builds the app, `acp` and `ash`, stages
them with `windeployqt`, then packages the folder with WiX
(`ApolloExplorer.wxs`) into `packaging/windows/dist/ApolloExplorer-<version>.msi`.
Installs to `Program Files\ApolloExplorer` with a Start Menu shortcut for the
GUI app, and adds the install folder to the machine `PATH` so `acp.exe` and
`ash.exe` are runnable from any command prompt or PowerShell session.

## Versioning

Each script takes the version as an optional argument/parameter. When
omitted, the version is extracted automatically from `VERSION_STRING` in
`protocolTypes.h` at the repository root, so packages stay in sync with the
protocol version by default. Pass a version explicitly to override this,
e.g. `packaging/linux/build-deb.sh 1.5.0`.
