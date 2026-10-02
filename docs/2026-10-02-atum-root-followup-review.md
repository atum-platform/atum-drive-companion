# Retained targeted source follow-up

Job: 4f5749a0-6394-46fb-87bd-bc0f6fe68fd0

Candidate: 2e225d91baf03ee6e9110c8305dbb8aafec82669

## Targeted follow-up — narrow source checkpoint

Read-only. Diff + surrounding code inspected. No delegation/mutation.

### Primary P0 dispositions

**P0-1 live Folder proof — RESOLVED (introduced fix verified)**
- `test/testatumrootfolder.cpp:146-318` real `FolderMan::addFolder`, real `Folder(checkLocalPath→acquire→journal.open→startVfs)`, real SQLite rows, real owner `flock`, real settings, real VFS-Off, real `removeFolder`/`signOutByUi`. Graph/creds synthetic only, explicitly disclosed (`:18-19`, `initTestCase:124-130` QSKIP unbranded).
- `test/testsyncengine.cpp:48-70` invalidates `_localRootValid` in `aboutToPropagate`, asserts zero PUT/POST/DELETE/MOVE, remote preserved + journal row valid. Correct post-discovery ordering proof.

**P0-2 duplicate bypass — RESOLVED (introduced)**
- Wizard `src/gui/newwizard/states/accountconfiguredsetupwizardstate.cpp:66-72` calls `checkPathValidityForNewFolder(SpacesFolder)` before `checkCandidate`. `src/gui/folderman.cpp:390-402` canonicalizes, forces `journalName+Off`, then duplicate + full-identity `checkCandidate` before `addFolderInternal`/`saveFolders`.
- `checkPathValidityForNewFolder:642-674` covers equal/child/parent via canonical paths; `safeRoot:59,71-82` converges aliases + rejects Atum-parent overlap. Tested `duplicateAliasParentAndChildNeverPersist:186-206` incl. symlink alias.

**P0-4 re-add/noisy transient — RESOLVED (introduced)**
- `src/gui/application.cpp:77-127` is `Qt::SingleShotConnection`. `src/libsync/graphapi/spacesmanager.cpp:63,80-83` emits `ready` only after HTTP 200; offline emits `updated` only, no warning. Guard `application.cpp:98-103` skips duplicate same `accountState+space`.

**P0-5 exactly-one on resume — RESOLVED (introduced)**
- `src/gui/folder.cpp:456-476` `atumSpaceMatches()` counts enabled `personal==1` + own space enabled/personal + saved DAV == live DAV. `canSync` requires it. `folder.cpp:107-121` `updated` watcher aborts + `Paused`, re-`Queued` when unique returns without touching `_atumRootInvalid`. Tested `secondPersonalSpaceStopsResume:237-257`.

**P0-6 missing account/plugin qFatal — RESOLVED for Atum (introduced; pre-existing unbranded qFatal retained)**
- `src/gui/folderman.cpp:200-206` Atum `!vfs||account.isNull()` → `_unavailableFolders`, warn, `continue`, never `qFatal`. `:232-251` preserves across `saveFolders` with visible warning `:218-225`. Busy/foreign/missing stay `SetupError` via `Folder::checkLocalPath:237-252`.

**P0-3 removal cascade — RESOLVED as reinterpreted per instruction (do not preserve deleted-account attachment)**
- Verified: `src/gui/accountstate.cpp:308-316` `signOutByUi` only blocks/sets `SignedOut`, never deletes definitions. `src/gui/folderman.cpp:301-324` deschedules/terminates only.
- Explicit removal correctly forgets attachment: `folderman.cpp:469-492` `removeFolder` erases + `saveFolders`; `folder.cpp:884-892` Atum `wipeForRemoval` early-returns after unregister/journal-close/vfs-stop, preserves files/partials/marker/journal. `accountmanager.cpp:211-233` `deleteAccount` → `accountRemoved` → removal. Test `:161-183` proves sign-out preserves definition/journal row vs removal `savedCount 0` + data preserved + same-identity re-enroll restores row. Retaining an active deleted-account definition would auto-resume unexpectedly — correctly not done.

**Full identity + missing-journal refusal — RESOLVED (introduced)**
- `src/gui/atumrootbinding.cpp:137-163` requires origin/issuer/subject, exact `expectedBinding(root,identity)` equality (version/origin/issuer/subject/space/canonicalRoot/journal `:115-119`), `journalLinksSafe`, plus `!journal+nonempty → paused`. `acquire:227-268` same + under-lock recheck. `checkCandidate` empty-space fallback `:149-152` only for pre-Graph wizard preflight; enrollment/resume supply authoritative space.

**Durable rename — RESOLVED (introduced)**
- `:269-278` `QSaveFile::commit` + Unix `fsync(dir)`. Failure after commit safely retries as resume. Windows exclusion expected.

**Synchronous checks — RESOLVED (introduced)**
- Before discovery `src/libsync/syncengine.cpp:313-317`, after discovery before commit/propagator `:536-542`, per-item `src/libsync/owncloudpropagator.cpp:107-118`, wired `folder.cpp:292-294` to `canSync()`. Setup vs readiness correct: options installed at `Vfs::started:646`, `startSync:943,951` gates `isReady+canSync`.

**Legacy command refusal — RESOLVED (introduced)**
- `src/cmd/cmd.cpp:240-243` exits before `TokenCredentials` attach and before `--trust/ignoreSslErrors`. Unbranded behavior retained — correct.

### Non-blocking advisories (not SHIP-blocking)

- Orphan overlap: `checkPathValidityForNewFolder` scans `_folders` only, not `_unavailableFolders`. Same-path re-enroll while orphan persists yields two settings entries (orphan inert, no journal open — no data loss). Fix: include unavailable in duplicate scan or drop orphan on same-canonical re-enroll.
- Per-item `_localRootValid→canSync→spacesManager` can run off GUI thread (`scheduleSelfOrChild:114-118`). File/identity part is safe; `spaces()` pointer read races GUI `deleteLater`. Recommend file-only check on worker, keep count on GUI watcher.
- `folderman.cpp:411` creates VFS from original mode, not forced `Off`. Product Atum path passes Off so benign; use forced value.
- CI Atum step `main.yml:328-337` Linux-PR only; Windows `LockFileEx`/rename semantics and macOS Atum have no hosted proof. `with-test-keyring.sh:9-38` + `IPC_LOCK:82-85` + preflight `:131-135` correct; hosted Atum run still pending publication.

### Verdict

**SHIP this narrow source checkpoint.** All primary P0s resolved with real-Folder evidence; no blocking correctness/security/data-loss/lifecycle regression found.

Does **not** close: live wizard/root/removal browser acceptance, nonempty-inventory UX, portable closure, signed release, two-Mac/Windows Atum, full G8/72h, public-source publication auth, hosted Atum-profile run, action-time app launch.
