# Root checkpoint review disposition

Primary review job `87e5adba-48ff-4e85-b218-528d1fe22538` returned DO NOT SHIP.
Its full result is retained in `2026-10-02-atum-root-source-review.md`, read,
acknowledged and its route closed. One targeted follow-up is reserved after
local assembly and verification; disagreement alone does not trigger a fallback.

## Confirmed findings and assembled fixes

- Duplicate roots: wizard and addFolder now check equal/parent/child paths,
  including canonical aliases, before persisting a second definition. Candidate
  preflight compares the exact marker schema, version, origin, issuer, subject,
  canonical path, journal and known space. Before Graph resolves the space,
  preflight checks its recorded nonempty space; actual enrollment compares the
  authoritative personal space under the owner lock.
- Resume requires exactly one enabled personal space whose live DAV root matches
  the saved binding. Graph updates abort active sync and visibly pause ambiguous
  or absent spaces. They can resume when the same unique personal space returns.
- Orphaned Atum definitions are retained inertly across settings saves with a
  visible warning. Missing account/plugin never guesses identity, opens a journal,
  or reaches the upstream qFatal branch. Root-busy, missing and foreign cases
  retain ordinary Folder SetupError definitions.
- Unix marker creation fsyncs the held directory after QSaveFile commit.
- Discovery and each propagation item synchronously validate the bound root.
  This closes the concrete interval in which a replacement empty mount could
  otherwise produce remote deletions before the five-second health timer runs.
  In-flight operations cannot be undone; this is not an atomic filesystem/network
  transaction. The timer still aborts active work and preserves journal state.
- The network constructor audit found no raw QNetworkAccessManager allocation
  bypass in the shipped desktop flow. The separate command client's pre-existing
  --trust handler could still call ignoreSslErrors, and the legacy command client
  has neither the pinned OAuth flow nor bound-root enrollment. The pinned profile
  refuses that unsupported sync path before credentials are attached. Unbranded
  command-client behavior is retained.

## Claims resolved against local code

- Sign-out does not remove Folder definitions. AccountState marks SignedOut and
  FolderMan only deschedules/terminates; settings remain for reauthentication.
  Explicit account/folder removal deliberately detaches its settings. The Atum
  wipe path preserves files, partial downloads, marker and journal, allowing
  deliberate same-identity re-enrollment. Retaining an active deleted-account
  definition would contradict removal and could resume unexpectedly. Tests must
  prove sign-out persistence and explicit-removal data preservation separately.
- Initial enrollment is a Qt::SingleShotConnection, not an add on every ready
  event. SpacesManager emits its first ready only after HTTP 200 Graph discovery;
  an offline request does not reach the exactly-one warning. A guard for an
  existing same-account space nevertheless avoids redundant bootstrap attempts.
- Missing/offline/busy roots do not cause the upstream missing-account/plugin
  qFatal. They are Folder SetupError. The distinct orphan-settings crash window is
  real and has been softened without discarding the definition.

## Verified native checkpoint

New AtumRootFolder tests compile against an actual immutable S2 OEM profile.
Folder, SQLite journal, settings, VFS-off, physical owner and removal code are
real; credentials and remote HTTP responses are synthetic. Authentication and
native custody remain separate real suites. An unbranded skip is explicit
and never counted as Atum lifecycle evidence. Existing real owner tests also
exercise full candidate-marker refusal. A SyncEngine regression removes a local
file, invalidates root validation after discovery and asserts zero remote
mutations plus preserved remote file/journal row.

Final native build completed at 2026-10-02T14:23Z. RootBinding (17) and the
compiled S2 RootFolder suite (8) were rerun against the final candidate preflight
and physical-root checks at 14:24Z. Focused SyncEngine (6), FolderMan (5), OAuth
(52), exclusions (16) and real Cocoa/QtKeychain custody (16) also passed: 120
Qt cases including fixture setup/cleanup, zero failures/errors/skips. The
focused engine invocation selects boundRootLossAfterDiscoveryDoesNotDeleteRemoteFile,
testRemoteDelete, testFileDownload and testFileUpload. Receipts are retained in
2026-10-02-atum-root-followup-native.json. A pinned-profile legacy command invocation
with synthetic placeholders exited 1 before attaching credentials; the proposed
test root stayed empty. This is source/native proof, not browser or release proof.

The locally built isolated S2 app launch was rejected by automatic approval
review because computer-use policy requires action-time confirmation for this
temporary source build. Owner approval is pending; no launch took place.

Apple iconutil cannot access its converter service inside the build sandbox:
the same local iconset succeeds outside it. Native builds use that observed
requirement. No icon pixels, production source, signing settings or action
allowlists are changed as a workaround.

Live wizard/root/removal acceptance, portable dependency closure, signed release,
nonempty inventory approval UX, lifecycle status, release-pin reconciliation and
full G8 remain unaccepted. Public source publication still awaits authorization;
the earlier identity-only payload does not cover this root implementation.
