#!/usr/bin/env python3
import hashlib
import json
import os
import sys
from pathlib import Path


bundle = Path(sys.argv[1]).resolve()
output_path = Path(sys.argv[2])
digest = hashlib.sha256()
records = []


def walk(folder: Path) -> None:
    for name in sorted(os.listdir(folder)):
        path = folder / name
        relative = path.relative_to(bundle).as_posix()
        if path.is_symlink():
            record = [relative, "link", os.readlink(path)]
        elif path.is_dir():
            walk(path)
            continue
        else:
            file_digest = hashlib.sha256()
            with path.open("rb") as handle:
                for chunk in iter(lambda: handle.read(1024 * 1024), b""):
                    file_digest.update(chunk)
            record = [relative, "file", file_digest.hexdigest()]
        line = json.dumps(record, separators=(",", ":"), ensure_ascii=False)
        records.append(line)
        digest.update((line + "\n").encode())


walk(bundle)
output_path.write_text("\n".join(records) + "\n")
print(digest.hexdigest())
