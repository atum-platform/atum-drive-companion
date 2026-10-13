# H2 engine proof preparation — 2026-10-13

Drive engine PR #4 has green exact-head source checks, but its Apple Silicon CI
artifact was the generic OpenCloud Beta profile. Executing that artifact's real
`--atum-engine` entry point produced the expected closed result
`{"kind":"state","state":"unavailable"}`: it cannot establish the Atum
profile's feature negotiation or connect to the H2 acceptance realm.

This checkpoint adds the missing acceptance-only build identity. `h2` fixes the
Drive and issuer origins to the four-name H2 realm, has a distinct bundle and
keychain identity, and cannot be selected at runtime. Production and the retired
S2 profile are unchanged. The Apple Silicon pull-request lane builds that H2
profile, executes the compiled binary, validates the exact identity plus the
offered `progress`, `sync`, `quota`, and `exclusions` features, stops it before
OAuth or filesystem access, and retains the unsigned bundle in the normal CI
artifact.

Local verification passed the immutable 1.1 fixture pack (37 accept / 78 reject,
digest `d273a1e201425d28b479f2614415575cd1cd695efbb1b6fbc81d04ff2f851ccb`),
Python compilation, workflow YAML parsing, whitespace checks, the three-profile
contract checker, and a fake-process round trip through the proof harness while
draining 5,000 synthetic stderr lines. The same harness rejects PR #4's generic
Apple Silicon artifact at the bundle-identity boundary. Hosted execution on the
final head is still required.

More importantly, this is only the real-binary hello receipt. PR #4's merge gate
still requires an approved, unexpired synthetic-realm run proving:

- the app accepts `exclusions` and gets exactly one authoritative frame after
  the bound folder becomes ready;
- declining that feature emits no exclusions frame and retains the app's pinned
  fallback; and
- a real ignored-file sync remains usable and reports the bounded warning.

The existing H2 endpoints still answer, but their recorded authority expired on
2026-10-08. This checkpoint does not extend or reuse that authority, start or
modify the host, publish/tag/sign an engine, pin it into Atum, or touch
production. S-C/A-HOST must supply a renewed reviewed realm receipt before the
live proof; a dependency blocks the row rather than deleting it.

## Assembly review and remediation

Enforced route `190f8051-05b2-483f-8a47-501b204f0014` selected OpenCode/Muse;
job `be495002-59a3-4b92-b51a-075d60990b6a` returned **DO NOT SHIP** on the first
draft. The complete retained result was read, its delivery acknowledged and
routing feedback closed. Its verified concerns are addressed before any push:

- the proof now uses one bounded `communicate` call so stdout and stderr drain
  concurrently; timeout, exit, malformed output and diagnostic tails are
  bounded, and each build/proof/package step has its own workflow timeout;
- the real binary runs through `craft --run`, not a raw host environment;
- the normal Apple Silicon build, tests and portable package remain generic;
  H2 is reconfigured and built only afterwards, mirroring the existing Linux
  Atum-profile order;
- `atum-drive-profiles.json` pins production, S2 and H2 identities plus the
  authoritative platform source commit/path and shared public client; a checker
  requires exact production/S2 preservation and distinct short name,
  executable/config and bundle/keychain identity;
- the compiled H2 header, bundle plist, hello identity, shared client and offered
  features are checked against that independent fixture; the receipt explicitly
  reports that live authority was not evaluated; and
- CoreFoundation as well as XDG state is redirected to the disposable proof
  directory before the hello-only process starts.

The requested hosted H2 hello and packaged-artifact evidence necessarily follows
the final push. One targeted follow-up is reserved only if the retained primary
result still requires it after exact-head CI. The primary review's renewed-realm
requirement remains the later live-sync dependency, not something this source
checkpoint fabricates.

## Hosted infrastructure correction

The first current-head CI attempt and its one allowed unchanged rerun both failed
before checkout: the KDE registry returned HTTP 502 while GitHub initialized the
Linux job container, and fail-fast canceled Windows and both macOS jobs. No source
or test step ran. The last successful full PR run (`38245152609`) records that
`craft-appimage-alma9:latest` resolved to
`sha256:41dd698b4c6c4035ca5216077170006642712f59dab27cbb3dd41e814af8448b`.
The workflow now pins those exact previously proven bytes rather than asking the
registry for a mutable tag. This is a reproducibility/supply-chain correction on
the standard GitHub-hosted runner, not a self-hosted fallback or a disguised
unchanged rerun. A fresh commit and complete exact-head run are still required.

The digest endpoint also returned HTTP 502, proving a registry-wide outage. The
matrix now keeps `fail-fast` off so that this one external Linux initialization
failure cannot cancel independent Windows/macOS checks or the required H2 proof.
The workflow and pull request still fail while Linux is red; this does not weaken
the merge gate, reroute Linux, or turn partial evidence into acceptance.
