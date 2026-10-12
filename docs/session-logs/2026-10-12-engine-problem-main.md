# Drive ignored-file result classification — 2026-10-12

OpenCloud defines `SyncResult::Problem` as a completed sync where some files
were ignored. The Atum stdio adapter previously sent `error` for every status
not explicitly listed, so this normal native result made the desktop report
that Drive stopped and hide its usable state.

This narrow main-branch fix maps both `Success` and `Problem` to `ready`. Every
other native status keeps its existing closed mapping. The mapping is extracted
as a pure exported helper and a focused native test locks all nine enum values,
so a future status cannot silently fall into a generic UI error branch.

The patch changes no protocol field, feature negotiation, OAuth/credential
custody, retry scheduling, transfer, file, root or server behavior. It is split
from the larger engine 1.1 facts PR so the currently shipped 1.0 adapter defect
can merge without waiting for that PR's separate real-binary facts acceptance.

Verification before commit:

- `git diff --check` passed.
- Source verification confirms `Folder::slotSyncFinished` resets consecutive
  failure state for both `Success` and `Problem`, and the native display string
  describes `Problem` as success with ignored files.
- Independent OpenCode review `022a638d-54ed-401d-b2bd-db70b561a7c7`
  returned `SHIP` with no blocker or high-severity findings. It verified the
  nine-value mapping, additive ABI, cross-platform test wiring and error-state
  precedence. Its one low advisory was the intentional closed mapping: a future
  enum value must be handled explicitly instead of silently becoming `error`.
- Full local compilation is held below the repository's documented 5 GiB free-
  disk floor. The existing GitHub-hosted Linux, Apple Silicon, Intel macOS,
  Windows and clang-tidy matrix must compile and run the new test on the exact
  pushed head before merge.
