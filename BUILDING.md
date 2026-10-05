# Building the Atum Drive engine

This tree is the complete corresponding source of the Atum Drive sync engine
that ships inside the Atum desktop app for macOS. The engine is a modified
version of OpenCloud Desktop. It runs as its own program; the Atum app starts
it and talks to it over standard input and output.

## Which releases this source belongs to

| | |
|---|---|
| Atum desktop releases | 0.18.0 and 0.18.1 for macOS on Apple silicon, and any later Atum release whose engine manifest names the revision below |
| Location in the app | `Contents/Resources/drive-engine/AtumDriveEngine.app` |
| Engine name and version | Atum Drive 4.0.0, bundle identifier `com.atumplatform.drive`, executable `AtumDrive` |
| Source revision | `0b4bf330a0f8e4a2134785844fde69f66e6d04e9` (Atum's internal revision id; it is the `sourceCommit` field of `Contents/Resources/drive-engine/manifest.json` in the app) |
| Upstream base | OpenCloud Desktop v4.0.0, commit `9f701e6db2e101cb3247d3331647a60266e83b0f`, https://github.com/opencloud-eu/desktop |

This commit contains exactly the source tree of that revision, plus this file.

To check which engine your copy of Atum contains, open
`Contents/Resources/drive-engine/manifest.json` inside the app and read
`sourceCommit`. For the releases above:

- SHA-256 of `AtumDrive` as shipped (signed with Atum's Developer ID):
  `e4b3c876f4aaea404b86f208bf718b9d1d4cc199ba44d22673a28d9c71e0b05a`
- SHA-256 of `AtumDrive` before Developer ID signing:
  `7c3d05d2ec044c1aa27c7aca3b94bb993ec19a887fbb56cd2b29971ba539159a`

Code signing rewrites every executable file, so a rebuild will not match these
hashes byte for byte. They identify the shipped files; they are not a
reproducible-build claim.

## What Atum changed

Compared with OpenCloud Desktop v4.0.0, this tree adds or changes:

- `oem/atum/`: the Atum product profile (name, bundle identifier, fixed Drive
  origin `https://drive.atumplatform.com`, fixed sign-in issuer
  `https://auth.atumplatform.com/`, public OAuth client id, CC0 artwork). The
  build refuses to configure without an explicit profile. `s2` is a disposable
  test profile; releases use `production`.
- `src/gui/atumengine.*`: the `--atum-engine` mode, a background process that
  speaks a small newline-delimited JSON protocol on stdin/stdout.
- `src/gui/atumrootbinding.*`, `src/gui/atumfilemetadata.*`: binding one sync
  folder to one signed-in owner, folder safety checks, and file metadata replies.
- `src/libsync/` and `src/gui/`: credential storage that waits for the system
  keychain before reporting success, transport pinned to the profile's origin and
  issuer, extra exclusions for credential and tool folders, and related fixes.
- Tests for the above in `test/`, plus CI and design notes in `.github/` and
  `docs/`.

`git diff 9f701e6db2e101cb3247d3331647a60266e83b0f` against upstream shows
every change. Atum made these changes between 2 and 3 October 2026.

## Build

These are the steps and settings recorded for the shipped engine. Signing is
left out.

### Toolchain

- An Apple silicon Mac. The shipped binaries record macOS SDK 14.5 and linker
  `ld` 1053.12 (the Xcode 15.4 toolchain) with a minimum macOS of 14.0.
- KDE Craft through CraftMaster (https://invent.kde.org/kde/craftmaster), target
  `macos-clang-arm64`. Craft supplies every library and build tool. The exact
  versions are pinned in `.craft.shelf` in this tree, including:
  - Qt 6.11.1 (qtbase, qtdeclarative, qtsvg, qtimageformats, qttools, qttranslations)
  - QtKeychain 0.16.0
  - libre-graph-api-cpp-qt-client 1.0.7
  - KDSingleApplication 1.2.0
  - OpenSSL 3.6.3, ICU 78.1, SQLite 3.50.4, GLib 2.86.0, FreeType 2.14.1, HarfBuzz 11.2.0
  - extra-cmake-modules 6.29.0, CMake 4.1.4, Ninja 1.13.2
- PowerShell 7 (`pwsh`) and Python 3, used by `.github/workflows/.craft.ps1`.

### 1. Restore the Craft dependencies

From the root of this tree:

```sh
git clone --depth=1 https://invent.kde.org/kde/craftmaster.git ~/craft/CraftMaster/CraftMaster
export CRAFT_TARGET=macos-clang-arm64
pwsh .github/workflows/.craft.ps1 --setup
pwsh .github/workflows/.craft.ps1 -c --unshelve "$PWD/.craft.shelf"
pwsh .github/workflows/.craft.ps1 -c craft
pwsh .github/workflows/.craft.ps1 -c --install-deps opencloud/opencloud-desktop
```

### 2. Configure and build the production profile

`.craft.ps1 -c --run` runs a command inside the Craft environment, so CMake finds
the Craft libraries. Run the configure step outside Craft only if you pass the
Craft root as `CMAKE_PREFIX_PATH` yourself.

```sh
pwsh .github/workflows/.craft.ps1 -c --run cmake -S "$PWD" -B "$PWD/build-atum" -G Ninja \
  -DCMAKE_BUILD_TYPE=RelWithDebInfo \
  "-DOEM_THEME_DIR=$PWD/oem/atum" \
  -DATUM_DRIVE_PROFILE=production
pwsh .github/workflows/.craft.ps1 -c --run cmake --build "$PWD/build-atum"
```

`oem/atum/OEM.cmake` then fixes the remaining options: virtual files off
(`VIRTUAL_FILE_SYSTEM_PLUGINS=off`), crash reporter off, auto-updater and update
notifications off, and `CMAKE_OSX_DEPLOYMENT_TARGET=14.0`. The build produces
`AtumDrive.app`, with the `OpenCloud_vfs_off.so` plugin in `Contents/PlugIns`.

To build and run the Atum tests as well, add `-DBUILD_TESTING=ON` and build the
targets `testatumrootbinding` and `testatumrootfolder`.

### 3. Assemble the bundle

The shipped bundle was assembled from the build output in these steps:

1. Run `macdeployqt` from the same Qt 6.11.1 SDK on `AtumDrive.app` to copy the
   Qt frameworks and the other dynamic libraries from the Craft root into
   `Contents/Frameworks`.
2. Copy `platforms/libqcocoa.dylib` and `platforms/libqminimal.dylib` from the
   Qt SDK's `plugins` directory into `Contents/PlugIns/platforms`. In
   `libqminimal.dylib`, change the FreeType reference from the SDK's absolute path
   to the bundled copy:
   ```sh
   install_name_tool -change <craft-root>/lib/libfreetype.6.dylib \
     @loader_path/../../Frameworks/libfreetype.6.dylib \
     Contents/PlugIns/platforms/libqminimal.dylib
   ```
3. Copy `plugins/tls/libqsecuretransportbackend.dylib` from the same Qt SDK
   into `Contents/PlugIns/tls`. Without it the engine has no TLS backend.
4. The `.iconset` that CMake generated was rejected by `iconutil` on the build
   machine. `Contents/Resources/atum.icns` was made instead from the committed
   PNGs in `oem/atum/theme/colored/` (16 to 1024 pixels). The shipped file's
   SHA-256 is `49ee1ced9c6f4c38b1a2220a6b65cd06158511c31a86604ba5c39dce048c218b`.
5. Mark the engine as a background program with no Dock icon:
   ```sh
   /usr/libexec/PlistBuddy -c "Add :LSUIElement bool true" AtumDrive.app/Contents/Info.plist
   ```
6. Rename the bundle directory to `AtumDriveEngine.app`.

These steps were carried out by hand. The receipts record their results: the file
hashes and the bundle contents. The exact `macdeployqt` arguments and the command
that staged the icon were not recorded.

The finished bundle holds these frameworks: QtConcurrent, QtCore, QtDBus, QtGui,
QtNetwork, QtOpenGL, QtQml, QtQmlMeta, QtQmlModels, QtQmlWorkerScript, QtQuick,
QtQuickControls2, QtQuickTemplates2, QtQuickWidgets, QtWidgets, QtXml and png. Its
plugins are `OpenCloud_vfs_off.so`, `platforms/libqcocoa.dylib`,
`platforms/libqminimal.dylib` and `tls/libqsecuretransportbackend.dylib`.

### 4. Signing (not included)

Atum signs the bundle leaf first, then signs it again with its Developer ID
(hardened runtime, secure timestamp) inside the Atum app. Signing needs Atum's
certificate and is not part of the source. An ad-hoc signed build
(`codesign --force --sign - --deep AtumDriveEngine.app`) runs locally for testing.

## Third-party components

The engine bundle also contains unmodified third-party libraries built by KDE
Craft from the recipes pinned in `.craft.shelf`: Qt, QtKeychain,
KDSingleApplication, libre-graph-api-cpp-qt-client, OpenSSL, ICU, GLib, gettext
(libintl), FreeType, HarfBuzz, libpng, PCRE2, Brotli, bzip2, libb2, zlib,
Zstandard (zstd) and SQLite. Each keeps its own licence; Qt, GLib and libintl are under the LGPL. Their source
is available from each project and through the Craft blueprints at the revisions
in `.craft.shelf`. For a copy of the source of any of them as shipped, write to
support@atumplatform.com.

## Licence

OpenCloud Desktop and the Atum changes to it are licensed under the GNU General
Public License, version 2 or (at your option) any later version. See `COPYING`.
The Atum artwork in `oem/atum/` and its generator are CC0-1.0.

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful, but
    WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
    General Public License for more details.

Questions about this source: support@atumplatform.com.
