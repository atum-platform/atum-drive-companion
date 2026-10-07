# Complete S4-8 source assembly review

Exact reviewed source `791f6a1eef90d3f6e4bb6d38d178321705b718c7`, draft engine PR4. Primary OpenCode job060b2fd1-5f3a-43d8-bc56-6a5e9c2c1f30 failedprovider_exit before a verdict; the enforced one-hop Opus fallback bc58197a-bb02-42b1-b397-837cc4771990 completed **SHIP for source, zero blockers**. Both results were read, deliveries acknowledged, and routes closed exactly once (primary escalated; fallback completed). Full provider output below is retained advice, not executed verification. The public PR receipt also holds it for recovery.

## Verified disposition and limitations

Source tracing and executed focused checks confirm accepted-only/legacy behavior, complete native snapshot/load/regex gating, whole-frame caps and actual app matcher parity. QSet order is not material to matching; no exact-array equality is introduced. Leading CR/LF intentionally withholds the whole export instead of skipping entries. Reattachment is presently limited to shutdown. No targeted follow-up is required by the retained result.

The review's four-platform build-wiring statement describes the prior protocol target pattern; it is not a pass for the new library tests/current head. Those tests have not yet run. Hosted exact-head run37583768031 subsequently caught a test-source compilation error under QT_USE_QSTRINGBUILDER: explicit QString is required before binding the concatenated name to QStringView. The mechanical local-variable type repair does not change production behavior or the reviewer-adjudicated matcher semantics. It receives focused compiler verification and a fresh full hosted run, not a redundant broad source review.

Production engine objects compiled and changed-line format passed on the failed Linux run. Other jobs were cancelled by fail-fast. Realbinary accepted-only/folder-ready/stdout proof on the approved realm remains an A-HOST merge gate; signed release, app pin and trash guarantees are separate. No full Drive completion is claimed.

## Full retained response

+Still read-only. Folder load, reload and bundled-list paths check out. I'm now checking how the test target links and whether two engine instances can share state.**Verdict: SHIP the S48 source PR at `791f6a1ee`.** This is my source review only. It is not a production-readiness claim. I found no blocking problems in correctness, security or data loss, compatibility, or lifecycle and concurrency. I only read code: nothing was built, run, written or posted. Plan mode is on, but I have no file-writing or plan-exit tool here, so this answer is the whole review.

## What I checked in the source

**Exporter and frame validation (`src/gui/atumengineprotocol.cpp:183-213`)**
- **Legacy and accepted-only frames:** a `start` without `features` accepts nothing. `fact()` drops any kind that wasn't accepted (`atumengine.cpp:122`). The export is also gated on `features.accepted("exclusions")` (`atumengine.cpp:189`). The test at `test/testatumengineprotocol.cpp:159-165` covers both the legacy and current cases.
- **16 KiB UTF-8 cap:** the final check runs the real encoder, which measures the compact JSON plus LF in bytes. Escaped backslashes and multi-byte characters are therefore counted at their real size. The 100×203-byte test case proves a list over the cap is withheld whole. The per-entry 256-byte limit is checked on the raw UTF-8 before JSON escaping, which matches the frame validator.
- **No spurious engine error:** `fact()` re-checks with `atumEngineFactValid(message, {}, sentSyncAt)`. The exclusions branch ignores `lastSyncAt`, so that check is identical to the one the exporter already passed and cannot trip `fail()`.
- **Raw versus projected patterns:** a CR is rewritten only when the pattern has no `[` and no `\`.
  - In `convertToRegexpSyntax` (`csync_exclude.cpp:539-563`), the bracket scan sees `\` followed by `r`, not `]`, so it closes on the following `]`. `[\r]` therefore reaches PCRE as a class matching exactly CR, case-insensitive or not.
  - A leading `]` (silent removal) and a trailing `/` (directories only) are stripped by `prepare()` before translation, so `]Icon[\r]*` keeps both its silent-removal meaning and its scope.
  - Entries starting with CR or LF withhold the whole export. A CR after a leading `]` (for example `]\rfoo`) is still active in native and projects correctly.
  - Removing duplicates cannot change matching. Every pattern is combined into one alternation per group, and silent-removal versus listed exclusion depends on the group, not on order.
- **Bundled list:** I read `src/resources/theme/universal/sync-exclude.lst` in full. Every entry passes `pattern()`: none starts with `/` or `\`, there are no control characters other than the expanded `Icon` CR, no `..`, no drive letter, and well under 256 entries. The preflight and the round-trip test should therefore produce a frame on all three platforms.

**Reload state and regex validity (`csync_exclude.cpp:220-300`)**
- `_lastReloadComplete` is cleared when a file path is added and at the start of a reload. It is set only from `success`.
- A pattern added with `addManualExclude` keeps whatever state already exists, which is right because it lives in memory.
- All six prepared regexes must be valid before a list is returned, and the `[z-a]` test covers this.
- Matching itself is unchanged: `prepare()` and the order of `reloadExcludeFiles` are untouched apart from the flag.

**Hello preflight (`atumengine.cpp:62-72`)**
- `ConfigFile::defaultExcludeFile()` is a fixed Qt resource path (`configfile.cpp:225-228`). It never calls `configPath()`, so no private or desktop config is read before `start`.

**Folder and account lifecycle**
- `Folder` builds its engine and calls `loadDefaultExcludes()` (`folder.cpp:96`). That calls `setupDefaultExcludeFilePaths` with the private config directory (set by `setConfDir` at `start`), then `setAtumRootExclusions()`, then `reloadExcludeFiles()` (`syncengine.cpp:808-815`). The flag is therefore settled before the VFS starts.
- `setIsReady(true)` is followed by `folderListChanged` (`folder.cpp:651-652`), which reaches `attachFacts`. `wire()` connects that signal before `loadFolders` or `addFolderFromWizard` run, in both the existing-account and enrollment paths.
- A missing user list is never added (`configfile.cpp:515`). An unreadable one makes the reload incomplete, so no frame is sent, the other facts keep flowing, and `Folder::startSync` already reports the sync error at `folder.cpp:964`.
- **Concurrency:** the list is read on the main thread when the folder becomes ready, before it is queued for sync. The only writer during a run is `startSync` on the main thread, before discovery. The returned `QStringList` is an implicitly shared copy. Nothing new is introduced here.

**Build wiring**
- `target_sources(testexcludedfiles …atumengineprotocol.cpp)` copies the pattern `testatumengineprotocol` already uses. That target also links the shared `OpenCloudGui` library, and it passed on all four hosted platforms.
- The header's `AtumEngineProgress` has no `Q_OBJECT`, so no extra moc step is needed.

## Non-blocking findings (follow-ups, not required for this PR)

1. **The frame's order can change between process runs (`csync_exclude.cpp:279`).** `_excludeFiles` is a `QSet`, and Qt 6 randomises its iteration order per process. When a user list exists, the system and user entries can come out in a different order on each launch. Matching is unaffected, but any app-side check that compares frames or pins an exact array would be flaky. The preflight and current tests use one file, so they are deterministic. Fix it if equality ever matters, for example by keeping file paths in insertion order.
2. **No integration test covers the export call in `attachFacts` (`atumengine.cpp:189-196`).** The pieces have unit tests, but the step "folder ready → accepted → one frame on stdout" is only checked by reading. Live or loopback proof must confirm exactly one frame after ready, and none when the feature is not accepted.
3. **A replacement folder would trigger a second, identical frame.** If the folder were deleted and another attached, `factsFolder` would go null and `attachFacts` would send the full list again. Today only `stop()` unloads folders, so this can't happen now. The same re-attach behaviour already applies to the existing quota and sync facts.
4. **The planning doc mixes two answers.** The retained Opus text recommends *skipping* entries that start with CR or LF, while the adjudication and the code *withhold the whole export*. The adjudication paragraph states the final decision, so this is acceptable. Someone skimming only the "Must-fix" list could misread it.
5. **Out of scope: user patterns of just `]`.** Natively such an entry adds an empty alternative, and the exporter would pass it through. This matcher behaviour predates this PR. I flag it only as a possible gap if the shared native-versus-app test vectors are extended.

## Claims I did not verify
- **App side:** the 522-case app parity run, the fallback where `importExclusions()` returns `undefined`, the 167/167 QtCore pass and the frozen-digest results come from your logs. I did not re-run any of them or look at app code.
- **New library tests:** the added `testexcludedfiles` cases (round-trip, all 256 bytes, invalid regex, unreadable list) have **not run anywhere yet**.

## Remaining gates (separate from SHIP)
- **Exact-head hosted CI on `791f6a1ee` (run 37583768031):** full build on Windows, macOS and Linux, changed-line clang-format, and CTest including the new `testexcludedfiles` cases. That run must pass before merge.
- **Real-binary loopback on the approved synthetic realm (external owner):** still pending. It should cover hello offering `exclusions`, exactly one frame after the folder is ready, no frame when the feature isn't accepted, and the app falling back to its pinned list when no frame arrives.
- **Not covered by this PR:** S4-9 trash, which waits on the server's atomic no-overwrite restore and restore-quota guarantees; and tag, release, signed pin, flags, quota/retention and production or host operations, which keep their separate owner gates. Full Drive completion is not claimed.
