# Empty-credential cleanup completion — 2026-10-23

Branch `fix/cleanup-empty-credentials` started from prepared engine facts head
`7c5c85abf4a2949078e0bdeeb0c947d0f6a4c754`. Scope is the bounded Atum Drive
engine cleanup completion bug; desktop deadline alignment, integration,
building/signing/restaging, installed recovery, CI, and release remain with the
parent session.

## Verified premise

An otherwise valid single restored account can have no `Credentials`,
`CredentialOperations/pending`, or `CredentialOperations/failed` metadata.
Both the startup `cleanup` request and the live `forget` request call
`AccountState::signOutByUi()`, whose credential clear schedules zero keychain
jobs in that state. `CredentialManager` therefore emits no
`stateChanged`/`credentialCleanupChanged` signal. The engine previously waited
only for that signal, so it never sent `disconnected` and never exited.

## Implementation

`AtumEngineCleanupCompletion::finishCleanupIfSettled` is the one idempotent
settlement classifier used by the engine. It emits one terminal protocol state:

- `cleanup_failed` when credential-operation persistence or deletion has
  failed, even if a durable pending-delete fence remains;
- no terminal state while an ordinary deletion is pending; and
- `disconnected` when no deletion is pending and no failure exists, including
  the zero-credential/zero-job case.

The engine invokes the same path from `credentialCleanupChanged` and
immediately after each `signOutByUi()` plus `AccountManager::save()` sequence.
Terminal shutdown is queued after publishing the state because a settings
persistence failure may be reported synchronously from inside `signOutByUi()`;
this lets that stack and the explicit account save finish before account
destruction. The completion latch prevents duplicate state output or shutdown
scheduling.

The existing exact issuer, subject, Drive origin, single-account, sync-root,
and config-directory validation fences are unchanged. No account schema,
credential binding, user file, configuration, or external service was changed.

Focused `testatumenginestate` coverage now checks zero scheduled jobs,
asynchronous pending-to-settled completion, immediate persistence failure,
pending deletion failure, and duplicate completion.

## Verification and limitations

- `git diff --check` passes.
- The source targets C++20, and installed Qt Core/Test metadata is available at
  6.11.2.
- No configured build directory or existing `testatumenginestate` executable
  is present in this checkout or the bounded searched project paths.
- Native compilation and QtTest execution were not run. The volume had only
  about 723 MiB free, below the task's safe capacity for configuring this
  Craft/CMake engine tree; no dependency install, configure, heavy build,
  generated artifact, worktree, CI run, or cloud operation was attempted.

The parent session must integrate this local commit, run the focused native
`testatumenginestate` target in its prepared engine build, then own the full
build/sign/restage and installed recovery evidence.

## Coordinator native verification

Exact source dba50e8d80599d224583ac4e3eb5e156a21ae076 was archived and built
on the mini external APFS image using Xcode27 / Clang21, Qt6.11.1,
CMake4.1.4 and Ninja1.13.2, production profile, macOS14 deployment target.
Focused testatumenginestate compiled (292 steps) and passed all8 QtTest cases.
Full engine build with BUILD_SHELL_INTEGRATION=OFF completed. Custody suite
(excludedfiles, rootbinding, rootfolder, enginestate, engineprotocol) passed5/5.

Broad safe suite passed33/37 targets. Credentialmanager was deliberately not
executed because it accesses Keychain. The four failures (folderwatcher,
folderman, protocolmodel, oauth) were rebuilt and run against exact parent
7c5c85abf4a2949078e0bdeeb0c947d0f6a4c754 in a separate matching environment.
Every failed assertion, value and line matched; normalized assertion SHA256
c955345ce2b64e34c9dcfa6786f532dce27bb91e872a3a274342b7f5a08bff2f.
These are pre-existing profile/environment gates, not cleanup regressions.

Default FinderSync compilation failed on its upstream10.15 deployment target
with Xcode27. Excluding shell integration matches the currently shipped Atum
embedded engine inventory and startup path; no existing product feature is
removed. Finder badges/context menus remain separately scoped future work.

Evidence on mini: /private/tmp/atum-companion-build-01a0fad5/
focused-dba50e8d-20261023b-logs and baseline-7c5c85ab-20261023a-logs.
Production executable SHA256 before bundle assembly/signing:
b757a18f5e571425e460d09c4975dd03b9e9a296de3914399318d235f60f474d.
This is not release readiness: independent review, exact-head hosted checks,
bundle assembly/signing/restaging and installed recovery remain outstanding.

## Engine-process regression follow-up

Independent review approved the cleanup classifier contract but found that its
tests stopped at the extracted settlement logic. They did not prove the real
zero-job path through account restore, `signOutByUi()`, account save, protocol
output and process shutdown. The follow-up adds a production-profile subprocess
regression using a disposable config with one exact-identity account and no
`Credentials`, `CredentialOperations/pending` or
`CredentialOperations/failed` keys. It sends the same startup `cleanup` frame
as the desktop, closes stdin immediately to exercise EOF racing the deferred
cleanup stop, and requires one `disconnected` state, exit zero and unchanged
sync-root file bytes. The subprocess receives isolated `HOME` and `TMPDIR`
directories and its empty credential inventory prevents any Keychain job or
external request from being scheduled.

The process shutdown path now has its own `stopped` latch inside `stop()`.
`stopping` cannot provide this guard: cleanup sets it before queuing `stop()`,
and stdin EOF may call `stop()` before that queued callback. The independent
latch makes folder teardown, account-manager shutdown and `qApp->quit()`
idempotent without weakening the existing protocol-output gates.

The credential-job timeout remains 30 seconds **per OS Keychain attempt**.
Deletion-after-timeout, a fenced late write, a repeated delete and event-loop
or persistence work can add overhead, so 30 seconds is not a hard overall
cleanup deadline.

`AccountManager::save()` still returns `void`; a settings write can therefore
fail without making the SignedOut persistence result observable to the engine.
That is a separate durability/diagnostic gap, not evidence that credentials
were retained, and is deliberately not broadened into this bounded fix.

Exact implementation commit `c51742cfafaca5b023bc9e66a068c254c6114aab`
was archived into a new isolated mini source/build directory and compiled with
the existing verified SDK: Xcode 27 / Clang 21, Qt 6.11.1, CMake 4.1.4,
Ninja 1.13.2, production profile, RelWithDebInfo, macOS 14 minimum,
`BUILD_TESTING=ON`, `BUILD_SHELL_INTEGRATION=OFF`, parallelism two. The clean
target build completed 292/292 steps. `testatumenginestate` passed 9/9 QtTest
cases, including the subprocess regression; the combined engine state/protocol
CTest run passed 2/2 targets. The subprocess-only case then passed 20/20
additional isolated repetitions while closing stdin immediately after the
cleanup frame.

The process proof validates the exact production issuer/origin and a restored
synthetic owner, zero credential-operation metadata before and after cleanup,
exactly two stdout frames (`hello`, then one `disconnected`), normal exit zero,
unchanged sync-root file bytes and no Keychain diagnostic. No live account,
external endpoint, credential, Keychain item, installed app or user file was
accessed.

Evidence directory:
`/private/tmp/atum-companion-build-01a0fad5/followup-c51742cf-20261023a-logs`.
Source archive SHA-256 is
`46df21fb3238fad8620abb4d752a5520dc29ac23d1fb959d7043e50c8c5e233e`;
focused CTest log SHA-256 is
`7bc9bfdc7918c75428d2e136e06220999183d973eada17652f1450fddb25f6cd`;
20-run process log SHA-256 is
`71412e51f9e5f4919793a491bd6ee8369be580488ce362ab3e13e21c4c63a90c`.
