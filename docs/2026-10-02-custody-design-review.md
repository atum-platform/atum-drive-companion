# Drive custody design consultation

Session: cloud-launch-finish-01a0fad5. Routing decision: 9891cf00-5ac8-40d6-a857-4b0d27b93900. Retained agent-jobs job: 9229dcf2-0f54-4ae3-8a70-d21a443056cf. Provider: OpenCode (Muse Spark), read-only. This is architecture advice, not source review or a release verdict. No credentials or runtime secrets were supplied.

## Minimal credential custody repair — consultation

Scope: `HttpCredentials::refreshFinished`, `HttpCredentialsGui::asyncAuthResult`, `HttpCredentials(accessToken)` + `SetupWizardAccountBuilder::build`, `CredentialManager::set/remove/clear/get`, `AccountState::signOutByUi`. No new backend, no policy manager, no scope/client change.

### 1. Smallest working vertical slice

Keep narrow waist: `Account -> HttpCredentials -> CredentialManager -> QKeychain::Job`. All durability state in two places only:

1. **OS secure store:** only place that ever holds `http/oauthtoken`.
2. **Existing credential-index `QSettings` (`Credentials/<app>:<host>:<uuid>`):** holds **no token bytes**, only:
   * `http/oauthtoken` presence bit (existing),
   * `pending-write/<generation>` marker,
   * `pending-delete/<scopedKey>` marker,
   * `logout-cleanup-pending/<uuid>` marker.

Rules, in order:

* **Consume-then-persist is forbidden.** `refreshFinished` / `asyncAuthResult` must not set `_ready=true` / `_accessToken` / emit `fetched()` before `WritePasswordJob::NoError`. Sequence: `OAuth refresh success (new RT in memory, old RT consumed server-side) -> sync pending-write marker -> start WritePasswordJob -> on NoError: install `_refreshToken/_accessToken`, `_ready=true`, clear marker, emit `fetched()`; on error/timeout/crash: `_ready=false`, no access header, sync blocked`.
* **Enrollment same gate.** `HttpCredentialsGui(token,refreshToken)` starting `ready=true` and `SetupWizardAccountBuilder::build(): setCredentials(); persist();` fire-and-forget is the enrollment variant of the same bug. Wizard must stay in `EnrollmentPending` (not `Connected/ready`) until that first `set()` acks. No sync, no `wantsAccountSaved as connected` before ack.
* **Delete is acknowledged.** `CredentialManager::remove()` must not `credentialsList().remove(key)` before `DeletePasswordJob` result. Keep key as `pending-delete` until `NoError|EntryNotFound`, `sync()` the marker, deduplicate concurrent deletes on same `scopedKey`, deny `get()` on pending entries. `AccountState::signOutByUi()` must `sync()` logout + pending-delete state **before** `setState(SignedOut)` / `wantsAccountSaved`, block `checkConnectivity/reconnect/sync` while pending, surface `pending/failed` status. Failure stays `SignedOut+CleanupFailed`, files + dirty journal retained, no reuse of binding by another account, bounded user-triggered retry.
* **Serialize + fence.** One in-flight `AccountBasedOAuth` per `HttpCredentials`. Add monotonic `generation` incremented on `refresh/start`, `logout`, `enrollment`. All `QKeychain::Job::finished` and `OAuth::result/refreshFinished` callbacks carry generation; stale generations ignored. This reconciles "issued writes may finish": write success with stale generation after logout must not clear `pending-delete` or set `ready`, must trigger re-delete.
* **Fail closed, retain.** Failed/lost write: pause new sync, retain edits/journal on disk, no plaintext fallback, no replay of indexed consumed RT. Recovery only: one bounded re-read/re-refresh gated by MAS tolerance proof, else fresh browser sign-in. Never unlimited replay.

File-level minimum:

* `creds/credentialmanager.{h,cpp}`: pending markers, `sync()` before job start, `get()` denies pending-delete, `set/remove/clear` deduplicate + generation-aware, `knownKeys/clear` excludes `_pending_*`, relax `OC_ASSERT(error!=EntryNotFound)` — `EntryNotFound` is success for delete-idempotent recovery.
* `creds/httpcredentials.{h,cpp}`: `_ready` gated by write success, `generation`, `refreshInFlight`, withhold `fetched()` until durable, `createAM()` attaches `Bearer` only when `ready && !writePending && !cleanupPending`.
* `gui/creds/httpcredentialsgui.cpp`: same gate for `asyncAuthResult`, no early `ready=true`.
* `gui/accountstate.{h,cpp}` + `gui/newwizard/setupwizardaccountbuilder.cpp`: persist `SignedOut + cleanup-pending` synchronously first, `Connecting/Connected` forbidden while pending, enrollment waits for ack.

Preserved: tokens only in OS store, files/journal untouched on failure, `host:uuid` binding immutable for key lifetime, upstream rotation stays serialized, all failures block sync.

### 2. State matrix

| State | `ready()` / access header | Sync / net | `get(RT)` | Recovery |
|---|---|---|---|---|
| `StableReady` (no pending) | `true` / attached | allowed | allowed | — |
| `RefreshInFlight+WritePending` (marker synced, OAuth done, keychain pending) | `false` / stripped | blocked, queue held | denied | wait for ack only |
| `WriteFailedBlocked` (write error / crash with `pending-write`) | `false` / stripped | blocked, edits/journal retained | denied, no replay of consumed RT | bounded one re-read-or-refresh if MAS-proved, else fresh sign-in |
| `EnrollmentPending` (wizard `persist` pending) | `false` / stripped | blocked | denied | wait; on fail stay in wizard, do not create connected account |
| `CleanupPending` (`SignedOut` + `pending-delete` synced) | `false` / stripped | blocked, reconnect denied | denied | wait for `NoError\|EntryNotFound` |
| `DeleteFailedBlocked` | `false` / stripped, visible failure | blocked, files/journal retained, binding not reusable | denied | bounded user retry; issued late writes fenced + re-deleted |

Transitions: `StableReady -> WritePending` on refresh success (marker first); `WritePending -> StableReady` only on `NoError`; `WritePending -> WriteFailedBlocked` on error/timeout/crash-restart; `Any -> CleanupPending` on `forgetSensitiveData/signOutByUi` (marker first); `CleanupPending -> done` only on delete ack; `CleanupPending -> DeleteFailedBlocked` on error; stale write ack in `Cleanup*` ignored + re-delete.

### 3. Required seam tests — real keychain, failure, crash, race

Use real `QKeychain::Job` + `TestCredentialManager`-style harness with error/delay injection at job seam, not mocked `CredentialManager`:

1. **Rotation durability:** full `refreshFinished(newRT)` -> assert `ready()==false`, no `fetched()`, no `Authorization` header until `WritePasswordJob finished NoError`.
2. **Write failure fail-closed:** inject `OtherError` on write -> `ready()==false`, sync blocked, journal/files intact, no plaintext file, bounded retry path exposed.
3. **Crash with `pending-write`:** sync marker, kill before keychain callback, restart -> `get()` denied, no auto `refreshAccessToken()` replay of old RT, state requires fresh sign-in (or single MAS-gated retry).
4. **Successful write then crash before marker clear:** restart must re-read once to reconcile, not blindly force sign-in nor blindly trust index — validates marker-vs-store divergence handling.
5. **Delete ack:** `remove/signOutByUi` retains `pending-delete`, `get()` denied during pending, `SignedOut` not claimed before ack, reconnect refused while pending.
6. **Delete failure:** inject delete error -> `SignedOut+DeleteFailed` visible, `knownKeys` still shows pending, files/journal recoverable, second account with different `uuid` cannot claim binding, retry with success clears.
7. **Write-vs-delete race:** start rotation write (delayed success), `signOutByUi` before completion -> stale write success with old generation must not set `ready` or clear `pending-delete`; must re-issue delete. Assert no resurrection via subsequent `get()`.
8. **Double-refresh serialize:** two `slotAuthentication/invalidate` triggers coalesce to one `AccountBasedOAuth`, one consumption of RT; second callback fenced.
9. **Enrollment gate:** `OAuth2AuthenticationStrategy::makeCreds + build` does not yield connectable account until first `set()` acks; interrupt -> no orphan connected account, no lost RT reported as ready.

### 4. Concrete flaw in proposed approach as worded

Two load-bearing gaps:

**a) Marker namespace collides with `knownKeys/clear`.** If `pending-write/pending-delete` live in the same `Credentials/...` group enumerated by `allKeys()` / `clear("http")`, they will either be mistaken for real keys (spurious `DeletePasswordJob` on marker names) or wiped by `clear()`. Must use excluded `_pending_*` subgroup/prefix and filter it in `knownKeys()` / `clear()` / `contains()`. Also `OC_ASSERT(...!=EntryNotFound)` in `remove`/`CredentialJob::start` must go — idempotent recovery legitimately sees `EntryNotFound`.

**b) Ordering + write/delete fence underspecified.** "Flushed and checked before consumption" must mean `QSettings::sync()` of `pending-write` **before** `refreshAuthentication(consumes old RT)` network call, not just before keychain write — otherwise crash after server invalidation but before marker leaves no trace. And deduplicating deletes alone does not fix resurrection: a rotation `WritePasswordJob` issued before logout can complete **after** `DeletePasswordJob`, recreating the secret with no index entry. The generation fence must cover `CredentialManager::set` vs `remove`, binding `scopedKey` immutably at issue time (do not recompute `host:uuid` after URL change), or the design still loses on the exact race `DC-4` calls out.

