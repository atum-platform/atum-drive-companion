# Retained root source review

Job `87e5adba-48ff-4e85-b218-528d1fe22538`, decision `50949768-153b-416c-9703-ba13f6dc3123`. Read in full before disposition. Primary result: DO NOT SHIP. Review covered `ec4c4f7` against `e16c00f`.

## Atum root enrollment/ownership + pinned transport — source review ec4c4f7 vs e16c00f

Read-only. No delegation. Based on supplied diff + `src/gui/atumrootbinding.cpp/.h`, `test/testatumrootbinding.cpp`, `docs/2026-10-02-atum-root-*`.

Compiled optional identity outranks unbranded injected descriptor — verified. No credentials/tokens in context. No secret disclosure found.

### What is solid (introduced, correct)

* Owner never unlinks: `src/gui/atumrootbinding.cpp:~204-222` destructor only `close`/`CloseHandle`. No `unlink`. Old-owner shutdown cannot unlink replacement — matches `replacedRootCannotBeMistaken…` test.
* `flock`/`LockFileEx` + retained dir/file handles + `sameFile(dev/ino)` revalidation: `src/gui/atumrootbinding.cpp:~171-202,269-278`. Alias convergence via `canonicalFilePath`, same-process second `acquire` fails, two crash contenders elect one — covered by real `QTemporaryDir`/`SyncJournalDb`/`QProcess --atum-root-owner` tests.
* `acquire` double-check: pre-lock marker/emptiness check, `lockOwner`, then re-read marker + `journalLinksSafe` + empty/missing-journal handling: `src/gui/atumrootbinding.cpp:~224-267`. Correct TOCTOU order. Fresh-empty enroll + matching resume only; foreign/corrupt/unknown-nonempty/unknown-journal/home/managed refuse no-mutation.
* Pinned `AccessManager`: `src/libsync/accessmanager.cpp:~32-42,67-78,125-135`, `src/libsync/accessmanager.h` — `compiled ? compiled : identity`, `sameOrigin=https+host+port(443)+no-userInfo/no-fragment`, foreign → `createRequest(op,QNetworkRequest{},nullptr)` async invalid-URL before headers/cookies/body/socket, `ManualRedirectPolicy`, `defaultConfiguration+VerifyPeer`, ignore `_customTrustedCaCertificates`, `sslErrors` early-return + `filterSslErrors` passthrough when pinned. Test `pinnedTransportRejectsForeignHeaderAndBodyBeforeSocket` asserts `body.pos()==0`, no `Authorization`/`Cookie`, no `trap.hasPendingConnections`.
* `Folder::wipeForRemoval` Atum early-return: `src/gui/folder.cpp:~855-868` — `slotUnregisterPath+journal.close+vfs->stop`, skips `slotDiscardDownloadProgress` + DB/log deletion. Files/partial-downloads/dirty journal preserved. Introduced, correct direction.
* `FolderMan::addFolder` Atum: `src/gui/folderman.cpp:~370-380` canonicalizes, forces `journalName()+Vfs::Off`, skips `ensureJournalGone`. `Folder::checkLocalPath` Atum: `src/gui/folder.cpp:~212-235` checks `account.url==driveOrigin`, `iss`, `webDavUrl` origin/no-userInfo/no-query/no-fragment, `journalPath==journalName`, `Vfs::Off`, `hasDefaultSyncRoot+canonical equality`, then `acquire` before `dbIsTooNew`/`Vfs::prepare`. `loadFolders` skips `dbIsTooNew` continue for Atum: `src/gui/folderman.cpp:~195`, deferring to owned check. Good.
* Excludes survive `clearManualExcludes/reload` + hidden toggle: `src/libsync/csync_exclude.cpp:setAtumRootExclusions/reloadExcludeFiles`, `src/libsync/syncengine.cpp:loadDefaultExcludes`. `FolderDefinition::load` early-return `Off`, `setVirtualFilesEnabled` no-op for Atum. No VFS migration/wipe.
* `OAuth::persist` caches only `iss/sub` via `setAtumIdentity`: `src/libsync/creds/oauth.cpp`, persisted as `atum-issuer/subject` in `src/gui/accountmanager.cpp:~110,147-148`. No token extraction. Missing facts → `acquire` demands reauth. `AccountState::sslErrors` gated: `src/gui/accountstate.cpp`.
* `reloadSyncOptions` null-`_engine` guard + `application.cpp` null-`folder` guard are introduced fixes to pre-existing crashes.

### P0 — must fix, blocks source SHIP

**1. New Folder/wizard/removal path compiled but not live-exercised — cannot verify owned-first journal in product path. [Introduced gap]**
`test/testatumrootbinding.cpp` proves helper + transport + excludes with real I/O, but zero live `Folder`/`FolderMan` Atum cases: no real `Folder(checkLocalPath→_journal.open→startVfs)`, no `loadFolders→addFolderInternal(enrollEmpty=false)` resume with row-count, no `removeFolder/sign-out` via `FolderMan` cascade, no `setUpInitialSyncFolder` single-personal enrollment. Upstream `testfolderman:5` are regressions only. Static shows correct ordering, but `numberOfSyncJournals`, `checkPathValidityRecursive`, `wipeErrorBlacklist`, `setSelectiveSyncList`, `slotCleanup/unloadAndDelete` ordering remain unproven in situ.
*Action:* add real-I/O Folder test (temp root, stub account/space) for empty-enroll→write→close→resume row/file preserved + `ensureJournalGone` never called, foreign/nonempty/busy SetupError preserve, `wipeForRemoval`/sign-out leaves `*.db*+marker+files`.*

**2. Duplicate Folder bypass — wizard + `addFolder` skip `checkPathValidityForNewFolder/folderForPath`. [Introduced]**
`src/gui/newwizard/states/accountconfiguredsetupwizardstate.cpp:~64-71` Atum branch returns after `AtumRootBinding::checkCandidate` (only `iss/sub/canonicalRoot+emptiness`), never calls duplicate/parent-child check. `src/gui/folderman.cpp:addFolder` Atum never checks existing `_folders`. `src/gui/atumrootbinding.cpp:checkCandidate` also omits origin/space/journal/version. Re-selecting same enrolled `~/Atum` passes wizard, creates second `Folder`; second loses `flock` → `SetupError`, but duplicate definition persists via `saveFolders`.
*Action:* enforce `folderForPath` parent/equal/child + full `expectedBinding` (origin/space/journal/version) in both `checkCandidate` and `addFolder` before `addFolderInternal`.*

**3. `removeFolder/slotRemoveFoldersForAccount/slotIsConnectedChanged` still delete definition. [Pre-existing cascade, not neutralized for Atum]**
Only `Folder::wipeForRemoval` internals gated. `FolderMan::removeFolder` still erases definition + `saveFolders`. Design requires `pause+preserve definition` on sign-out/account-removed. Current preserves files/journal/marker (recoverable via re-enroll) but loses auto-resume.
*Action:* gate Atum cascade to `unload+slotUnregisterPath+journal.close`, retain definition as `SetupError/paused`.*

**4. `setUpInitialSyncFolder` Atum re-adds without existing-folder guard, noisy on transient. [Introduced]**
`src/gui/application.cpp:~84-101` filters to `personal`, `!=1 → QMessageBox+return`, else `addFolder(defaultSyncRoot,…)`. No check for existing folder for `spaceId/root`; called on every `SpacesManager::ready` it would duplicate (paused via lock, but definition spam). `spaces==0` on offline also pops “must have exactly one” instead of silently pausing + retrying on `ready`/reconnect.
*Action:* check existing folder first; transient empty → preserve/retry, only persistent `0/>1` personal → visible warning.*

**5. Resume does not enforce exactly-one-personal. [Introduced incompleteness]**
Enroll enforces `spaces.size()==1` personal, but `src/gui/folder.cpp:canSync` only checks own `space()->disabled/driveType/personal+webDavUrl==live root`. If second personal appears later, existing Folder keeps syncing — violates “0 or >1 fails closed”.
*Action:* enforce count in `canSync` (or dedicated spaces-count watcher) → `SetupError` pause.*

**6. `loadFolders` `qFatal` on missing account/plugin unverified. [Pre-existing, partially fixed]**
Only `dbIsTooNew` softened for Atum. If `qFatal` on missing account/plugin remains, Atum unavailable-root (spaces not ready/offline/lock busy) could crash or `continue+saveFolders` deletion instead of `SetupError` preserve. Tool limits prevented full audit.
*Action:* confirm Atum path softens all `qFatal/continue` in `loadFolders+addFolderInternal` to `SetupError` preserve, never `saveFolders` deletion.*

### P1 — fix before live S2 / release (not source-blocking alone)

* `checkCandidate(iss/sub only)` allows foreign origin/space to pass wizard then fail in `Folder` — tighten to full identity as in P0-2.
* `QSaveFile` marker without dir `fsync` — atomic rename OK, crash recovery already handles empty-with-marker vs nonempty-without-journal pause; add `fsync` where feasible.
* `ExcludedFiles: *.db/*.sqlite*/ *token*` intentionally broad — document that legitimate `*.db` in root will not sync (fail-closed, not loss).
* Audit no raw `QNetworkAccessManager` bypasses `AccessManager::createRequest` (OAuth/WebFlow/validator all route via it).
* `verifyAtumRoot` 5s + per-`canSync` `matches()` does `fstat/lstat+readBinding` — correct but I/O heavy; keep, do not relax.
* `AccountManager::deleteAccount` unmark gate `!oauthIdentityProfile` correct because `SetupWizardAccountBuilder::build` skips `markDirectoryAsSyncRoot` for Atum — keep, note migration from generic-marked roots still resumes (same `orgDomainName` allowed).

### Verdict

**DO NOT SHIP this source checkpoint.**

Binding/owner-replacement/no-unlink + pinned-transport no-header/body/redirect/TLS-exception are proven by real tests. But the actual journal open/read/mutation + removal/sign-out + single-personal enrollment in `FolderMan`/`Folder` — the first-slice product path — is compiled-only with duplicate-definition, definition-deletion, re-enroll-guard, spaces-count-on-resume, and `loadFolders` crash-preservation gaps above. Fix P0s + add live Folder open/resume/removal real-I/O proof, then re-review.

Explicitly open (not counted as done): signed release, full nonempty adoption/inventory UX, RT lifecycle interface, release-pin reconciliation, full two-Mac G8, 72h acceptance.


