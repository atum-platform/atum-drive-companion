# Atum Drive companion custody and release

Hosted validation run 37000282801 at 8bc5202 built successfully, but Linux's
real credential tests failed because the container had no D-Bus Secret Service
(`Cannot autolaunch D-Bus without X11 $DISPLAY`). ARM macOS passed; fail-fast
cancelled Intel macOS and Windows. This is not passing matrix evidence. Linux
tests now run in a fresh D-Bus session with a disposable GNOME keyring and a real
store/read/delete preflight. The directory and daemon are cleaned on exit;
only synthetic test entries use it. No production fallback or test skip was
added. Local bash syntax/diff checks precede a new exact-head hosted run.

Run 37003038322 at bbe2fcb failed at the new Secret Service bootstrap before
tests; other matrix jobs were cancelled by fail-fast. The daemon's diagnostic
was inside its disposable directory and absent from Actions output. The wrapper
now emits its test-only daemon error on failure, and a Linux preflight runs
before the lengthy Craft dependency/build stage. No failed check is treated as
passing and no unchanged-code rerun was requested. Local Linux probes were
unavailable: Mini Docker has no daemon socket, rpi SSH is down, and the asus
alias does not resolve on this host. Hosted diagnostics remain the actual lane.

Owner scope: finish cloud agents, Atum Drive and cloud execution. This independently
packaged GPL-2.0-or-later fork contains no Atum proprietary code. Baseline source
9bf87d5ea35b847a40dd7370fcfe104313150da4; custody source and focused native tests are complete; signed release
and broader companion integration remain to be completed. Official unchanged Desktop 4.0.0 real S2 evidence belongs
to the platform PR, not this fork's release.

## Parallel MECE workstreams

1. Companion-only credential state: secure-store enrollment/rotation acknowledgment,
   failed/lost writes, serialized rotation, and acknowledged/pending logout cleanup.
   Owns credential manager, HTTP credentials and their existing GUI consumers/tests.
2. Coordinator-only build/release: official KDE Craft baseline/toolchain, public
   GPL source/build notices, Atum configuration/branding and pinned artifact receipt.
   A tested source build precedes signing on the existing MacBook Developer ID.
3. Coordinator-only platform acceptance: isolated S2 negative/lifecycle proofs and
   durable app/platform records. These do not change companion source.

The build can prepare dependencies while custody is inspected, but new code must
compile and pass focused tests before review/signing. Lifecycle/two-Mac acceptance
depends on the reviewed built artifact. No cross-family nested review, independent
worker push/PR or production activation is permitted by this plan.

## Confirmed premise

`HttpCredentials` sets ready/access state and calls void persist before the
asynchronous keychain write finishes, including the refresh path. GUI reauth does
the same. `CredentialManager::remove` drops its persisted index before the async
DeletePasswordJob result, and failure only logs. `AccountState::signOutByUi`
reports SignedOut immediately. These do not satisfy frozen Drive DC-2/DC-4 custody
and cleanup acknowledgment. Preserve files/dirty journals and upstream token
rotation; repair these boundaries without a plaintext fallback.

## Build preparation

Both Macs lack a ready Qt/CMake toolchain and their internal disks have only
3.5/4.2 GiB free. MacBook has its existing valid Anka Developer ID Application
identity; no key was exported. Mac mini's external ExFAT drive has 2 TB free. A
new task-owned 20-GiB-capacity sparse APFS image under
`/Volumes/My Book/atum-companion-build-01a0fad5` is mounted at
`/private/tmp/atum-companion-build-01a0fad5` for the official KDE Craft dependency
workspace. Only public source/build dependencies go there. Signing is disabled
in this build lane. Existing user files and signing keychains are untouched.

Routing decision b855b330-ccfe-456e-9d62-167634444a07 enforced direct exploration
because native Codex quota was exhausted. No worker was spawned. A single review
will follow assembly and real focused verification; no release is claimed yet.

The coordinator owns source at
`/Users/nmmacmini/projects/atum-platform/.worktrees/drive-companion-custody`,
branch feat/drive-custody-release, authoritative origin
https://github.com/atum-platform/atum-drive-companion.git. The initial temporary
clone remains an unchanged baseline build input only. Review transport rejected
/tmp as outside configured workspaces before running; this owned source copy
fits its workspace fence.

One focused architecture consultation is job
9229dcf2-0f54-4ae3-8a70-d21a443056cf, route decision
9891cf00-5ac8-40d6-a857-4b0d27b93900 (enforced OpenCode/default because Claude
quota was exhausted). Exact owner:
cloud-launch-finish-01a0fad5:companion-custody-design:Drive companion custody design:Drive companion custody design.
Result must be read before acknowledgment/implementation decisions. This is
advice, not assembly code review or a release approval.

## Custody implementation

The retained consultation was read, acknowledged, and routing feedback closed.
Enrollment and refresh publish ready/access/fetched only after an acknowledged
keychain write. The access manager attaches no bearer while unready. A durable,
non-secret rotation marker is saved before the old refresh token reaches OAuth;
an interrupted or uncertain rotation requires fresh browser sign-in. Unlike a
blind token retry, this cannot replay a token whose response was lost. These
failures request sign-out without clearing queued uploads/dirty journals.

Deletion retains the index until NoError or EntryNotFound, prevents reads and
reconnect while pending, exposes failure with an explicit retry, and fences late
writes with a second delete. Separate operation metadata avoids confusing fences
with credential keys. Scope is captured once, including across a server-URL
change. Real jobs hold the account until acknowledgement so cancellation of an
enrollment window can finish its cleanup. The initial wizard completes only
after secure storage succeeds. Files are never removed by these paths.

Focused verification includes controlled failure/order jobs and the existing
real QKeychain path. No production plaintext fallback is enabled. The isolated
build configured and compiled successfully with Qt 6.11.1/QtKeychain 0.16.0 and
the restored vendor shelf. The final native suite has 16 passes, zero failures
and zero skips, including real secure-store write/read/corrupt-read/delete,
write/delete timeout fences and both late-write/delete orderings. The OAuth
suite has 17 passes, zero failures and zero skips, including lost/rejected
rotating-token replies. Neither suite enables a production plaintext fallback.

The initial expanded rerun exposed upstream one-second/five-second fixed test
waits: native OS completion took several seconds. Completion-based bounded waits
replace those timing assumptions. The Apple QtKeychain backend dispatches its
completion on the main queue; custody tests use the same native Cocoa
QApplication dispatcher as Desktop instead of QCoreApplication/offscreen.
The test fix does not relax production's thirty-second custody timeout. Test
scope remains account/core credentials; it is not a packaged wizard/account
switch, process-crash or two-Mac acceptance receipt.

Reproduce from an official restored Craft SDK: configure CMake with that SDK
as CMAKE_PREFIX_PATH, BUILD_TESTING=ON and VFS/crash-reporting/auto-update off;
build targets `testcredentialmanager testoauth`; run those two binaries. On
macOS use the native Cocoa dispatcher for the credential binary. All test
credentials are disposable and bound to generated test account UUIDs. The
controlled-job seam is private/friend-only; the runtime always starts real
QKeychain jobs with insecure fallback disabled. Preserve LICENSE/COPYING and
upstream attribution when packaging. Signing/release and two-Mac acceptance
remain unclaimed.


## Independent custody source review

Primary assembly job `7ea130ac-c788-4f9a-be74-87c80b4f555a`, decision
`af7db308-5c8e-4674-b95f-c91cc238b9fe`, returned SHIP for custody source with
no blocking defects. Complete retained result was read, acknowledged and routing
feedback closed. The nonblocking logout ordering finding was confirmed: queue
blocking still happens immediately, then forgetSensitiveData writes its durable
cleanup fence before publishing SignedOut. The redundant earlier SignedOut call
was removed. Existing account-removal orphan cleanup, exact-key OAuth clearing
and force-sync UI gating remain retained pre-existing follow-ups; no release or
full account-switch acceptance is inferred. No second review was launched.

The reviewed correction has a real AccountState regression test: its SignedOut
notification observes the durable deletion fence, and reconnect remains blocked
until the deletion acknowledgement. The rebuilt native custody suite passed all
16 cases without skips; the OAuth suite passed all 17 cases. Changed C++ lines
were formatted with the repository's clang-format 19 convention, and the changed
QML file with the restored SDK's qmlformat. These checks cover the final source.

A separate real unchanged SyncJournalDb baseline confirms upstream SQLite
exclusive ownership blocks a same-path or symlink-alias process while the owner
lives. Each case independently recovers after killing only its generated owner,
and retains the generated local edit. Repeating the proof with two simultaneous
waiters timed out after owner death: they can remain mutually busy, so concurrent
recovery is not proved. The earlier preliminary receipt overstated that recovery
scope; the committed reproducible receipt supersedes it. No new journal lock is
justified for exclusion, but the Desktop update path must serialize ownership
transfer before two waiting clients can enter. Full binding/update-overlap
acceptance remains separate.

The reproducible manual proof is `test/manual/journal-exclusion`: configure its
CMake project with the restored Qt SDK, COMPANION_SOURCE pointing to this checkout
and COMPANION_BUILD pointing to the built Desktop tree, build `journal-probe`, then
run `python3 test/manual/journal-exclusion/prove.py <built-journal-probe>`. It links
the real companion library, uses only generated temporary data and kills only its
own owner process. Its JSON receipt distinguishes this proof from full Desktop
acceptance.

## Hosted validation

Custody PR https://github.com/atum-platform/atum-drive-companion/pull/1 is open.
GitHub reported Actions enabled but zero registered workflows/runs/check suites;
enabling the imported workflow by filename returned 404 despite the source file
existing on main. The workflow name now identifies this fork, and the upstream
scheduled release trigger is removed: this repository does not schedule vendor
signing or releases. Normal PR validation still uses the existing hosted Linux,
macOS and Windows build/test matrix. Required current-head checks must be observed
before merging; an empty check list is not a passing result.
