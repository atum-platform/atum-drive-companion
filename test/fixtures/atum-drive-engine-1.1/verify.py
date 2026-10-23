#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""Verify vendored source bytes and envelopes, not receiver conformance."""

import hashlib
import json
import re
from pathlib import Path


def verify():
    folder = Path(__file__).resolve().parent
    source = json.loads((folder / "source.json").read_bytes())
    raw_pin = (folder / "vendored.json").read_bytes()
    pin = json.loads(raw_pin)
    assert re.fullmatch(r"[0-9a-f]{40}", source["source_commit"])
    assert source["contract"] == pin["contract"] == "atum.drive-engine/1.1"
    assert source["source_repository"] == pin["source_repository"]
    assert hashlib.sha256(raw_pin).hexdigest() == source["vendor_record_sha256"]
    assert source["pack_sha256"] == pin["pinned_sha256"]
    assert set(source["source_files"]) == set(pin["files"])
    assert len(set(source["source_files"].values())) == len(pin["files"])

    entries = []
    ids = set()
    counts = {"accept": 0, "reject": 0}
    for original, digest in sorted(pin["files"].items()):
        local = source["source_files"][original]
        assert local == Path(original).name
        raw = (folder / local).read_bytes()
        assert hashlib.sha256(raw).hexdigest() == digest
        entries.append(json.dumps([original, digest], separators=(",", ":")) + "\n")
        if not local.endswith(".jsonl"):
            continue
        for line in raw.decode("utf-8").splitlines():
            case = json.loads(line)
            assert case["id"] not in ids
            ids.add(case["id"])
            verdict = case["expected"]["verdict"]
            counts[verdict] += 1
            assert (case["expected"]["code"] == "ok") == (verdict == "accept")
            wire = case["wire"].encode("utf-8")
            if "chunks" in case:
                assert min(case["chunks"]) > 0
                assert sum(case["chunks"]) == len(wire)
            if verdict == "accept":
                assert wire.endswith(b"\n") and b"\r" not in wire
                for frame in wire.splitlines():
                    assert len(frame) + 1 <= 16384
                    assert isinstance(json.loads(frame), dict)
    digest = hashlib.sha256("".join(entries).encode("utf-8")).hexdigest()
    assert digest == source["pack_sha256"]
    print("DRIVE_ENGINE_VENDOR_OK", counts, digest)


if __name__ == "__main__":
    verify()
