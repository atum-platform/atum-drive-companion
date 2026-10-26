#!/bin/bash
set -euo pipefail

SCRIPT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)

usage() {
    cat <<'EOF'
Usage: assemble-atum-engine-unsigned.sh \
  --build-dir PATH --source-dir PATH --sdk-dir PATH --output-dir NEW_PATH \
  --compiled-source SHA --reviewed-head SHA --source-tag TAG \
  [--source-archive PATH] [--reviewed-source-archive PATH] \
  [--implementation-equivalence-receipt PATH] [--craft-override PATH]

Assembles an unsigned, self-contained production AtumDriveEngine.app. The
output directory must not exist. This command never reads signing identities,
uses signing keys, signs code, installs an app, or submits anything to Apple.
EOF
}

BUILD_DIR=
SOURCE_DIR=
SDK_DIR=
OUTPUT_DIR=
COMPILED_SOURCE=
REVIEWED_HEAD=
SOURCE_TAG=
SOURCE_ARCHIVE=
REVIEWED_SOURCE_ARCHIVE=
EQUIVALENCE_RECEIPT=
CRAFT_OVERRIDE=

while (($#)); do
    case "$1" in
        --build-dir) BUILD_DIR=$2; shift 2 ;;
        --source-dir) SOURCE_DIR=$2; shift 2 ;;
        --sdk-dir) SDK_DIR=$2; shift 2 ;;
        --output-dir) OUTPUT_DIR=$2; shift 2 ;;
        --compiled-source) COMPILED_SOURCE=$2; shift 2 ;;
        --reviewed-head) REVIEWED_HEAD=$2; shift 2 ;;
        --source-tag) SOURCE_TAG=$2; shift 2 ;;
        --source-archive) SOURCE_ARCHIVE=$2; shift 2 ;;
        --reviewed-source-archive) REVIEWED_SOURCE_ARCHIVE=$2; shift 2 ;;
        --implementation-equivalence-receipt) EQUIVALENCE_RECEIPT=$2; shift 2 ;;
        --craft-override) CRAFT_OVERRIDE=$2; shift 2 ;;
        -h|--help) usage; exit 0 ;;
        *) echo "Unknown argument: $1" >&2; usage >&2; exit 2 ;;
    esac
done

for value in BUILD_DIR SOURCE_DIR SDK_DIR OUTPUT_DIR COMPILED_SOURCE REVIEWED_HEAD SOURCE_TAG; do
    if [[ -z ${!value} ]]; then
        echo "Missing required argument: $value" >&2
        usage >&2
        exit 2
    fi
done
[[ $COMPILED_SOURCE =~ ^[0-9a-f]{40}$ ]] || { echo "Invalid compiled source SHA" >&2; exit 2; }
[[ $REVIEWED_HEAD =~ ^[0-9a-f]{40}$ ]] || { echo "Invalid reviewed-head SHA" >&2; exit 2; }
[[ $(uname -s) == Darwin ]] || { echo "macOS is required" >&2; exit 2; }

SOURCE_APP="$BUILD_DIR/bin/AtumDrive.app"
WORK_APP="$OUTPUT_DIR/AtumDrive.app"
BUNDLE="$OUTPUT_DIR/AtumDriveEngine.app"
EVIDENCE="$OUTPUT_DIR/evidence"
RECIPE="$OUTPUT_DIR/recipe"

test ! -e "$OUTPUT_DIR"
test -x "$SOURCE_APP/Contents/MacOS/AtumDrive"
test -x "$SDK_DIR/bin/macdeployqt"
test -f "$SOURCE_DIR/.craft.shelf"
test -f "$BUILD_DIR/CMakeCache.txt"
test -f "$BUILD_DIR/src/libsync/atumprofile.h"
mkdir "$OUTPUT_DIR"
mkdir "$EVIDENCE" "$RECIPE"

for helper in assemble-atum-engine-unsigned.sh verify-atum-engine-bundle.py inventory-atum-engine-bundle.py hello-stop-atum-engine.py; do
    /bin/cp "$SCRIPT_DIR/$helper" "$RECIPE/$helper"
done
chmod 755 "$RECIPE"/*.sh "$RECIPE"/*.py

/usr/bin/ditto "$SOURCE_APP" "$WORK_APP"
test -e "$WORK_APP/Contents/PlugIns/OpenCloud_vfs_off.so"
"$SDK_DIR/bin/macdeployqt" "$WORK_APP" -verbose=3 -always-overwrite -no-strip -no-plugins -no-codesign > "$EVIDENCE/macdeployqt.log" 2>&1

mkdir -p "$WORK_APP/Contents/PlugIns/platforms" "$WORK_APP/Contents/PlugIns/tls"
/bin/cp -p "$SDK_DIR/plugins/platforms/libqcocoa.dylib" "$WORK_APP/Contents/PlugIns/platforms/"
/bin/cp -p "$SDK_DIR/plugins/platforms/libqminimal.dylib" "$WORK_APP/Contents/PlugIns/platforms/"
/bin/cp -p "$SDK_DIR/plugins/tls/libqsecuretransportbackend.dylib" "$WORK_APP/Contents/PlugIns/tls/"

test -e "$WORK_APP/Contents/Frameworks/libfreetype.6.dylib"
/usr/bin/install_name_tool -change "$SDK_DIR/lib/libfreetype.6.dylib" '@loader_path/../../Frameworks/libfreetype.6.dylib' \
    "$WORK_APP/Contents/PlugIns/platforms/libqminimal.dylib"

# macdeployqt rewrites dependency install names but can retain absolute build and
# SDK LC_RPATH entries. Remove only absolute rpaths and retain an exact receipt.
: > "$EVIDENCE/rpaths-removed.tsv"
while IFS= read -r -d '' file; do
    if /usr/bin/file -b "$file" | /usr/bin/grep -q Mach-O; then
        while IFS= read -r rpath; do
            test -n "$rpath" || continue
            printf '%s\t%s\n' "${file#$OUTPUT_DIR/}" "$rpath" >> "$EVIDENCE/rpaths-removed.tsv"
            /usr/bin/install_name_tool -delete_rpath "$rpath" "$file"
        done < <(/usr/bin/otool -l "$file" | /usr/bin/awk '$1=="cmd" && $2=="LC_RPATH" {want=1; next} want && $1=="path" {if ($2 ~ "^/") print $2; want=0}')
    fi
done < <(find "$WORK_APP/Contents" -type f -print0)

# macdeployqt processes the main executable and bundled frameworks, but not
# additional executables in Contents/MacOS (for example atum-drivecmd). Rewrite
# any absolute dependency whose library macdeployqt already bundled.
: > "$EVIDENCE/extra-executable-dependencies-rewritten.tsv"
for executable in "$WORK_APP"/Contents/MacOS/*; do
    test -x "$executable" || continue
    test "$executable" = "$WORK_APP/Contents/MacOS/AtumDrive" && continue
    while IFS= read -r dependency; do
        test -n "$dependency" || continue
        library=$(basename "$dependency")
        test -e "$WORK_APP/Contents/Frameworks/$library" || continue
        printf '%s\t%s\t%s\n' "${executable#$OUTPUT_DIR/}" "$dependency" "@loader_path/../Frameworks/$library" >> "$EVIDENCE/extra-executable-dependencies-rewritten.tsv"
        /usr/bin/install_name_tool -change "$dependency" "@loader_path/../Frameworks/$library" "$executable"
    done < <(/usr/bin/otool -L "$executable" | /usr/bin/awk 'NR>1 {print $1}' | /usr/bin/grep -E '^/' || true)
done

echo '49ee1ced9c6f4c38b1a2220a6b65cd06158511c31a86604ba5c39dce048c218b  '"$WORK_APP/Contents/Resources/atum.icns" \
    | /usr/bin/shasum -a 256 -c -
if /usr/libexec/PlistBuddy -c 'Print :LSUIElement' "$WORK_APP/Contents/Info.plist" >/dev/null 2>&1; then
    /usr/libexec/PlistBuddy -c 'Set :LSUIElement true' "$WORK_APP/Contents/Info.plist"
else
    /usr/libexec/PlistBuddy -c 'Add :LSUIElement bool true' "$WORK_APP/Contents/Info.plist"
fi
/bin/mv "$WORK_APP" "$BUNDLE"

/usr/bin/python3 "$RECIPE/verify-atum-engine-bundle.py" "$BUNDLE" > "$EVIDENCE/macho-closure.jsonl"
mkdir "$EVIDENCE/preassembly-fixture"
/usr/bin/python3 "$RECIPE/hello-stop-atum-engine.py" \
    --executable "$SOURCE_APP/Contents/MacOS/AtumDrive" \
    --evidence "$EVIDENCE/preassembly-fixture" \
    --issuer 'https://auth.atumplatform.com/' \
    --origin 'https://drive.atumplatform.com' \
    > "$EVIDENCE/preassembly-hello-stop-receipt.json"

first=$(/usr/bin/python3 "$RECIPE/inventory-atum-engine-bundle.py" "$BUNDLE" "$EVIDENCE/bundle-inventory.jsonl")
second=$(/usr/bin/python3 "$RECIPE/inventory-atum-engine-bundle.py" "$BUNDLE" "$EVIDENCE/bundle-inventory-second.jsonl")
test "$first" = "$second"
/usr/bin/cmp "$EVIDENCE/bundle-inventory.jsonl" "$EVIDENCE/bundle-inventory-second.jsonl"
echo "$first" > "$EVIDENCE/bundle-sha256.txt"
/usr/bin/shasum -a 256 "$BUNDLE/Contents/MacOS/AtumDrive" > "$EVIDENCE/executable-sha256.txt"

{
    echo "compiled_source=$COMPILED_SOURCE"
    echo "reviewed_head=$REVIEWED_HEAD"
    echo "source_tag=$SOURCE_TAG"
    echo 'profile=production'
    echo 'signed=false'
    echo 'finder_sync=false'
    echo 'assembled_runtime_smoke=requires_signature'
    echo 'macdeployqt_options=-verbose=3 -always-overwrite -no-strip -no-plugins -no-codesign'
} > "$EVIDENCE/provenance.txt"

/bin/cp "$SOURCE_DIR/.craft.shelf" "$EVIDENCE/craft.shelf"
/bin/cp "$SOURCE_DIR/.craft.ini" "$EVIDENCE/craft.ini"
/bin/cp "$SOURCE_DIR/BUILDING.md" "$EVIDENCE/BUILDING.md"
/bin/cp "$SOURCE_DIR/COPYING" "$EVIDENCE/COPYING"
/bin/cp "$BUILD_DIR/CMakeCache.txt" "$EVIDENCE/CMakeCache.txt"
/bin/cp "$BUILD_DIR/src/libsync/atumprofile.h" "$EVIDENCE/atumprofile.h"
[[ -z $CRAFT_OVERRIDE ]] || /bin/cp "$CRAFT_OVERRIDE" "$EVIDENCE/craft-override.ini"
[[ -z $SOURCE_ARCHIVE ]] || /bin/cp "$SOURCE_ARCHIVE" "$EVIDENCE/compiled-source.tar"
[[ -z $REVIEWED_SOURCE_ARCHIVE ]] || /bin/cp "$REVIEWED_SOURCE_ARCHIVE" "$EVIDENCE/reviewed-source.tar"
[[ -z $EQUIVALENCE_RECEIPT ]] || /bin/cp "$EQUIVALENCE_RECEIPT" "$EVIDENCE/implementation-equivalence.txt"

{
    /usr/bin/xcodebuild -version
    /usr/bin/clang++ --version
    "$SDK_DIR/dev-utils/bin/cmake" --version
    "$SDK_DIR/dev-utils/bin/ninja" --version
    "$SDK_DIR/bin/qmake" -query QT_VERSION
} > "$EVIDENCE/toolchain.txt"
/usr/bin/strings "$BUNDLE/Contents/Frameworks/libcrypto.3.dylib" \
    | /usr/bin/awk '/OpenSSL 3/ && !found {print; found=1} END {exit !found}' > "$EVIDENCE/openssl-version.txt"

# This is intentionally not a signing step. It records that load-command
# changes invalidate linker ad-hoc signatures and that parent signing is still
# required before assembled-bundle runtime verification.
/usr/bin/codesign -dvvv "$BUNDLE" > "$EVIDENCE/codesign-display.log" 2>&1 || true
/usr/bin/codesign --verify --deep --strict --verbose=2 "$BUNDLE" > "$EVIDENCE/codesign-verify-expected-failure.log" 2>&1 || true
{
    echo 'status=not_run_unsigned_bundle_requires_signature'
    echo 'authorization_required=ad-hoc test copy or Developer ID signing'
} > "$EVIDENCE/assembled-fixture-status.txt"

find "$BUNDLE" \( -type f -o -type l \) -print | LC_ALL=C sort > "$EVIDENCE/path-inventory.txt"
/usr/bin/du -sk "$BUNDLE" > "$EVIDENCE/bundle-du-kib.txt"
cd "$OUTPUT_DIR"
find evidence recipe -type f ! -path evidence/evidence-sha256.txt -print0 | LC_ALL=C sort -z \
    | xargs -0 /usr/bin/shasum -a 256 > evidence/evidence-sha256.txt

echo "bundle=$BUNDLE"
echo "bundleSha256=$first"
cat "$EVIDENCE/executable-sha256.txt"
