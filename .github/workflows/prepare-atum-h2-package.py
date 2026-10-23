#!/usr/bin/env python3
"""Add the pinned OpenCloud blueprint's narrow H2 packaging option.

The upstream recipe knows only the generic OpenCloud application names.  This
temporary CI patch is deliberately revision- and content-locked so a future
blueprint change fails closed instead of receiving a best-effort rewrite.
"""

from __future__ import annotations

import argparse
import hashlib
import json
import os
import stat
import subprocess
import tempfile
from pathlib import Path
from typing import NoReturn


BLUEPRINT_REVISION = "4b5f539521b7a68d51417faffe5278694a68996f"
BLUEPRINT_SHA256 = "d01ae171b66f401b7c757fe2d06d9961037e7c93cb83c19bada9812d316006c6"
BLUEPRINT_PATH = Path("opencloud/opencloud-desktop/opencloud-desktop.py")

OPTION_ANCHOR = '        self.options.dynamic.registerOption("buildBeta", False)\n'
OPTION_REPLACEMENT = OPTION_ANCHOR + '        self.options.dynamic.registerOption("atumH2Package", False)\n'
EXECUTABLE_ANCHOR = '''    @property
    def applicationExecutable(self):
        return "opencloud_beta" if self.subinfo.options.dynamic.buildBeta else "opencloud"
'''
EXECUTABLE_REPLACEMENT = '''    @property
    def applicationExecutable(self):
        if self.subinfo.options.dynamic.atumH2Package:
            return "AtumDriveH2"
        return "opencloud_beta" if self.subinfo.options.dynamic.buildBeta else "opencloud"
'''
APP_NAME_ANCHOR = (
    '        self.defines["appname"] = "OpenCloud" if not self.subinfo.options.dynamic.buildBeta else "OpenCloud Beta"\n'
)
APP_NAME_REPLACEMENT = '''        self.defines["appname"] = (
            "AtumDriveH2"
            if self.subinfo.options.dynamic.atumH2Package
            else ("OpenCloud" if not self.subinfo.options.dynamic.buildBeta else "OpenCloud Beta")
        )
'''


def stop(message: str) -> NoReturn:
    raise SystemExit(message)


def git(repository: Path, *arguments: str) -> str:
    process = subprocess.run(
        ["git", "-C", str(repository), *arguments],
        check=False,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        text=True,
    )
    if process.returncode != 0:
        stop(f"failed to inspect pinned OpenCloud blueprint checkout: {process.stderr.strip()}")
    return process.stdout.strip()


def replace_once(source: str, anchor: str, replacement: str, label: str) -> str:
    if source.count(anchor) != 1:
        stop(f"pinned OpenCloud blueprint no longer has the exact {label} anchor")
    return source.replace(anchor, replacement, 1)


def patched_source(source: str) -> str:
    source = replace_once(source, OPTION_ANCHOR, OPTION_REPLACEMENT, "dynamic option")
    source = replace_once(source, EXECUTABLE_ANCHOR, EXECUTABLE_REPLACEMENT, "application executable")
    source = replace_once(source, APP_NAME_ANCHOR, APP_NAME_REPLACEMENT, "application name")
    return source


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--repository", required=True, type=Path)
    args = parser.parse_args()

    repository = args.repository.resolve(strict=True)
    if not repository.is_dir():
        stop("OpenCloud blueprint repository must be a directory")
    revision = git(repository, "rev-parse", "HEAD")
    if revision != BLUEPRINT_REVISION:
        stop(f"OpenCloud blueprint revision is {revision}, expected {BLUEPRINT_REVISION}")

    blueprint = repository / BLUEPRINT_PATH
    if git(repository, "status", "--porcelain", "--untracked-files=no", "--", str(BLUEPRINT_PATH)):
        stop("pinned OpenCloud desktop blueprint already has local changes")
    source_mode = stat.S_IMODE(blueprint.stat().st_mode)
    source_bytes = blueprint.read_bytes()
    source_hash = hashlib.sha256(source_bytes).hexdigest()
    if source_hash != BLUEPRINT_SHA256:
        stop(f"OpenCloud desktop blueprint hash is {source_hash}, expected {BLUEPRINT_SHA256}")

    source = source_bytes.decode("utf-8")
    patched = patched_source(source)
    compile(patched, str(blueprint), "exec")
    patched_bytes = patched.encode("utf-8")

    with tempfile.NamedTemporaryFile(dir=blueprint.parent, delete=False) as handle:
        temporary = Path(handle.name)
        handle.write(patched_bytes)
    try:
        os.chmod(temporary, source_mode)
        os.replace(temporary, blueprint)
    finally:
        temporary.unlink(missing_ok=True)

    print(
        json.dumps(
            {
                "blueprint": str(BLUEPRINT_PATH),
                "inputRevision": revision,
                "inputSha256": source_hash,
                "outputSha256": hashlib.sha256(patched_bytes).hexdigest(),
                "result": "patched_for_h2_package_only",
            },
            sort_keys=True,
        )
    )


if __name__ == "__main__":
    main()
