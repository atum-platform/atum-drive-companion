# Drive completed-with-ignored-files classification — 2026-10-12

This checkpoint closes the adapter defect identified while implementing desktop
local-first browsing. OpenCloud's `SyncResult::Problem` is a completed run with
one or more ignored files. The Atum engine previously let that value fall through
to its generic `error` state, which made the desktop report that Drive stopped
even though the native engine considered the run successful and retained normal
reconnect scheduling.

The engine now maps both `Success` and `Problem` to the usable `ready` state.
A real successful run in either status advances the engine-owned successful-sync
timestamp; an aborted run does not. The mapping is a closed helper covering every
native `SyncResult::Status`, and the focused regression asserts all statuses plus
the successful-run predicate. The wire protocol remains 1.0/1.1 compatible: no
field, feature, credential, path, retry policy, or transfer behavior changes.

This change is assembled on the existing engine 1.1 facts branch and draft PR
#4. It does not sign, tag, release, pin, deploy, advertise trash, or change the
OpenCloud server. The desktop must separately present the existing nonzero
`excluded` count as attention rather than claiming every file synced.

Verification before commit:

- `git diff --check` passed.
- Source inspection confirms upstream `Folder::slotSyncFinished` treats
  `Success` and `Problem` as non-failing completed statuses and assigns a real
  sync time to both.
- Local full engine compilation remains inappropriate on this host while free
  disk is below the repository's documented 5 GiB heavy-build floor. The exact
  pushed head must pass the existing GitHub-hosted Linux, Apple Silicon, Intel
  macOS, Windows, and clang-tidy checks, including the new focused test, before
  the PR can leave draft.

The prior source review for PR #4 predates this production change and is not
reused as its review. One new assembly review is required for the resulting
diff, followed by exact-head hosted checks.

Review route `8f19853a-132a-45ef-aee3-6a0ef7c2dd08` selected OpenCode job
`a3e90780-9551-4e9e-baa2-02e1471c1404`. The provider verified that the native
result definitions support the premise and was tracing completion/timestamp
semantics when it failed with provider `UnknownError`, before findings or a
verdict. Its retained partial result was read; it is not treated as approval.
The one allowed escalation returned enforced `direct` decision
`e174ec80-67f6-48a6-9e32-2603b35ec33e`, so no second provider was submitted.
Coordinator review rechecked every enum value, the `success` boolean fence,
`SyncResult::setStatus` timestamp assignment, `Folder::slotSyncFinished`'s
Success/Problem handling, and the existing sync-fact warning counters. No
blocking source finding remains; exact-head compilation/tests are still the
ship gate.

Integration follow-up:

- The independently shippable `Problem -> ready` subset merged to `main` in
  PR #5 at `489d9312238ade24230224bfdf0e963f57f0e805` after the exact-head
  Linux, Windows, Apple Silicon, Intel macOS, and clang-tidy gates passed.
- PR #4 was then merged with that new `main`. Its four mechanical conflicts
  kept the branch-only 1.1 additions (`atumEngineRunSucceeded`, protocol tests,
  and excluded-file facts) on top of the now-authoritative mapping and focused
  status test from `main`.
- PR #4 remains intentionally draft. A new exact-head hosted matrix must pass
  after this integration, and leaving draft still requires the separate real-
  binary synthetic-realm acceptance gate; this merge does not weaken or bypass
  that gate.
