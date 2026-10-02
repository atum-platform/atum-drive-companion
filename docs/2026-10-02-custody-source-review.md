# Companion custody source review — 2026-10-02

Job: `7ea130ac-c788-4f9a-be74-87c80b4f555a`; decision: `af7db308-5c8e-4674-b95f-c91cc238b9fe`.
Owner: `cloud-launch-finish-01a0fad5:companion-custody-assembly:Drive companion custody assembly review`.
Reviewed checkpoint: `d01ca08`; source baseline: `9bf87d5ea35b847a40dd7370fcfe104313150da4`.
Read complete retained result before delivery acknowledgment. Cursor 0, event cursor 18684. No targeted follow-up required by the SHIP verdict; coordinator verifies the nonblocking ordering correction below. This verdict covers custody source only.

**Verdict: SHIP — custody source only.**

No ship-blocking correctness / resurrection / disclosure / data-loss defect in this checkpoint. Remaining signing / OEM / lifecycle / two-Mac work is **separately unclaimed**, not a custody defect.

### Prioritized findings — introduced vs pre-existing

**P2 — Low: `signOutByUi` emits `SignedOut` before durable marker — `src/gui/accountstate.cpp:308-317`**
```cpp
_queueGuard.block(); setState(SignedOut); // 310-311
account()->credentials()->forgetSensitiveData(); // 312 marks pending+syncs+issues Delete
```
Consultation required `sync()` logout + `pending-delete` **before** `setState/wantsAccountSaved`. Window is synchronous — no UI `signIn()` can interleave — but crash between first `setState` and `markPending().sync()` leaves no pending + no `wantsAccountSaved`. Restart then resumes `Disconnected` with old OS entry intact, not `SignedOut+pending`. Fail-safe direction (still logged-in, no leak), not exploitable. **Action:** drop first `setState`, keep post-`forget` `setState` + `wantsAccountSaved`. Introduced ordering deviation, not blocker.

**P3 — Pre-existing: `AccountManager::deleteAccount` fire-and-forget orphan — `src/gui/accountmanager.cpp:208-230`**
`forgetSensitiveData()+clear()` issued, then `erase+deleteLater+save()` without waiting for delete-ack. If OS delete fails, `CredentialOperations/<host:uuid>` pending is orphaned (uuid gone). OS entry leaks but **cannot be resurrected** — binding includes `uuid` (`src/libsync/creds/credentialmanager.cpp:30-34,224-230`). Upstream same fire-and-forget. **Action:** tombstone/retry sweep follow-up. Not blocker.

**P3 — Pre-existing: single-key `clear("oauth/...")` no-op — `src/libsync/creds/oauth.cpp:480,485` + `src/libsync/creds/credentialmanager.cpp:210-222`**
`knownKeys(group)` filters `k.startsWith(group+'/')`. `clear("oauth/dynamicRegistration")` / `clear("oauth/id_token")` match 0 keys, leave stale dynamic data. Old `beginGroup+allKeys` same. Non-rotating, non-secret, out of DC-2 scope. `HttpCredentials::invalidateToken:251` now uses `clear()` (all) so logout path is complete. **Action:** follow-up single-key delete. Not blocker.

**P4 — Info / pre-existing: binding cache is in-memory — `src/libsync/creds/credentialmanager.cpp:224-238`**
`_binding` cached once fixes in-session URL-change resurrection (verified `testDeletionFailureRetainsFenceAndRetriesSameBinding:261-264` — retry `key()==binding` after `setUrl(changed)`). Crash + URL-change + restart with new host computes new binding, orphans old OS entry (leak, not resurrection). Pre-existing recompute had same. Not blocker.

**P4 — Info: Force-sync QML gates on `Connected` only — `src/gui/qml/FolderDelegate.qml:364,372` vs `src/gui/accountstate.cpp:591-593`**
`readyForSync` correctly requires `ready() && !cleanupPending`, `Account::jobQueue` held via `authenticationStarted/fetched`, `AccountState::_queueGuard` blocked on `requestLogout/cleanupPending`. Manual Force/Pause buttons check only `state==Connected`, so `WritePending` with state still `Connected` can enqueue a bearer-less attempt (401 ignored via `HttpCredentials::slotAuthentication:157-158` `if(!_ready) return`). No leak, just failed sync. UX gate follow-up.

### Verification — all six custody gates hold

* **Queued edits/journal retained:** `httpcredentials.cpp:193-206,209-225,284-313` use `requestLogout` (block, not `authenticationFailed/clear`); `account.cpp:205` `authenticationFailed->clear` not hit; `accountstate.cpp:182-185,310` block only. Test `testWriteFailureStaysUnreadyAndRetainsFiles:190-221` asserts `currentLocalState` + file bytes unchanged, no `synthetic-refresh` in `operations/credentialsList`.
* **Bearer/readiness waits for save:** `httpcredentials.cpp:55` `ready() && !empty`, `275-313` `_ready=false` until `writeFinished(success)`, `fetched/credentialsStored` withheld; `httpcredentialsgui.cpp:112-114` same for `asyncAuthResult`; `HttpCredentialsGui(token,refresh):47-52` starts `_ready=false`. Test `testReadyWaitsForSecureWrite:157-188` asserts no `Authorization` + `!ready` + `fetched==0` before `emitFinished`, `Bearer synthetic-access` after.
* **Rotation marker durable before consumption:** `httpcredentials.cpp:179-187,227` `beginRotation()->sync()` before `refreshAuthentication(oldRT)`; `credentialmanager.cpp:249-252,241-247` `sync()` + `status` check. `refreshError` retains fence, clears memory tokens, `requestLogout`, no replay; second `refreshAccessToken` blocked via `hasPendingOperation`. Tests `testLostRefreshReply:959-1025` (`tokenRequestCount==1`, `logout==1`, `authFailed==0`, `!refreshAccessToken`) + `testRejectedRefresh:1027-1088` + `testRotationFenceSurvivesRestart:223-241` (restart denies `get`, `read->_job==null`).
* **Logout fences reads/reconnect/late writes:** `credentialmanager.cpp:204-208,327-333,349-353` `contains/get` deny pending; `accountstate.cpp:321-323,361` `signIn/checkConnectivity` return on `cleanupPending`; `httpcredentialsgui.cpp:57` `restartOauth` returns on pending; `credentialmanager.cpp:94-105,155-172` generation fence + `_deleteAgain`/re-`remove`. Tests `testLateWriteAfterLogout:272-293` + `testWriteTimeoutFences:295-323` (both orderings) + `testDeleteTimeout:325-348` + `testDeletionFailure:243-270`.
* **Pending cannot be misreported completed:** `hasPendingDeletion/deletionFailed:267-285` until `NoError|EntryNotFound` + `QSettings::NoError`; `contains` false; `accountsettings.cpp:125-128,368-374` + `FolderDelegate.qml:82-85` pending/failed label, button disabled unless failed, `retryCredentialCleanup:342-347` bounded user retry. Real-backend tests `testCorruptRead:350-378` + `testSecureBackend:380-403` assert `!insecureFallback`, `writeFinished` ack, `knownKeys empty` after delete. No production `setInsecureFallback`; seam `credentialmanager.h:67-69` friend-only.
* **Wizard waits + cancellation outlives disposal:** `setupwizardaccountbuilder.cpp:91-92` no `persist`; `setupwizardcontroller.cpp:172-192` `build→persist→credentialsStored(true)=>finished`, `false=>CredentialsState`; `93-96,214-219` `forgetSensitiveData` on navigate/destructor; `credentialmanager.cpp:79-82,144-146` `write/delete` hold `account->sharedFromThis()`. No orphan `Connected` account.

Timeout `30s` `credentialmanager.cpp:24` matches bounded `QTRY/wait(30000)`; Apple Cocoa dispatcher note does not relax production timeout.

### Scoped acceptance gaps — not custody defects

Per task instruction, not counted against this checkpoint: signing/Developer-ID, Atum OEM config/root ownership, lifecycle interface, full process-crash kill, packaged wizard/account-switch, two-Mac acceptance, official Desktop 4.0.0 S2 proof (separate platform PR). Test scope is account/core credentials as disclosed in `docs/2026-10-02-custody-release.md`.


