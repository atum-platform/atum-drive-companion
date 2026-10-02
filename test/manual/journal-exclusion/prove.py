#!/usr/bin/env python3
# SPDX-License-Identifier: CC0-1.0
"""Real unchanged SyncJournalDb exclusion/crash proof; only generated test data."""
import argparse
import json
import os
from pathlib import Path
import selectors
import subprocess
import tempfile

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("probe", type=Path)
args = parser.parse_args()
binary = args.probe.resolve(strict=True)
os.umask(0o077)
with tempfile.TemporaryDirectory(prefix="companion-journal-exclusion-") as temporary:
    root = Path(temporary) / "root"
    root.mkdir()
    alias = Path(temporary) / "alias"
    alias.symlink_to(root, target_is_directory=True)
    edit = root / "local-dirty.txt"
    edit.write_text("synthetic dirty edit retained\n")
    with (Path(temporary) / "runtime.log").open("w") as log:
        for path in (root, alias):
            owner = subprocess.Popen([str(binary), "hold", str(root)], stdin=subprocess.PIPE,
                                     stdout=subprocess.PIPE, stderr=log, text=True)
            contender = None
            try:
                with selectors.DefaultSelector() as ready:
                    ready.register(owner.stdout, selectors.EVENT_READ)
                    assert ready.select(timeout=10), "owner_start_timeout"
                assert owner.stdout.readline().strip() == "journal_open=1" and owner.poll() is None
                contender = subprocess.Popen([str(binary), "probe", str(path)],
                                             stdout=subprocess.PIPE, stderr=log, text=True)
                with selectors.DefaultSelector() as ready:
                    ready.register(contender.stdout, selectors.EVENT_READ)
                    assert not ready.select(timeout=3), "second_journal_owner"
                assert owner.poll() is None and contender.poll() is None
                owner.kill()  # Only our generated proof process; exercise crash release.
                owner.wait(timeout=5)
                output, _ = contender.communicate(timeout=25)
                assert contender.returncode == 0 and output.strip() == "journal_open=1", "crash_recovery_failed"
                assert edit.read_text() == "synthetic dirty edit retained\n"
            finally:
                for process in (owner, contender):
                    if process is not None and process.poll() is None:
                        process.kill()
                        process.wait(timeout=5)
        print(json.dumps({"schema": "atum.drive.journal-upstream-exclusion/1",
                          "samePathAndSymlinkContendersBlockedWhileOwnerAlive": True,
                          "oneContenderPerCrashRecovery": True,
                          "simultaneousContenderCrashRecoveryProved": False,
                          "crashedOwnerReleasedLock": True, "localEditRetained": True,
                          "realSyncJournalDb": True, "fullDesktopAcceptance": False,
                          "onlyGeneratedTestRoot": True, "containsSecrets": False}))
