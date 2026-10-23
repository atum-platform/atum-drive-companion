#!/usr/bin/env python3
import argparse
import json
import os
import subprocess
from pathlib import Path


parser = argparse.ArgumentParser()
parser.add_argument("--executable", required=True, type=Path)
parser.add_argument("--evidence", required=True, type=Path)
parser.add_argument("--issuer", required=True)
parser.add_argument("--origin", required=True)
args = parser.parse_args()

executable = args.executable.resolve()
evidence = args.evidence.resolve()
home = evidence / "fixture-home"
temporary = evidence / "fixture-tmp"
home.mkdir()
temporary.mkdir()
environment = os.environ.copy()
environment.update(
    {"HOME": str(home), "TMPDIR": str(temporary), "QT_QPA_PLATFORM": "minimal"}
)
process = subprocess.run(
    [str(executable), "--atum-engine"],
    input=b'{"kind":"stop"}\n',
    stdout=subprocess.PIPE,
    stderr=subprocess.PIPE,
    timeout=10,
    env=environment,
    check=False,
)
(evidence / "hello-stop.stdout.jsonl").write_bytes(process.stdout)
(evidence / "hello-stop.stderr.log").write_bytes(process.stderr)
messages = [json.loads(line) for line in process.stdout.splitlines() if line]
if process.returncode != 0:
    raise SystemExit("engine exited {}".format(process.returncode))
if len(messages) != 1 or messages[0].get("kind") != "hello":
    raise SystemExit("unexpected protocol output: {!r}".format(messages))
hello = messages[0]
if hello.get("issuer") != args.issuer or hello.get("origin") != args.origin:
    raise SystemExit("unexpected production identity: {!r}".format(hello))
if b"authorize" in process.stdout.lower() or b"keychain" in process.stderr.lower():
    raise SystemExit("fixture attempted authentication or Keychain access")
print(
    json.dumps(
        {"result": "pass", "exitCode": process.returncode, "hello": hello},
        sort_keys=True,
    )
)
