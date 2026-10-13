# Drive engine 1.1 canonical data

Copied byte for byte from the merged Atum app contract. `source.json` records the full merged source commit, canonical pack digest, original paths and local filenames. `vendored.json` is the unmodified source pin. The document's relative links retain their original app context.

Run from the engine repository root:

```sh
python3 test/fixtures/atum-drive-engine-1.1/verify.py
```

This checks copied bytes, the aggregate pin and basic test-envelope integrity. It does **not** exercise an engine receiver, advertise features, or claim runtime conformance. S4-8/S4-9 consume the decoded `wire` bytes with the actual implementations and the documented initialized context. Never send the JSONL envelope itself as a frame.

The current 1.0 engine remains unchanged. Before the complete S4-8 outcome PR, implement negotiated facts, CTest conformance and sender-rate tests, then retain the required live synthetic-realm proof. No release/tag or production operation is part of this preparation.
