#!/usr/bin/env python3
import json
import plistlib
import re
import subprocess
import sys
from pathlib import Path
from typing import List, Optional


bundle = Path(sys.argv[1]).resolve()
contents = bundle / "Contents"
main = contents / "MacOS" / "AtumDrive"
required = [
    main,
    contents / "PlugIns" / "OpenCloud_vfs_off.so",
    contents / "PlugIns" / "platforms" / "libqcocoa.dylib",
    contents / "PlugIns" / "platforms" / "libqminimal.dylib",
    contents / "PlugIns" / "tls" / "libqsecuretransportbackend.dylib",
    contents / "Frameworks" / "libfreetype.6.dylib",
]
for path in required:
    if not path.exists():
        raise SystemExit("missing required bundle component: {}".format(path))
if any(bundle.rglob("FinderSyncExt.appex")):
    raise SystemExit("unexpected FinderSync extension in minimal engine bundle")

with (contents / "Info.plist").open("rb") as handle:
    plist = plistlib.load(handle)
expected_plist = {
    "CFBundleIdentifier": "com.atumplatform.drive",
    "CFBundleExecutable": "AtumDrive",
    "LSMinimumSystemVersion": "14.0",
    "LSUIElement": True,
}
for key, expected in expected_plist.items():
    if plist.get(key) != expected:
        raise SystemExit("unexpected plist {}: {!r}".format(key, plist.get(key)))


def output(*command: str) -> str:
    return subprocess.check_output(command, text=True, stderr=subprocess.STDOUT)


def is_macho(path: Path) -> bool:
    return "Mach-O" in output("/usr/bin/file", "-b", str(path))


def dependencies(path: Path) -> List[str]:
    return [
        line.strip().split(" (", 1)[0]
        for line in output("/usr/bin/otool", "-L", str(path)).splitlines()[1:]
        if line.strip()
    ]


def rpaths(path: Path) -> List[str]:
    lines = output("/usr/bin/otool", "-l", str(path)).splitlines()
    found = []
    for index, line in enumerate(lines):
        if line.strip() == "cmd LC_RPATH":
            for candidate in lines[index + 1 : index + 6]:
                match = re.match(r"\s*path (.+) \(offset \d+\)", candidate)
                if match:
                    found.append(match.group(1))
                    break
    return found


def without_prefix(value: str, prefix: str) -> str:
    return value[len(prefix) :]


def resolve_dependency(path: Path, dependency: str) -> Optional[Path]:
    if dependency.startswith("@rpath/"):
        return contents / "Frameworks" / without_prefix(dependency, "@rpath/")
    if dependency.startswith("@loader_path/"):
        return (path.parent / without_prefix(dependency, "@loader_path/")).resolve()
    if dependency.startswith("@executable_path/"):
        return (
            contents / "MacOS" / without_prefix(dependency, "@executable_path/")
        ).resolve()
    return None


records = []
for path in sorted(contents.rglob("*")):
    if path.is_symlink() or not path.is_file() or not is_macho(path):
        continue
    archs = output("/usr/bin/lipo", "-archs", str(path)).strip().split()
    if archs != ["arm64"]:
        raise SystemExit("non-arm64 Mach-O {}: {}".format(path, archs))
    deps = dependencies(path)
    for dependency in deps:
        if dependency.startswith(("/System/Library/", "/usr/lib/")):
            continue
        resolved = resolve_dependency(path, dependency)
        if resolved is None:
            raise SystemExit(
                "external absolute dependency in {}: {}".format(path, dependency)
            )
        if not resolved.exists():
            raise SystemExit(
                "unresolved dependency in {}: {} -> {}".format(
                    path, dependency, resolved
                )
            )
    paths = rpaths(path)
    for value in paths:
        if value.startswith("/"):
            raise SystemExit("external absolute LC_RPATH in {}: {}".format(path, value))
    build = output("/usr/bin/vtool", "-show-build", str(path))
    platform = re.search(r"\bplatform (\S+)", build)
    minos = re.search(r"\bminos (\S+)", build)
    sdk = re.search(r"\bsdk (\S+)", build)
    records.append(
        {
            "path": str(path.relative_to(bundle)),
            "archs": archs,
            "platform": platform.group(1) if platform else None,
            "minos": minos.group(1) if minos else None,
            "sdk": sdk.group(1) if sdk else None,
            "dependencies": deps,
            "rpaths": paths,
        }
    )

main_record = next(
    record for record in records if record["path"] == "Contents/MacOS/AtumDrive"
)
if main_record["platform"] != "MACOS" or main_record["minos"] != "14.0":
    raise SystemExit("unexpected main build target: {}".format(main_record))
print(
    json.dumps(
        {
            "result": "pass",
            "bundle": str(bundle),
            "machoCount": len(records),
            "main": main_record,
        },
        sort_keys=True,
    )
)
for record in records:
    print(json.dumps(record, sort_keys=True))
