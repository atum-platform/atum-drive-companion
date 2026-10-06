# Engine facts checkpoint review

Primary job `c53dc349-c861-4655-b968-860a002c61c2`, OpenCode `opencode-go/muse-spark-1.3-contributor`, route `77d18906-ba83-487a-b128-2d1e0ef8bdb2`. Reviewed source checkpoint `b91dfdb366d8315ec2592f754a240dde73d7dae7` against preparation `07d7cbbb4f029f9c75ef1ec4c29999a79389f8f6`. Completed result read, terminal delivery acknowledged; one primary review, no Claude job.

The review is source advice for a WIP checkpoint, not S4-8 acceptance. Subsequent local/CI evidence confirmed two issues that it did not identify: strict Qt ASCII-conversion flags rejected plain-character string operations, and a blocked stdout callback allowed an immediate progress burst. Those require the explicit QLatin1Char conversion fix, matching standalone compiler flags, and measuring the interval after output completion, with a backpressure regression test. Neither the earlier review nor the earlier failed CI is a green receipt for the corrected revision.

The review's exclusion-count note exposed narrow coverage: the existing reserved-item signal did not include ordinary ignore-list items or silent exclusions. The source repair combines SyncResult's completed Excluded/FileIgnored count with unique silent/reserved signal observations; a real SyncEngine temporary-tree proof still awaits full CTest. A targeted retained-note follow-up is owed after verification. Full exclusion export also needs a representation decision for the upstream control-character glob. The compiled adapters, pinned CTest and live approved S2 loopback remain acceptance gates; no partial PR or release is approved here.

## Retained primary result

## Verdict: SHIP — WIP source checkpoint, no confirmed blocker

`b91dfdb36` feature-branch checkpoint only. No PR/release. Held items remain held and are not claimed: pinned Craft6.11.1 full-engine compile, live S2 realm proof, S4-8 outcome, complete exclusions list, trash S4-9. Route `77d18906-ba83-487a-b128-2d1e0ef8bdb2` OpenCode/Muse only.

## Confirmed blockers: 0

Traced real APIs/signal order. No correctness/security regression requiring HOLD:

* **Transport/UTF-8/byte cap:** `src/gui/atumengineprotocol.cpp:50-81` per-line `lineCap=16384` incl LF, `_pending+size+LF` check, consume-as-append, `endsWith('\r')` CRLF reject, `QStringDecoder::Utf8.hasError` reject, `!isObject` reject, sticky `_failed`. `src/gui/atumengine.cpp:328-329,514-517` fail+stop on `!valid && !stopping`. `encode:83-87` `size<=16384`.
* **Legacy start/cleanup:** `atumengine.cpp:365-372` `started` once, `acceptStart` only for `!cleanup`; absent `features` → empty accept → legacy run, `accepted()` false gates facts. `cleanup` bypasses negotiation, `forgetting=true`, preserves `checkCandidate/root/configDir/issuer/subject` `379-383`, `restore/accounts/root` fences `391-411`. `file_metadata:349-364` requires `started`, `Malformed→fail+stop`, legacy `Unavailable/Ready` preserved.
* **Hello/handshake:** `atumengine.cpp:530-532` single `hello protocol:1 origin/issuer/taskFiles:true/features sorted`; `atumengineprotocol.cpp:94-112` one-time, `<=8`, offered-only, duplicate/unknown fail, absent→none. No second hello/start.
* **Progress pacing/monotonic/terminal:** `atumengineprotocol.cpp:126-148` exact keys, `done<=total`, `N/null` ETA, `up/down/mixed`, monotonic iff `previous.active`. `atumengine.cpp:145-173` high-water `max()` for totals/done, `trustEta+ceil` ETA, direction OR. `atumengineprotocol.cpp:192-253` `>=500ms` emit, terminal prioritized, `beginRun` preserves prior terminal, `cancel` on stop/fail/forget. `atumengine.cpp:81-95,181-190,200-202` `runActive` single active/inactive per `syncStateChange`→`finished`.
* **Lifecycle/race/credential/root:** `atumengine.cpp:97-125` `stopping/forgetting/failed` fences on all facts; `stop` cancels+unloads+quit. `Folder::slotSyncFinished` connected first `src/gui/folder.cpp:124` Direct, engine handler second `atumengine.cpp:195-212` Direct, `SyncEngine::finalize:src/libsync/syncengine.cpp:647-657` asserts GUI thread → finalized `SyncResult` read valid, `success` bit retained `204-205`, `lastSyncAt=max` monotonic, failed preserves. `QPointer<Folder> factsFolder:76`, `folderListChanged→attachFacts:281` + VFS-ready retry `folder.cpp:652`.
* **SyncResult/conflict/excluded:** `atumengine.cpp:203-210` `conflicts=max(new+old, journal.conflictRecordPaths.size())`, `errors=max(numError, errorStrings.size())`, `excluded=excluded.size()` no paths sent. `ProgressDispatcher::excluded←Folder←SyncEngine←DiscoveryPhase` chain verified `folder.cpp:134`, `syncengine.cpp:425-426`, `discovery.cpp:161-162`.
* **Quota no extra IO/timer:** `atumengine.cpp:126-144,278,282,213` reads `factsFolder->space().drive().getQuota()` only on `attachFacts/isConnectedChanged/spaceChanged`; no `Drives` job/`QTimer`. Valid+explicit `is_total/used/state_Set`, `total>0`, safe-int, `remaining=max`, `atumEngineFactValid` state/remaining/`exceeded==used>=total` gate; invalid→omit, not zero.
* **Registration:** `src/gui/CMakeLists.txt:44-45`, `test/CMakeLists.txt:77-78`, `test/atumengine-protocol/CMakeLists.txt:6-8` correct `SOURCEDIR`, `QtCore+Test` standalone, `opencloud_add_test` path correct.

No narrow fix required.

## Non-blocking notes (not blockers)

* Pinned Craft6.11.1 adapter compile + real binary/loopback not done here — held to hosted CI by design.
* `quota is_total_Set/is_used_Set/is_state_Set/getState` plausible OpenAPI optional-field API (cf `folderstatusmodel.cpp:294-298` uses `isValid/getTotal/getUsed`) but uncompiled here — needs pinned-build proof.
* `excluded` counts only `ProgressDispatcher::excluded` (currently `RESERVED` path, `LIST` goes via `IGNORE` items). Always-`0` in practice not proven wrong per frozen `excluded:N` definition; flag for S4-8 live-count clarification, do not change now.
* Short-run coalesce (`atumengineprotocol.cpp:204-207` retains prior terminal, drops invisible run; sync fact still emitted) matches documented intent; live pacing proof held.
* Withheld `trash/exclusions` offer (`progress/sync/quota` only `atumengine.cpp:64`) is documented contract-compliant subset, not a regression.

## Test/acceptance limits

* Session-claimed 164 QtTest rows, 0 fail/skip on QtBase6.11.2 for QtCore helpers only — not re-executed here (read-only). Logic matches canonical pack `d273a1e201425d28b479f2614415575cd1cd695efbb1b6fbc81d04ff2f851ccb` from `743e7e7744`.
* `test/testatumengineprotocol.cpp:38-143` correctly scopes: framing (structural only), start negotiation, sender fact-schema; explicitly excludes receiver `phase/direction/feature_not_accepted`, `trash`, `line_cap/ending` (covered elsewhere). No app receiver semantics/trash claim.
* No live synthetic restore, no full CTest, no signing/tag/production authorized.

## Retained targeted follow-up

Job `2e31a415-1670-4ec6-a7ed-fce3037de5b9`, route `83060352-2592-4f96-8cff-b106672769bf`, OpenCode `opencode-go/muse-spark-1.3-contributor`. Reviewed corrected `e4baeaa26e88543c3f0c4b8fd974a94edaa58589` against `b91dfdb366d8315ec2592f754a240dde73d7dae7`. Completed result read at cursor 0 / event cursor 20988. This is the one permitted targeted follow-up; no second primary review or Claude job. Its source-only verdict does not replace full-build or live acceptance evidence. The non-blocking test limitation is retained: ordinary/silent disjointness is source-traced, while the producer test checks each disposition separately.

## Targeted verdict: SHIP — corrected WIP only, 0 confirmed blockers

Corrected head since `b91dfdb36` only. No PR/merge/signing/production/live-realm. Held remain held: complete exclusions export, approved S2 live proof, pinned full-engine/real binary, engine release.

## 1. Exclusion-count repair — verified, disjoint, no disposition change

* `src/libsync/discovery.cpp:158-164`: `SILENTLY_EXCLUDED→silentlyExcluded`, `RESERVED→excluded`, early `return true` with no `itemDiscovered`.
* `src/libsync/discovery.cpp:166-182,217-219,231-233`: ordinary `LIST→Excluded`, `HIDDEN→Excluded`, others via `IGNORE` item + `itemDiscovered`. `src/libsync/discovery.cpp:1013-1016`: selective-blacklist `IGNORE+FileIgnored`.
* `src/libsync/syncengine.cpp:424-427`: new `silentlyExcluded→SyncEngine::excluded` plus existing `excluded→excluded`; both also to tracker. No `traversalPatternMatch`/`handleExcluded`/`processFile` rule change.
* Ordinary path to counter: `slotItemDiscovered:258-289` → `_syncItems` → `slotDiscoveryFinished:469-585` → `OwncloudPropagator::start` → `owncloudpropagator.cpp:356-358` `IGNORE/ERROR→PropagateIgnoreJob` → `owncloudpropagator.h:365-377` preserves `Excluded`, `NoStatus→FileIgnored` → `slotItemCompleted:608-620→itemCompleted`.
* `src/libsync/syncresult.cpp:99-100`: `++_numExcludedItems` iff `Excluded||FileIgnored`, before warning/error branches; does not touch new/removed/updated/conflict/error counts. `Excluded/IGNORE` has `direction None`, `FileIgnored` excluded from `Down` branch `:130`, so no double increment.
* Reset: `syncresult.cpp:33-36` `reset()=*this=SyncResult()`, `syncresult.h:76,113` `qint64 _numExcludedItems=0`; `src/gui/folder.cpp:957` per-run `reset()`, `folder.cpp:1118-1125` same `processCompletedItem`; `src/gui/atumengine.cpp:184-185` per-run `excluded.clear()`.
* Combine: `atumengine.cpp:78` `QSet<QString> excluded` unique, `:219-222` insert via `ProgressDispatcher::excluded`, `:210` `result.numExcludedItems()+excluded.size()`. Signal set and completed-item set are disjoint by construction above, so ordinary never double-counts.

## 2. Real FakeFolder producer — proves upload disposition, not schema-only

`test/testsyncengine.cpp:48-71` `exclusionFactsObserveOrdinaryAndSilentItemsWithoutUpload`: real `FakeFolder+Vfs Off`, `addManualExclude(".env")→LIST`, `addManualExclude("]silent-note")→SILENTLY_EXCLUDED` per `csync_exclude.cpp:660-668,428-431`.

Verifies:

* `numExcludedItems()==1` ordinary only; `excluded.contains("silent-note")`, `!contains("kept-note")`
* `!currentRemoteState.find(".env")`, `!find("silent-note")`, `find("kept-note")` — excluded stay off remote, kept uploads
* `reset→0`

Limit: does not assert `!excluded.contains(".env")` nor fact sum `1+1=2`; disjointness is source-traced, not test-proven. `FileIgnored`/selective path counted but not exercised. Test has NOT run in full CTest locally — not a receipt.

## 3. Backpressure + strict-ASCII — verified

* `src/gui/atumengineprotocol.cpp:240-244`: `_emit` then `_sinceEmit.start()` with comment. Closes 0ms burst where pre-fix start-before-write left `elapsed>=500` after 600ms block. `flush:225-246` still terminal-first, `schedule:214-223` `>=500` else timer, `beginRun:192-196` preserves `_terminal`, `cancel:249-255` clears.
* `test/testatumengineprotocol.cpp:174-192` `blockedOutputCannotProduceImmediateBurst` complements `:145-173` pacing/terminal test; asserts `writes[1]-writes[0]>=500` after 600ms block.
* `atumengineprotocol.cpp:36-46`: all `'/','\\',':'`, `replace/split` now `QLatin1Char`. `test/atumengine-protocol/CMakeLists.txt:3,9-13` `C++20` matches root `CMakeLists.txt:6`, strict defs are superset of `OCApplyCommonSettings.cmake:18-28` (`QT_NO_CAST_*`, `QT_STRICT_ITERATORS` etc). Conservative for ASCII; `QT_MESSAGELOGCONTEXT` delta irrelevant. No other changed file introduces plain-char conversion.

## Acceptance limits

* Source run on corrected head pending; b91 full CI fail is not green for corrected rev. Claimed strict local `165` QtCore pass not re-executed here.
* New SyncEngine temp-tree test unrun; canonical hash unchanged claimed but not re-verified here.
* Complete `exclusions` representation decision, pinned Craft6.11.1 compile, live approved S2 loopback, S4-8/9, release held.

No narrow fix required.
