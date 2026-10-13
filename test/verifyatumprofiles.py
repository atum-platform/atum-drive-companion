#!/usr/bin/env python3
"""Pin the three immutable Atum build identities and keep them disjoint."""

from __future__ import annotations

import json
import re
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
CONTRACT = ROOT / "test/fixtures/atum-drive-profiles.json"
OEM = ROOT / "oem/atum/OEM.cmake"
BLOCK = re.compile(
    r'(?:if|elseif)\(ATUM_DRIVE_PROFILE STREQUAL "([^"]+)"\)\n(.*?)(?=elseif\(ATUM_DRIVE_PROFILE|else\(\))',
    re.DOTALL,
)
SETTING = re.compile(r'set\(([A-Z0-9_]+)\s+"([^"]+)"\)')
FIELDS = {
    "ATUM_DRIVE_ORIGIN": "origin",
    "ATUM_MAS_ISSUER": "issuer",
    "APPLICATION_SHORTNAME": "applicationShortName",
    "APPLICATION_NAME": "applicationName",
    "APPLICATION_EXECUTABLE": "applicationExecutable",
    "APPLICATION_REV_DOMAIN": "bundleIdentifier",
}


def main() -> None:
    contract = json.loads(CONTRACT.read_text(encoding="utf-8"))
    source = OEM.read_text(encoding="utf-8")
    actual: dict[str, dict[str, str]] = {}
    for name, body in BLOCK.findall(source):
        values = dict(SETTING.findall(body))
        actual[name] = {field: values[variable] for variable, field in FIELDS.items()}

    expected = contract["profiles"]
    if actual != expected:
        raise SystemExit("Atum compiled profile contract does not match OEM.cmake")
    if set(re.findall(r"set_property\(CACHE ATUM_DRIVE_PROFILE PROPERTY STRINGS ([^)]+)\)", source)) != {
        "production s2 h2"
    }:
        raise SystemExit("Atum profile allowlist is not exactly production/s2/h2")
    client = re.search(r'set\(ATUM_OAUTH_CLIENT_ID "([^"]+)"\)', source)
    if client is None or client.group(1) != contract["oauthClientId"]:
        raise SystemExit("Atum profile OAuth client does not match the pinned contract")
    for field in ("applicationShortName", "applicationExecutable", "bundleIdentifier"):
        values = [profile[field] for profile in expected.values()]
        if len(set(values)) != len(values):
            raise SystemExit(f"Atum compiled profiles share {field}")

    print(
        "ATUM_DRIVE_PROFILES_OK",
        json.dumps(
            {
                "client": contract["oauthClientId"],
                "h2Source": contract["h2Source"],
                "profiles": sorted(actual),
            },
            sort_keys=True,
        ),
    )


if __name__ == "__main__":
    main()
