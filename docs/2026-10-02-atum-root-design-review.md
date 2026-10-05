# Retained root design advice

Job 740d6c5b-2e0b-4428-bd38-888338aaf8e9; decision
79a4f2f6-d01d-47de-8f84-be4f32927bf2; OpenCode Go/Muse Spark.
Complete retained response was read. This is design advice from supplied public
source and requirements, not source approval or executed acceptance.

Coordinator dispositions: the lock must be fixed for the whole canonical root,
independent of a requested journal basename. Keying it by journal name could let
two fresh contenders choose different journals before any marker exists.
QLockFile's same-process behavior will be proved with two distinct real objects;
an additional in-process registry is not assumed necessary. Hold ownership
through journal shutdown. Cache only verified non-secret issuer/subject metadata,
keep native credentials internal, and revalidate identity before admitting sync.
Unknown nonempty adoption remains refused until explicit inventory approval UX
exists. No frozen release pin is changed by this work.

## Complete retained advice

## Retained canonical-root design

Keep single personal `Drive` root only:

* `Atum` profile filters `SpacesManager::spaces()` to exactly one personal, non-disabled space. `0` or `>1` fails closed. Reuse `space->disabled()` filter in `src/gui/application.cpp:setUpInitialSyncFolder`.
* Use explicitly selected root directly. Delete suffix-duplicate path (`findGoodPathForNewSyncFolder`) and `all spaces` loop in that function.
* Binding tuple: `canonical issuer + sub + spaceId + canonical root + exact journal basename + marker version`. `AccountUUID` is `QUuid::createUuid()` in `src/gui/newwizard/setupwizardaccountbuilder.cpp:SetupWizardAccountBuilder::build` - not identity, do not bind on it.
* Persist non-secret enrolled `sub` only after already-verified `OAuth::persist(account,dynamicRegistration,idToken)` in `src/libsync/creds/oauth.cpp:OAuth::persist`. Read `idTokenC` JSON from native `credentialManager`, never extract `access/refresh/token`. Later OEM `UserInfo` check remains source of truth; slice must not invent second token path.
* Versioned atomic root marker in canonical root, self-excluded from sync. `QLockFile` non-blocking owner guard acquired before any journal open/mutation, held until journal close.
* Fresh enroll: only empty safe root. Matching marker resumes without `ensureJournalGone`. Foreign/corrupt/incomplete, unknown non-empty, unknown journal, home/managed, duplicate folder: refuse with no mutation.
* Sign-out / `accountRemoved`: pause + preserve files/journal/enrollment/definition. Never `wipeForRemoval`, never `ensureJournalGone` on match.
* Always exclude secrets/managed paths independent of `ignoreHiddenFiles`. Status only opaque `identity/root/dirty/conflict/auth/cleanup`, tokens stay in native store.
* Gate new lock only to branded `Atum` build. No behavior change for generic unbranded builds.

## Smallest correct ordering - blocking

Enforce in this order, all under canonical path:

1. `OAuth::persist` done -> cache enrolled `sub`. No binding before.
2. Explicit root select -> `mkpath` if missing (`src/gui/folderman.cpp:FolderMan::prepareFolder`) -> `FileSystem::canonicalPath` / `QFileInfo::canonicalFilePath` as in `src/gui/folder.cpp:Folder::checkLocalPath`. Reject home, home-parent, managed runtime/db, skill/source material, non-dir, non-readable/writable, `Folder::checkPathLength`.
3. Acquire `QLockFile::tryLock(0)` on sibling lock derived from canonical root + exact journal basename e.g. `<canonicalRoot>/<journalBase>-owner.lock`. Non-blocking, no wait.
4. Under lock: re-read marker + inventory + `numberOfSyncJournals` (`src/gui/folderman.cpp:numberOfSyncJournals`), `checkPathValidityRecursive` / `checkPathForSyncRootMarkingRecursive`, duplicate-folder check in `checkPathValidityForNewFolder`.
5. If no marker and empty + no journal: atomic marker write (`QSaveFile` + rename, `fsync` dir where feasible) then open journal.
6. If marker matches enrolled tuple + journal name matches: skip `ensureJournalGone`, open existing journal.
7. Else: release lock, refuse, preserve. No `ensureJournalGone`, no marker overwrite, no `Folder` construction.
8. Only then `FolderMan::addFolderInternal` / `Folder::checkLocalPath:_journal.open()` / `Folder::startVfs`. Release lock only after `_journal.close()` in `Folder` teardown / `unloadAndDeleteAllFolders` / `Application::slotCleanup` order.

Integration points, minimal:

* Replace `src/gui/application.cpp:setUpInitialSyncFolder` loop with single-space filter + explicit root.
* Branch `src/gui/folderman.cpp:FolderMan::addFolder` around `ensureJournalGone`: canonical path never calls it on match; refuses before it otherwise.
* Harden `src/gui/folderman.cpp:FolderMan::loadFolders` + `addFolderInternal`: pre-check marker/binding/lock before `new Folder`. On transient missing (`spaces not ready`, offline, lock busy) keep definition and create `Folder` in `SetupError` / paused, do not skip + do not `saveFolders` deletion. Current `qFatal` on missing account/plugin and `continue` on `dbIsTooNewForClient` will otherwise lose preserved binding - must soften for canonical root.
* Neutralize `src/gui/folderman.cpp:FolderMan::removeFolder:wipeForRemoval` + `slotRemoveFoldersForAccount` + `slotIsConnectedChanged` cascade for canonical binding: `unload` + `slotUnregisterPath` + `journal.close`, leave files/journal/marker/definition.
* Install always-excludes before `src/gui/folder.cpp:Folder::startVfs:startSync`: marker basename, lock basename, `journalPath + -wal/-shm/-journal/-ctmp`, credential/runtime/db/paid paths via `loadDefaultExcludes/reloadExcludes/isFileExcludedAbsolute`.

## Concrete pitfalls

Ordering: `Folder::Folder` calls `checkLocalPath()->_journal.open()` synchronously then `QTimer::singleShot(0,startVfs)`. Any check after construction is too late. `loadFolders` bypasses `addFolder` and calls `addFolderInternal` directly. Guard must wrap both entry paths.

Crash/owner: baseline `SyncJournalDb` exclusive ownership holds while owner lives, but two waiters in SQLite recovery can stay mutually busy after owner crash. `QLockFile::tryLock` before open fixes election liveness only: losers never enter SQLite wait, fail fast to `SetupError`/retry. It does not replace journal internal locking.

Identity: `displayName`, `davUrl`, local `accountUUID`, `defaultSyncRoot` marking in `SetupWizardAccountBuilder::build` are mutable/local. Bind only verified `iss/sub` + `spaceId` + canonical root/journal. Fresh `UserInfo` verification (later OEM profile) must precede native-store ack; slice must not ack on cached token alone.

Inventory: `checkLocalPath` and `checkPathValidityRecursive` do not check emptiness. Without pre-flight scan, non-empty unmarked root would be adopted and uploaded. Scan must exclude only expected sidecars, run under lock, and treat any other entry as `nonempty-refuse` until explicit preview/approval path exists.

Exclusion: `setIgnoreHiddenFiles`, `setVirtualFilesEnabled:wipeDehydratedVirtualFiles`, `startVfs:_vfs->fileStatusChanged(*-wal/*-shm,Excluded)` show excludes are stateful. Marker/lock/journal excludes must be unconditional, re-applied on `reloadSyncOptions/reloadExcludes`, tested via `isFileExcludedAbsolute/Relative` with `ignoreHiddenFiles` flipped both ways. Otherwise marker uploads or appears as conflict.

Startup preservation: `Folder::hasSetupError==SetupError` keeps folder in `_folders` but `unloadFolder` skips disconnects; `saveFolders` persists definition. Reuse that for unavailable root: represent as `SetupError`, do not `unloadAndDelete`, do not `wipeForRemoval`. Retry when `SpacesManager::ready` fires.

## QLockFile: solves / does not solve

Solves: multi-waiter mutual-busy after `SIGKILL`/crash. Owner crash -> kernel releases SQLite handle but waiters stuck in recovery; narrow file guard ensures only holder ever calls `_journal.open()/wipeErrorBlacklist/slotDiscardDownloadProgress/setSelectiveSyncList`. Second contender gets `false` immediately, pauses, preserves.

Does not solve if misused:

* Alias divergence: lock keyed by uncanonical path or config-dir path lets same root via symlink have two owners. Must canonicalize first, lock sibling to journal.
* Early release: releasing after `checkLocalPath` but before `startVfs/scheduler/startSync` reopens race. Hold for `Folder` lifetime.
* Same-process duplicates: two `FolderDefinitions` same canonical root need in-process singleton holder in addition to file lock.
* Stale PID reuse / update handoff: `QLockFile` stale detection clears after old PID gone; new instance must treat `tryLock==false` as `paused-retry`, not spin. Explicit unlock in `slotCleanup/unloadAndDeleteAllFolders` before exec/replace for fast handoff.
* Generic builds: do not add.

## Metadata preservation

* `FolderDefinition::save/load` stores `localPath/journalPath/spaceId/davUrl/displayName`. It does not store enrolled `sub/iss/marker version`. Keep enrollment in app config + marker file, not sync root content, not logs. Never log tokens; status opaque.
* Do not overwrite marker on resume. Do not call `ensureJournalGone` (deletes with `Retry/Abort` dialog in `src/gui/folderman.cpp:FolderMan::ensureJournalGone`) on match. Do not delete `-wal/-shm` except via normal close.
* `wipeForRemoval` deletes `stateDbFile + .ctmp/-shm/-wal/-journal + .OpenCloudSync.log + Desktop.ini`. For canonical root it must be disabled; test must assert journal + user files intact after `removeFolder`/sign-out simulation.

## Nonempty adoption: explicit

* Permit: empty root + no marker/journal -> enroll + write marker.
* Permit: matching marker + matching journal (+ now non-empty from prior sync) -> resume.
* Refuse no-mutation: unknown non-empty with no/foreign marker; unknown journal (`numberOfSyncJournals>0`) with no matching marker; foreign/corrupt/incomplete marker; home/managed root; duplicate folder (`folderForPath` parent/child/equal).
* Defer to later: inventory preview/approval UI, selective-sync whitelist import, `UseVfs/ConfigureUsingFolderWizard` flows.

## Feasible state matrix

`Enroll x Marker x Root x Journal x Lock -> Action`:

* none/valid-match/empty/absent/free -> enroll+marker+open+sync
* enrolled-match/valid-match/nonempty-from-sync/present/free -> open+sync, no wipe
* enrolled-match/missing/unavailable-offline/present/free -> `SetupError` paused, preserve, retry on `SpacesManager::ready`/reconnect
* enrolled-foreign or corrupt/incomplete/any/any -> refuse, no Folder, no journal/marker mutation, surfaced `SetupError`
* any/valid-foreign/nonempty-unknown/present-or-absent -> refuse no-mutation
* any/absent/nonempty-unknown/absent -> refuse no-mutation (needs approval path)
* any/any/home-or-managed/any -> refuse
* any/valid-match/any/present/busy -> paused `SetupError`, no SQLite touch, bounded retry
* sign-out/account-removed with match -> paused preserve, no `removeFolder` wipe

## Bounded real I/O verification

Use real `QTemporaryDir`, real symlinks, real `SyncJournalDb` files, real `QLockFile`; no mocks for ownership:

* Alias convergence: `root` + `symlink->root`, same journal basename; contender via alias fails `tryLock` or marker canonical mismatch, no mutation.
* Identical resume: enroll empty -> write file+sync-close -> reopen same `iss/sub/space/root/journal` -> journal row count + file preserved, `ensureJournalGone` not called.
* Foreign refusal: `sub2/space2/path2/journal2` vs enrolled marker -> refuse, assert `mtime/size` of marker/journal/files unchanged, no new `Folders` entry.
* Unknown nonempty/journal refusal: pre-seed files or `.sync_*.db` with no marker -> refuse, no upload, no delete.
* Home/managed refused + no duplicate: `homeDir`, managed dir, nested/parent of existing `Folder::path()` via `checkPathValidityForNewFolder`.
* Crash/owner: `A` holds lock+open; `B,C` `tryLock` fail fast; `kill -9 A`; `B` then `C` serialize: one wins, other stays paused; after winner closes, loser can acquire. Assert no mutual busy wait, no journal deletion.
* Removal preservation: simulated `removeFolder`/sign-out on match leaves user files + `*.db*` + marker intact.
* Exclusion: flip `ignoreHiddenFiles true/false`, assert `isFileExcludedAbsolute(marker/lock/journal/credential/managed)` stays true.

