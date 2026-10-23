# Building the Atum Drive engine

This tree is the complete corresponding source of the Atum Drive sync engine
that ships inside the Atum desktop app for macOS. The engine is a modified
version of OpenCloud Desktop. It runs as its own program; the Atum app starts
it and talks to it over standard input and output.

## Atum Drive engine 0.20.17 source-offer record

This record describes the engine prepared for Atum desktop 0.20.17. It is
separate from the historical 0.18 record below.

| | |
|---|---|
| Publication status | Prepared source-offer record; `drive-engine-0.20.17` tag and release are not yet published |
| Atum desktop release | 0.20.17 for macOS on Apple silicon |
| Location in the app | `Contents/Resources/drive-engine/AtumDriveEngine.app` |
| Engine name and version | Atum Drive 4.0.0, bundle identifier `com.atumplatform.drive`, executable `AtumDrive` |
| Compiled source revision | `919d5ddaca626098712632a54428d5bab8f571bd` |
| Reviewed documentation head | `f0309c60544c0eb6bf73c85740c834a0d1730a6e`; its only delta from the compiled revision is `docs/session-logs/2026-10-23-cleanup-empty-credentials.md` |
| Source-offer tag | `drive-engine-0.20.17`, to be created on the completed documentation descendant after source checks and before publication; the tag commit is deliberately not claimed to be the compiled revision |
| Upstream base | OpenCloud Desktop v4.0.0, commit `9f701e6db2e101cb3247d3331647a60266e83b0f`, https://github.com/opencloud-eu/desktop |

The release manifest must retain the actual compiled source revision above.
The future source tag contains that implementation plus the reviewed build,
licence and source-offer documentation needed to reproduce and understand the
distributed bundle. Record the final tag commit separately when the tag is
created; never replace the compiled revision with the documentation tag SHA.

Hashes for the prepared 0.20.17 engine are:

- unsigned `AtumDrive` SHA-256:
  `53741d34db72413701492d166d2f319c5ff3229e80030a86554f4344f76b82cb`;
- Developer ID-signed `AtumDrive` SHA-256:
  `2681e4747ceee503eff986cc8e530dbb12038dcee0d4035b611828a31026ea55`;
- deterministic unsigned bundle inventory SHA-256:
  `52e462198fb1a09dee66d187ffdd21f2da63691ad4faa696134a5876ad27aa5a`;
- signed runtime bundle inventory SHA-256:
  `073719dce3f53c7134411adab7431ec546d62254b527763d33e02403ab8ebc04`.

Signing changes Mach-O bytes and therefore changes both the executable and
bundle hashes. The unsigned hash is the assembly input; the signed executable
and runtime bundle hashes are the values the desktop manifest must enforce.

## Historical 0.18 release record

| | |
|---|---|
| Atum desktop releases | 0.18.0 and 0.18.1 for macOS on Apple silicon, and any later Atum release whose engine manifest names the revision below |
| Location in the app | `Contents/Resources/drive-engine/AtumDriveEngine.app` |
| Engine name and version | Atum Drive 4.0.0, bundle identifier `com.atumplatform.drive`, executable `AtumDrive` |
| Source revision | `0b4bf330a0f8e4a2134785844fde69f66e6d04e9` (Atum's internal revision id; it is the `sourceCommit` field of `Contents/Resources/drive-engine/manifest.json` in the app) |
| Upstream base | OpenCloud Desktop v4.0.0, commit `9f701e6db2e101cb3247d3331647a60266e83b0f`, https://github.com/opencloud-eu/desktop |

The tag `drive-engine-0.18` (commit `7f3037af2a159e5f19e1eea9aac0fb96f209e117`)
holds exactly the source tree of that revision, plus this file. This file on
`main` is the current version of these instructions: it corrects and extends
the copy in the tag.

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

- `oem/atum/`: the Atum product profiles (name, bundle identifier, fixed Drive
  origin `https://drive.atumplatform.com`, fixed sign-in issuer
  `https://auth.atumplatform.com/`, public OAuth client id, CC0 artwork). The
  build refuses to configure without an explicit profile. `s2` is the retired
  disposable S2 profile, `h2` is the current separately authorized acceptance
  realm, and releases use `production`. H2 has its own bundle/keychain identity
  and is never a release artifact.
- `src/gui/atumengine.*`: the `--atum-engine` mode, a background process that
  speaks a small newline-delimited JSON protocol on stdin/stdout.
- `src/gui/atumrootbinding.*`, `src/gui/atumfilemetadata.*`: binding one sync
  folder to one signed-in owner, folder safety checks, and file metadata replies.
- `src/libsync/` and `src/gui/`: credential storage that waits for the system
  keychain before reporting success, transport pinned to the profile's origin and
  issuer, extra exclusions for credential and tool folders, and related fixes.
- Tests for the above in `test/`, plus CI and design notes in `.github/` and
  `docs/`.

`git diff 9f701e6db2e101cb3247d3331647a60266e83b0f 919d5ddaca626098712632a54428d5bab8f571bd`
against upstream shows the compiled 0.20.17 change set. The initial 0.18 engine
was prepared between 2 and 3 October 2026; later commits add the reviewed 1.1
facts, custody and cleanup behavior recorded in this repository.

## Atum Drive engine 0.20.17 unsigned build and assembly

### Current toolchain and dependency record

The prepared bundle was built on Apple silicon with:

| Component | Version or revision |
|---|---|
| Xcode / macOS SDK | Xcode 27.0 (`27A266a`), SDK 27.0, Apple Clang 21.0.0 |
| Minimum macOS | 14.0 for `AtumDrive`; bundled dependencies have compatible minimums of 13.0, 13.3 or 14.0 |
| Qt | 6.11.1 |
| QtKeychain | 0.16.0 |
| libre-graph-api-cpp-qt-client | 1.0.7 |
| KDSingleApplication | 1.2.0 |
| OpenSSL / ICU / SQLite | 3.6.3 / 78.1 / 3.50.4 |
| GLib / FreeType / HarfBuzz | 2.86.0 / 2.14.1 / 11.2.0 |
| ECM / CMake / Ninja | 6.29.0 / 4.1.4 / 1.13.2 |
| KDE Craft blueprints | `11b07503ab24e84affc5ed9c94093f8b03b05fd4` |
| OpenCloud Craft blueprints | `4b5f539521b7a68d51417faffe5278694a68996f` |
| Craft Core | `b80e8f8c6c4f9fc8c09464dc8b696bfa625fa12c` |

`.craft.shelf` is the machine-readable authority for the complete dependency
set. The 0.20.17 bundle intentionally keeps OpenSSL 3.6.3 and the existing
pins; historical planning notes proposing other versions are not build input.

### Configure and build

Restore the pinned Craft SDK as described in the historical commands below,
then configure a fresh production build. The embedded Atum engine does not ship
the FinderSync extension, so use `BUILD_SHELL_INTEGRATION=OFF`:

```sh
SOURCE_DIR=/path/to/atum-drive-companion-at-919d5dda
BUILD_DIR=/new/path/atum-drive-production-build
SDK_DIR=/path/to/craft/macos-clang-arm64

"$SDK_DIR/dev-utils/bin/cmake" -S "$SOURCE_DIR" -B "$BUILD_DIR" -G Ninja \
  -DCMAKE_MAKE_PROGRAM="$SDK_DIR/dev-utils/bin/ninja" \
  -DCMAKE_BUILD_TYPE=RelWithDebInfo \
  -DCMAKE_PREFIX_PATH="$SDK_DIR" \
  -DCMAKE_C_COMPILER=/usr/bin/cc \
  -DCMAKE_CXX_COMPILER=/usr/bin/c++ \
  -DOEM_THEME_DIR="$SOURCE_DIR/oem/atum" \
  -DATUM_DRIVE_PROFILE=production \
  -DBUILD_TESTING=ON \
  -DBUILD_SHELL_INTEGRATION=OFF
"$SDK_DIR/dev-utils/bin/cmake" --build "$BUILD_DIR" --parallel 2
```

Run the native state, protocol, root-binding and root-folder tests appropriate
to the production profile before assembly. Exact-head CI and review are
additional evidence; they do not replace the source/toolchain record.

### Reproducible unsigned assembly

Use the repository recipe with a new output directory:

```sh
scripts/assemble-atum-engine-unsigned.sh \
  --build-dir "$BUILD_DIR" \
  --source-dir "$SOURCE_DIR" \
  --sdk-dir "$SDK_DIR" \
  --output-dir /new/path/unsigned-engine-output \
  --compiled-source 919d5ddaca626098712632a54428d5bab8f571bd \
  --reviewed-head f0309c60544c0eb6bf73c85740c834a0d1730a6e \
  --source-tag drive-engine-0.20.17 \
  --source-archive /path/to/compiled-source-919d5dda.tar \
  --reviewed-source-archive /path/to/reviewed-documentation-source.tar \
  --implementation-equivalence-receipt /path/to/equivalence.txt \
  --craft-override /path/to/craft-override.ini
```

The recipe runs `macdeployqt` with `-no-plugins -no-codesign -no-strip`, copies
only the Cocoa/minimal platform plugins and Secure Transport TLS plugin, fixes
the minimal plugin's FreeType reference, removes absolute build/SDK rpaths,
sets `LSUIElement`, renames the bundle, and writes deterministic file, Mach-O,
toolchain and source-provenance receipts. It never signs, installs, notarizes or
deploys anything and never reads a signing identity.

The retained unsigned bundle contains 48 arm64 Mach-O files and 134 canonical
file/link inventory entries. Its main executable records macOS minOS 14.0 and
SDK 27.0; every non-system dependency resolves inside the bundle and no
absolute build or SDK rpath remains.

The unsigned assembled bundle is not expected to launch on Apple silicon after
load-command changes invalidate linker ad-hoc signatures. The recipe therefore
proves hello/stop against the exact pre-assembly production executable and
records assembled runtime verification as a post-signing gate. A successful
ad-hoc development signature would not be Developer ID release evidence.

The verified minimal plugin inventory is:

- `Contents/PlugIns/OpenCloud_vfs_off.so`;
- `Contents/PlugIns/platforms/libqcocoa.dylib`;
- `Contents/PlugIns/platforms/libqminimal.dylib`;
- `Contents/PlugIns/tls/libqsecuretransportbackend.dylib`.

No FinderSync extension is part of this engine bundle.

## Historical 0.18 build and assembly record

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

For the approved H2 acceptance realm, replace `production` above with `h2`. A
pull-request Apple Silicon lane does this automatically, executes the compiled
engine far enough to verify its closed H2 identity and offered 1.1 features,
then uploads the unsigned test bundle. That hello receipt is not the remaining
folder-ready/accepted-only/live-sync proof and does not authorize extending an
expired realm.

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

### Libraries in the engine bundle

The engine bundle also contains third-party libraries, built by KDE Craft from
the recipes pinned in `.craft.shelf`: Qt, QtKeychain, KDSingleApplication,
libre-graph-api-cpp-qt-client, OpenSSL, ICU, GLib, gettext (libintl),
FreeType, HarfBuzz, libpng, PCRE2, Brotli, bzip2, libb2, zlib, Zstandard
(zstd) and SQLite. Each keeps its own licence. Qt, GLib and libintl are under
the GNU Lesser General Public License (LGPL).

Craft patches some of these libraries before building them:

- **Qt 6.11.1.** Craft applies the patches in `libs/qt6/qtbase/.craft` of
  craft-blueprints-kde at revision `11b07503ab24e84affc5ed9c94093f8b03b05fd4`
  (the revision in `.craft.shelf`) to qtbase:
  `0001-Implement-QTEST_MAX_WARNINGS.patch`,
  `qstandardpaths-extra-dirs-macos.diff` and
  `qstandardpaths-fix-genericdatalocation.diff`. The shipped QtCore therefore
  differs from upstream Qt. qtdeclarative is built without changes.
- **gettext 0.22.3.** Craft's recipe applies four patches. They change
  gettext-tools and the top-level build files, not the libintl sources that
  the engine ships.
- **GLib 2.86.0.** Built without changes.

### Exact sources of the LGPL libraries

The exact sources of Qt, GLib and libintl as shipped, with the Craft recipes
and patches that built them, are assets of the
[`drive-engine-0.18` release](https://github.com/atum-platform/atum-drive-companion/releases/tag/drive-engine-0.18):

| Asset | SHA-256 |
|---|---|
| `qtbase-everywhere-src-6.11.1.tar.xz` | `d9594a31228aa23ad6b531719a29b45f0f3989fe6c136d45767ea179f233c1ac` |
| `qtdeclarative-everywhere-src-6.11.1.tar.xz` | `52e670f670b0304f534b24f98c47ceb8a41bb710464414ebc9527ec71cc86aa4` |
| `qt-6.11.1-craft-recipes-11b07503.tar.gz` | `67b8d33490c4e92906df220d8f21fdf42417a9f0a2fc0802a5fab7e4cf56cd2a` |
| `glib-2.86.0.tar.xz` | `b5739972d737cfb0d6fd1e7f163dfe650e2e03740bb3b8d408e4d1faea580d6d` |
| `gettext-0.22.3.tar.gz` | `839a260b2314ba66274dae7d245ec19fce190a3aa67869bf31354cb558df42c7` |
| `glib-2.86.0-gettext-0.22.3-craft-recipes.tar.gz` | `864024f45d730b6a92d74748044c41a3e479e35f4522228744abe6f6dcc54d58` |

The 0.20.17 bundle uses these versions and recipes unchanged. Its source offer
therefore keeps the established assets at their stable 0.18 URLs rather than
publishing duplicate mirrors:

- [qtbase-everywhere-src-6.11.1.tar.xz](https://github.com/atum-platform/atum-drive-companion/releases/download/drive-engine-0.18/qtbase-everywhere-src-6.11.1.tar.xz)
- [qtdeclarative-everywhere-src-6.11.1.tar.xz](https://github.com/atum-platform/atum-drive-companion/releases/download/drive-engine-0.18/qtdeclarative-everywhere-src-6.11.1.tar.xz)
- [qt-6.11.1-craft-recipes-11b07503.tar.gz](https://github.com/atum-platform/atum-drive-companion/releases/download/drive-engine-0.18/qt-6.11.1-craft-recipes-11b07503.tar.gz)
- [glib-2.86.0.tar.xz](https://github.com/atum-platform/atum-drive-companion/releases/download/drive-engine-0.18/glib-2.86.0.tar.xz)
- [gettext-0.22.3.tar.gz](https://github.com/atum-platform/atum-drive-companion/releases/download/drive-engine-0.18/gettext-0.22.3.tar.gz)
- [gettext-0.22.3.tar.gz.sig](https://github.com/atum-platform/atum-drive-companion/releases/download/drive-engine-0.18/gettext-0.22.3.tar.gz.sig)
- [glib-2.86.0-gettext-0.22.3-craft-recipes.tar.gz](https://github.com/atum-platform/atum-drive-companion/releases/download/drive-engine-0.18/glib-2.86.0-gettext-0.22.3-craft-recipes.tar.gz)
- [SHA256SUMS.txt](https://github.com/atum-platform/atum-drive-companion/releases/download/drive-engine-0.18/SHA256SUMS.txt)

The Qt and GLib tarballs are byte for byte the files their projects publish
(download.qt.io and download.gnome.org) and match the SHA-256 values published
there. The gettext tarball is the one on ftp.gnu.org; GNU's signature for it,
`gettext-0.22.3.tar.gz.sig`, is beside it in the release and verifies against
the GNU keyring. Each recipe archive has a `README-ATUM.txt` saying which of
its patches apply. `SHA256SUMS.txt` in the release lists all six files. The
source of every other library is available from its project at the version in
`.craft.shelf`.

### Written offer

For at least three years after Atum last distributes a release that contains
this engine, Anka Ventures Vietnam Company Limited, the maker of Atum, will
give anyone who asks a complete, machine-readable copy of the corresponding
source of the engine and of every library in it, on a medium customarily used
for software interchange, for a charge no more than our cost of physically
performing the distribution. Write to support@atumplatform.com.

### Code inside the engine's own libraries

The engine source includes, under their own licences:

- `src/3rdparty/QProgressIndicator`: QProgressIndicator, MIT, copyright (c)
  2011 Morgan Leborgne. Built into `libOpenCloudGui`.
- `src/resources/font-awesome`: the Font Awesome 7 Free Solid font, version
  7.3.1, under the SIL Open Font License 1.1 (`LICENSE.txt` in that folder).
  Embedded in `libOpenCloudResources`.
- `src/resources/remixicon`: the Remix Icon font, version 4.6, under the
  Apache License 2.0 (`License.txt` in that folder). Embedded in
  `libOpenCloudResources`.

## Replacing Qt, GLib or libintl in your copy of Atum

Qt, GLib and libintl are separate dynamic libraries inside
`AtumDriveEngine.app/Contents/Frameworks`. You may replace them with your own
interface-compatible builds, for example ones built from the sources above.

Atum checks the engine before it starts it.
`Contents/Resources/drive-engine/manifest.json` in the app records the SHA-256
of the engine executable (`sha256`) and a digest of every file in the engine
bundle (`bundleSha256`). Atum does not start an engine that does not match.
So after you replace a library, re-sign the engine, record its new hashes,
then re-sign Atum:

```sh
# The Atum app as Terminal names it. For the 0.18 releases this is
# "/Applications/Atum Matrix Candidate.app" (Finder shows it as Atum).
APP="/Applications/Atum Matrix Candidate.app"
ENGINE="$APP/Contents/Resources/drive-engine"

# 1. Replace the library. For example, QtCore:
cp /path/to/your/QtCore \
  "$ENGINE/AtumDriveEngine.app/Contents/Frameworks/QtCore.framework/Versions/A/QtCore"

# 2. Re-sign the engine ad hoc. Its Developer ID signature no longer matches.
codesign --force --deep --sign - "$ENGINE/AtumDriveEngine.app"

# 3. Record the engine's new hashes in manifest.json.
python3 - "$ENGINE" <<'EOF'
import hashlib, json, os, sys
engine = os.path.realpath(sys.argv[1])
manifest_file = os.path.join(engine, "manifest.json")
manifest = json.load(open(manifest_file))
bundle = os.path.join(engine, manifest["binary"].split("/")[0])
digest = hashlib.sha256()
def walk(folder):
    for name in sorted(os.listdir(folder)):
        path = os.path.join(folder, name)
        rel = os.path.relpath(path, bundle)
        if os.path.islink(path):
            entry = [rel, "link", os.readlink(path)]
        elif os.path.isdir(path):
            walk(path)
            continue
        else:
            entry = [rel, "file", hashlib.sha256(open(path, "rb").read()).hexdigest()]
        digest.update((json.dumps(entry, separators=(",", ":"), ensure_ascii=False) + "\n").encode())
walk(bundle)
binary = hashlib.sha256(open(os.path.join(engine, manifest["binary"]), "rb").read()).hexdigest()
manifest["sha256"] = manifest["binarySha256"] = binary
manifest["bundleSha256"] = digest.hexdigest()
json.dump(manifest, open(manifest_file, "w"), indent=2)
print("sha256", binary)
print("bundleSha256", manifest["bundleSha256"])
EOF

# 4. Re-sign Atum ad hoc, since its contents changed.
codesign --force --deep --sign - "$APP"
```

`bundleSha256` is the SHA-256 of one JSON line per file or symbolic link in the
engine bundle, walking the bundle depth first with the entries of each folder
in sorted order: `[path,"file",<SHA-256 of the file>]` or
`[path,"link",<link target>]`, with paths relative to `AtumDriveEngine.app`.
Step 3 computes it the same way Atum does.

A copy changed this way is no longer signed by Atum, so macOS treats it as a
different app. It may ask again before Atum or the engine can read their
keychain items, and Atum's automatic updates may not install over it.
Reinstalling Atum from https://atumplatform.com restores the original.

## Licence

OpenCloud Desktop and the Atum changes to it are licensed under the GNU General
Public License, version 2 or (at your option) any later version. See `COPYING`.
The engine as Atum distributes it links Qt (LGPL-3.0) and OpenSSL 3
(Apache-2.0), which are compatible with version 3 of the GPL but not with
version 2. Atum therefore distributes the engine binary under the GNU General
Public License, version 3, as the "any later version" option allows. The
source itself stays under GPL-2.0-or-later. The Atum artwork in `oem/atum/` and
its generator are CC0-1.0.

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful, but
    WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
    General Public License for more details.

Questions about this source: support@atumplatform.com.
