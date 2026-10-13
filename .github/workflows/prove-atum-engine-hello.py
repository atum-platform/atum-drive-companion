#!/usr/bin/env python3
"""Exercise the compiled H2 engine through its real stdio entry point.

This stops after the immutable hello. It deliberately performs no OAuth, file,
realm, or keychain operation and therefore does not replace live acceptance.
"""

from __future__ import annotations

import argparse
import json
import os
import plistlib
import re
import subprocess
import tempfile
from pathlib import Path
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


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--profile", required=True, type=Path)
    parser.add_argument("--header", required=True, type=Path)
    parser.add_argument("executable", type=Path)
    args = parser.parse_args()
    executable = args.executable.resolve(strict=True)
    header = args.header.resolve(strict=True)
    profile_file = args.profile.resolve(strict=True)
    if not executable.is_file() or not header.is_file() or not profile_file.is_file():
        stop("H2 proof inputs must be regular files")

    contract = json.loads(profile_file.read_text(encoding="utf-8"))
    expected = contract["profiles"]["h2"]
    configured = macro_values(header)
    for field in ("origin", "issuer"):
        if configured[field] != expected[field]:
            stop(f"compiled H2 profile header mismatch for {field}")
    if configured["oauthClientId"] != contract["oauthClientId"]:
        stop("compiled H2 profile header mismatch for oauthClientId")

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
