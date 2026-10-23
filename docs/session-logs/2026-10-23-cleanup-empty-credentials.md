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
