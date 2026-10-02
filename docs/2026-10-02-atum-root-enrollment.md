# Atum root enrollment continuation

This branch follows the reviewed identity/OEM checkpoint e16c00f (source 4ee0ead).
The identity branch remains separately reviewable and is waiting for explicit
public-source publication approval. No new root implementation is included in
that pending approval.

## Parallel MECE workstreams

1. Coordinator: root/space/binding/journal preservation in the GPL companion.
   Inventory, binding and ownership are ordered dependencies; test integration
   follows assembled code and is not parallel implementation.
2. Existing custody CI branch: only hosted test environment repair and validation;
   no identity/root source changes. Linux Secret Service preflight and all four hosted platform checks passed at
   ef42a30 in run 37007478257. Custody PR #1 merged as bdf69050c993791f7c8c321a4862bb806535cf8c at 13:16:22Z.
3. Platform acceptance branch: redacted live S2 byte/expiry evidence, already
   committed/pushed at 8e7853be7. It does not publish this companion source.

Native exploration decision 6dba8ae1-0b81-4da8-9312-d484440b2d7a enforced direct
work because Codex quota was exhausted; it completed and feedback closed. Root
design decision 79a4f2f6-d01d-47de-8f84-be4f32927bf2 routes OpenCode because the
architecture provider's quota was exhausted. Its bounded consultation receives
public upstream/fork source and generic root requirements, without credentials.
The result is advice and must be read before implementation decisions. The
coordinator owns one assembled source review after real I/O verification.

## Confirmed boundaries and next implementation

Upstream FolderMan::addFolder calls ensureJournalGone before constructing a
Folder; removeFolder calls wipeForRemoval, which deletes journal state. Initial
sync iterates all returned spaces and uses a suffix-search folder allocator.
These are not safe Atum resume, account change or personal-root semantics.

The first root slice will use exactly one personal space and the explicitly
enrolled root. It will refuse foreign/ambiguous bindings without wiping or
renaming, preserve an unavailable root as a visible error, and keep credential,
journal, runtime/database and purchased skill/source material excluded. Empty
fresh enrollment and matching-binding resume precede explicit nonempty inventory
adoption. Unknown nonempty roots must remain refused until that approval UX is
implemented; they must never be silently called accepted.

The real exclusive-journal proof already shows an owner prevents same-path and
symlink-alias access, but two simultaneous recovery waiters can remain mutually
busy after an owner crash. A canonical nonblocking owner guard before journal
opening/mutation is warranted for the Atum profile; its lifetime must extend
through shutdown and journal close. It must not become a generic policy layer.

Implemented state matrix: fresh empty -> explicit enrollment; matching binding -> resume;
foreign/corrupt/unknown nonempty -> refuse and preserve; owner busy -> visible
pause; missing/unreadable root -> unavailable, no deletion inference; OS credential
cleanup pending -> no sync; new identity -> distinct binding. Code is assembled; final native regression receipts and primary source review follow below. Lifecycle status/interface, explicit inventory
approval, release-pin reconciliation, packaging/signing, two-Mac proof and G8
remain open.

## Assembled implementation

The pinned theme enrolls the explicitly chosen directory directly into exactly
one enabled personal space. The wizard never suffixes `~/Atum`, exposes no
manual/VFS bypass, and refuses unknown nonempty inventory. Verified non-secret
issuer/subject facts are cached separately from native credentials; they cannot
authorize remote access. Existing settings without those facts require fresh
reauthentication. The binding records origin, issuer, subject, personal-space
identifier, canonical root, fixed journal basename and version using QSaveFile.

New and restored Folder instances acquire the same canonical owner before any
SQLite open/read/mutation. Matching resume never calls ensureJournalGone. Errors
retain the Folder definition and show SetupError. Removal/sign-out preserve
user files, partial downloads, binding and dirty journal; credential cleanup
continues through the reviewed native-store fence. Atum stays on full local
files with no VFS migration or dehydrated-file wipe. Mandatory excludes remain
after reload and clearing manual excludes, independent of hidden-file settings.
They cover binding/owner/journal sidecars, known credentials, managed runtimes,
databases and protected managed skill/source directories. This does not classify
arbitrarily renamed plaintext secrets or unmarked purchased source; those must
remain outside an enrolled root. Nonempty adoption needs explicit inventory UX.

The first real QLockFile test passed exact/symlink/same-process ownership and
elected one of two simultaneous crash-recovery contenders. Source inspection
then exposed a cleanup hazard: Qt 6.11.1 unlock removes its pathname without
checking whether that pathname was replaced. [Qt's implementation](https://raw.githubusercontent.com/qt/qtbase/v6.11.1/src/corelib/io/qlockfile_unix.cpp)
confirms this behavior. The final guard uses an excluded persistent owner file,
nonblocking Unix flock / Windows LockFileEx and retained directory/file handles.
Close releases only the held inode's lock; no owner file is unlinked. Physical
root and lock identity are revalidated, so an empty replacement mount cannot
be treated as the original root. A five-second check aborts a running sync and
pauses visibly on root/identity/journal change; each new sync checks immediately.
Recovery requires restoring the original root and restarting, with files and
journal preserved. One real test replaces the live root, starts a new owner,
then destroys the old owner and proves the replacement lock still excludes a
third contender. Windows holds a directory handle that refuses live rename;
that Unix replacement test is explicitly skipped there.

Pinned AccessManager requests accept only HTTPS on the compiled Drive or MAS
origins. A foreign request is replaced by Qt's asynchronous invalid-URL error
before headers, cookies, body or a socket can escape. The compiled profile
outranks the unbranded test descriptor, redirect policy is always manual,
peer verification uses the default trust configuration, and custom certificate
exceptions/dialogs are disabled. The cached DAV root must have the compiled
Drive origin and must equal the live personal Graph root before sync.

This is a source checkpoint for empty enrollment and matching resume, not a
signed distribution, nonempty adoption, lifecycle interface or G8 completion.
The previously built e16c00f development bundle's staging audit found absolute
SDK dependency references and an invalid ad-hoc nested signature; it is not
portable/release-ready. The frozen 4.0.0 launch pin versus 4.0.1 fork baseline
remains unresolved. Public identity-source publication remains pending exact
payload approval; this new root checkpoint is not part of that earlier payload.

## Focused verification

Final native arm64 SDK build passed. Qt xUnit receipts on 2026-10-02:

| Suite | Cases | Failures/errors/skips |
|---|---:|---|
| Atum root binding / ownership / real socket | 16 | 0/0/0 |
| Existing FolderMan | 5 | 0/0/0 |
| Existing exclusions | 16 | 0/0/0 |
| OAuth and pinned resolver | 52 | 0/0/0 |
| Real Cocoa/QtKeychain custody | 16 | 0/0/0 |

105 total, including actual journal rows/file retention, exact-path and symlink
locks, same-process duplicates, two simultaneous crash recovery contenders,
root replacement without old-owner unlink, real excluded files with hidden
settings flipped/reload, and foreign request header/body rejection before a
local socket. The report is retained as native-verification.json. No mocked
SQLite/lock/keychain result is counted. Folder wiring is compiled and upstream
regressions run; the new pinned wizard/root/removal path still requires live
S2 exercise. Source review remains pending at this checkpoint.
