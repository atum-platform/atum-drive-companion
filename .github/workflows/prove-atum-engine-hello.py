#!/usr/bin/env python3
"""Exercise the compiled H2 engine through its real stdio entry point.

This stops after the immutable hello. It deliberately performs no OAuth, file,
realm, or keychain operation and therefore does not replace live acceptance.
"""

from __future__ import annotations

import argparse
import contextlib
import json
import os
import plistlib
import re
import subprocess
import tarfile
import tempfile
from collections.abc import Iterator
from pathlib import Path, PurePosixPath
from typing import NoReturn


FEATURES = {"exclusions", "progress", "quota", "sync"}
MACROS = {
    "ATUM_DRIVE_ORIGIN": "origin",
    "ATUM_MAS_ISSUER": "issuer",
    "ATUM_OAUTH_CLIENT_ID": "oauthClientId",
}


def stop(message: str, stderr: str = "") -> NoReturn:
    detail = stderr.strip()[-2000:]
    raise SystemExit(f"{message}{': ' + detail if detail else ''}")


def macro_values(header: Path) -> dict[str, str]:
    source = header.read_text(encoding="utf-8")
    values: dict[str, str] = {}
    for macro, field in MACROS.items():
        match = re.search(rf'^#define {macro} "([^"]+)"$', source, re.MULTILINE)
        if match is None:
            stop(f"compiled H2 profile header is missing {macro}")
        values[field] = match.group(1)
    return values


def engine_bundle(build_dir: Path, expected: dict[str, str]) -> Path:
    short_name = expected["applicationShortName"]
    path = build_dir / "bin" / f"{short_name}.app"
    try:
        bundle = path.resolve(strict=True)
    except FileNotFoundError:
        stop(f"compiled H2 bundle is missing from target output path {path}")
    if not bundle.is_dir():
        stop("compiled H2 engine target output must be an application bundle")
    return bundle


def engine_executable(bundle: Path, expected: dict[str, str]) -> Path:
    short_name = expected["applicationShortName"]
    path = bundle / "Contents" / "MacOS" / short_name
    try:
        executable = path.resolve(strict=True)
    except FileNotFoundError:
        stop(f"compiled H2 engine is missing from exact bundle path {path}")
    if not executable.is_file():
        stop("compiled H2 engine bundle executable must be a regular file")
    return executable


def normalized_member_path(member: tarfile.TarInfo) -> PurePosixPath:
    path = PurePosixPath(member.name)
    if path.is_absolute() or ".." in path.parts:
        stop(f"H2 package contains an unsafe archive path: {member.name}")
    return PurePosixPath(*[part for part in path.parts if part not in ("", ".")])


@contextlib.contextmanager
def packaged_engine_bundle(archive: Path, expected: dict[str, str]) -> Iterator[Path]:
    try:
        archive = archive.resolve(strict=True)
    except FileNotFoundError:
        stop(f"current-run H2 package does not exist: {archive}")
    if not archive.is_file():
        stop("current-run H2 package must be a regular file")

    short_name = expected["applicationShortName"]
    expected_app = f"{short_name}.app"
    expected_executable = PurePosixPath(expected_app, "Contents", "MacOS", short_name)
    forbidden_apps = {"OpenCloud.app", "OpenCloud Beta.app"}

    with tempfile.TemporaryDirectory(prefix="atum-drive-h2-package-") as temporary:
        extract_root = Path(temporary).resolve()
        try:
            package = tarfile.open(archive, mode="r:xz")
        except (tarfile.TarError, OSError) as error:
            stop(f"current-run H2 package is not a readable tar.xz archive: {error}")
        with package:
            members = package.getmembers()
            normalized = [(member, normalized_member_path(member)) for member in members]
            root_apps = {path.parts[0] for _, path in normalized if path.parts and path.parts[0].endswith(".app")}
            if root_apps != {expected_app}:
                stop(f"H2 package root application set is {sorted(root_apps)}, expected only {expected_app}")
            if any(forbidden_apps.intersection(path.parts) for _, path in normalized):
                stop("H2 package contains an old generic OpenCloud application bundle")
            executable_members = [member for member, path in normalized if path == expected_executable]
            if len(executable_members) != 1 or not executable_members[0].isfile():
                stop(f"H2 package must contain one regular executable at {expected_executable}")
            try:
                package.extractall(extract_root, filter="data")
            except (tarfile.TarError, OSError) as error:
                stop(f"failed to extract current-run H2 package safely: {error}")

        bundle = (extract_root / expected_app).resolve(strict=True)
        if bundle.parent != extract_root or not bundle.is_dir():
            stop("extracted H2 application bundle escaped the package root")
        yield bundle


def verify_packaged_bundle(bundle: Path, executable: Path, expected_architecture: str) -> None:
    architecture = subprocess.run(
        ["/usr/bin/lipo", "-archs", str(executable)],
        check=False,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        text=True,
    )
    if architecture.returncode != 0:
        stop("failed to inspect packaged H2 engine architecture", architecture.stderr)
    if architecture.stdout.strip() != expected_architecture:
        stop(
            f"packaged H2 engine architecture was '{architecture.stdout.strip()}', expected exactly '{expected_architecture}'"
        )

    signature = subprocess.run(
        ["/usr/bin/codesign", "--verify", "--verbose=2", "--strict", "--deep", str(bundle)],
        check=False,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        text=True,
    )
    if signature.returncode != 0:
        stop("packaged H2 application signature verification failed", signature.stderr)


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--profile", required=True, type=Path)
    parser.add_argument("--header", required=True, type=Path)
    source = parser.add_mutually_exclusive_group(required=True)
    source.add_argument("--build-dir", type=Path)
    source.add_argument("--archive", type=Path)
    parser.add_argument("--expected-architecture")
    args = parser.parse_args()
    header = args.header.resolve(strict=True)
    profile_file = args.profile.resolve(strict=True)
    if not header.is_file() or not profile_file.is_file():
        stop("H2 proof inputs require regular header/profile files")
    if args.archive is not None and not args.expected_architecture:
        stop("package proof requires --expected-architecture")

    contract = json.loads(profile_file.read_text(encoding="utf-8"))
    expected = contract["profiles"]["h2"]
    configured = macro_values(header)
    for field in ("origin", "issuer"):
        if configured[field] != expected[field]:
            stop(f"compiled H2 profile header mismatch for {field}")
    if configured["oauthClientId"] != contract["oauthClientId"]:
        stop("compiled H2 profile header mismatch for oauthClientId")

    with contextlib.ExitStack() as stack:
        if args.archive is not None:
            bundle = stack.enter_context(packaged_engine_bundle(args.archive, expected))
            artifact = "package"
        else:
            build_dir = args.build_dir.resolve(strict=True)
            if not build_dir.is_dir():
                stop("H2 build proof requires a build directory")
            bundle = engine_bundle(build_dir, expected)
            artifact = "build"
        executable = engine_executable(bundle, expected)
        if args.archive is not None:
            verify_packaged_bundle(bundle, executable, args.expected_architecture)

        contents = executable.parent.parent
        with (contents / "Info.plist").open("rb") as handle:
            info = plistlib.load(handle)
        if info.get("CFBundleIdentifier") != expected["bundleIdentifier"]:
            stop("compiled H2 bundle identifier mismatch")
        if info.get("CFBundleExecutable") != expected["applicationShortName"]:
            stop("compiled H2 bundle executable mismatch")

        with tempfile.TemporaryDirectory(prefix="atum-drive-h2-hello-") as private:
            environment = dict(os.environ)
            environment.update(
                {
                    # CoreFoundation/QStandardPaths on macOS ignores XDG alone.
                    "CFFIXED_USER_HOME": private,
                    "QT_QPA_PLATFORM": "offscreen",
                    "XDG_CONFIG_HOME": private,
                    "XDG_DATA_HOME": private,
                }
            )
            process = subprocess.Popen(
                [str(executable), "--atum-engine"],
                env=environment,
                stdin=subprocess.PIPE,
                stdout=subprocess.PIPE,
                stderr=subprocess.PIPE,
                text=True,
            )
            try:
                stdout, stderr = process.communicate(input='{"kind":"stop"}\n', timeout=60)
            except subprocess.TimeoutExpired:
                process.kill()
                stdout, stderr = process.communicate(timeout=10)
                stop("compiled H2 engine did not complete hello/stop within 60 seconds", stderr)
            if process.returncode != 0:
                stop(f"compiled H2 engine stopped with status {process.returncode}", stderr)

    lines = [line for line in stdout.splitlines() if line.strip()]
    if len(lines) != 1:
        stop("compiled H2 engine did not emit exactly one hello frame", stderr)
    try:
        hello = json.loads(lines[0])
    except json.JSONDecodeError as error:
        stop(f"compiled H2 engine emitted invalid JSON: {error}", stderr)
    expected_hello = {
        "issuer": expected["issuer"],
        "kind": "hello",
        "origin": expected["origin"],
        "protocol": 1,
        "taskFiles": True,
    }
    for key, value in expected_hello.items():
        if hello.get(key) != value:
            stop(f"compiled H2 engine hello mismatch for {key}", stderr)
    features = hello.get("features")
    if not isinstance(features, list) or set(features) != FEATURES or len(features) != len(FEATURES):
        stop("compiled H2 engine offered the wrong feature set", stderr)

    print(
        json.dumps(
            {
                "artifact": artifact,
                "authority": "not_evaluated_live_receipt_required",
                "bundleIdentifier": info["CFBundleIdentifier"],
                "clientId": configured["oauthClientId"],
                "features": sorted(features),
                "h2Source": contract["h2Source"],
                "issuer": hello["issuer"],
                "origin": hello["origin"],
                "protocol": hello["protocol"],
                "result": "pass",
            },
            sort_keys=True,
        )
    )


if __name__ == "__main__":
    main()
