# Exclusions planning consultation

Session `plan4-drive-2026-10-06`, S4-8. OpenCode primary planning job `fb57cbc0-09fb-466d-a77b-9120a402ab06` exited1 before a recommendation; no planning result is claimed for it. Its retained partial response was inspected and its delivery acknowledged. Route `0ad8b2f4-947a-4ed5-9dd7-6336662e6160` was escalated once. Enforced one-hop route `f12777a9-6a2f-4cd1-a84e-608701e6fe3b` selected Claude Opus; job `7adc1942-ffde-40fc-8dd8-fe99595c43d3` completed. The full retained response below was read before the source decision.

## Coordinator adjudication

The existing 1.1-compatible `[\\r]` projection is confirmed against both native source and the actual current app matcher (522 executed cases). Both loader completeness and validity of all six prepared native regexes now gate an authoritative snapshot. The invalid-range regression verifies a bad user glob returns unavailable and clearing it restores availability. Native leading CR/LF entries are ignored by prepare(); conservatively withhold the whole export when one is present, rather than rewriting it into a new effective rule or claiming a partial list. Focused sender tests cover both leading controls. No original matcher behavior is changed.

The actual app receiver stores only a valid complete frame; `importExclusions()` returns undefined without one, and `matrixDriveImportPatterns(undefined)` uses the existing pinned snapshot. Thus an offered/accepted capability with unavailable current data leaves that fallback in place. Full self-differential native tests now include full/traversal matching, files/directories, root/nested cases, uppercase, literal backslash-r and all256 byte suffixes. These library tests await hosted CTest; the current local receipt covers the QtCore sender target only. The frame represents the actual pattern list, not every native hidden/selective-sync/server rule. Frozen contract fixtures remain unchanged.

This is a planning consultation, not the final assembly code review. No follow-up consultation is needed for these verified source repairs.

## Full retained response

+Checked the projection helper and the attach/export path. Next I'm checking for other writers to the exclude list and the JS consumer.Found one real fidelity gap so far: entries starting with CR/LF. Now checking when `attachFacts` runs and whether `isReady` can be false at that point.I don't see a need for a protocol revision. The same-schema `[\r]` projection is sound for the actual upstream entry. The full-snapshot gating and the hello-time preflight are already mostly in the working tree. I found two small native-side fidelity gaps that I'd fix before calling S4-8 done. One limit up front: I could not inspect the app's JS (`driveExclusionExpression`; searching `~/Projects` timed out), so everything I say about the JS side is an acceptance criterion, not something I confirmed.

## Facts confirmed in the source
- **The entry itself:** `sync-exclude.lst:12` is `]Icon\r*`. `csync_exclude_expand_escapes` turns it into `]Icon<CR>*`.
- **Native handling of `[\r]`:** `convertToRegexpSyntax` copies a bracket expression straight into PCRE, so `[\r]` matches exactly CR. Running the projected list through native gives the same result as the raw list.
- **Projection:** `atumEngineExclusions` (`src/gui/atumengineprotocol.cpp:183-207`) keeps the raw list unchanged. It only rewrites CR, and only in patterns with no `[` and no `\`; otherwise it withholds the whole frame. It removes exact duplicates and re-checks the result with `atumEngineFactValid` (256 entries, 256 bytes each, 16 KiB line, no control characters, no `..`).
- **Reload tracking:** `ExcludedFiles::exclusionPatterns()` (`csync_exclude.cpp:249-254`) returns nothing unless the last reload completed. `addExcludeFilePath` clears that flag. A missing user list is never added (`ConfigFile::setupDefaultExcludeFilePaths` checks the file exists), so it doesn't fail the reload. A user list that is listed but unreadable does fail it, and you get nothing back.
- **Hello preflight** (`atumengine.cpp:62-72`) uses only `ConfigFile::defaultExcludeFile()` (a fixed resource path) plus `setAtumRootExclusions()`. It never calls `configPath()`, so it can't read or cache the default desktop-client config dir before `start`. Keep it that way. Using `setupDefaultExcludeFilePaths` at hello would read the wrong user's `sync-exclude.lst`.
- **Export timing:** the export happens once, in `attachFacts` (`:189-196`). Folders get attached again on `folderListChanged`, which `folder.cpp:652` emits after the VFS is ready, so a folder that isn't ready at load time still gets exported. The list comes from `loadDefaultExcludes` in the `Folder` constructor. `Folder::startSync` reloads it at `folder.cpp:964`, after the export.

## Must-fix (small)
1. **A broken pattern makes native exclude nothing, but the list is still exported.** All patterns are combined into one regex per category. One bad bracket in the user list (for example `[z-a]` or `[[:alpha:]`) makes the whole combined PCRE invalid, so native list-matching stops working, and `exclusionPatterns()` would still export the list as if it applied. In JS, the same pattern would likely throw or behave differently.
   - **Fix:** in `exclusionPatterns()`, also require `isValid()` on the six prepared regexes (`_bnameTraversalRegex{File,Dir}`, `_fullTraversalRegex{File,Dir}`, `_fullRegex{File,Dir}`). One line, no new state.
2. **Entries starting with CR or LF don't match.** `prepare()` skips entries whose first character is `\n` or `\r` (`csync_exclude.cpp:662-664`), so they never match natively. The projection would turn a user line like `\rfoo` into `[\r]foo`, which JS would match. A leading `\n` currently withholds the whole list for no reason.
   - **Fix:** in `atumEngineExclusions`, skip entries whose first character is `\n` or `\r` before projecting, the same way `prepare()` does. Add one test for each case.

## Not blocking, but write them down as acceptance criteria
- **JS parity (not verified).** `driveExclusionExpression` must do all of the following:
  - strip a leading `]`; otherwise `]Icon[\r]*` and every `].DS_Store` silently match nothing;
  - treat a trailing `/` as directory-only;
  - anchor a pattern to the start if it contains a `/`; otherwise match any path component, including parent folders;
  - make `*` and `?` stop at `/` on macOS;
  - turn `[!` into `[^`;
  - treat an unterminated `[` as a literal;
  - treat `\*`, `\?`, `\[`, `\\` as escapes and other `\x` as a literal backslash;
  - match case-insensitively on macOS (native uses case-insensitive matching when `fsCasePreserving`);
  - compile with no `u` flag, or prove `[\r]` and the escaped classes compile the same way under it.
- **The list is not the whole exclusion rule.** Native also excludes other things: hidden files when `ignoreHiddenFiles` is set, conflict files, names over 254 bytes, selective sync, the server blacklist and `invalidFilenameRegex`. The app must not treat the frame as complete.
- **No retraction.** The schema has no way to take a sent list back. The one-shot export at attach is fine because the private configDir has no editor in engine mode. If you later want to re-export on change, do it in the existing `SyncEngine::finished` handler, and only send when the result is valid and differs from the last frame. That's optional.

## Hello and full-list lifecycle (recommended semantics)
- **At hello:** offer `exclusions` only if the bundled system list plus the Atum list loads, passes the regex `isValid()` check, and projects into a valid frame. This matches spec §Exclusions ("If the full list cannot fit, do not advertise").
- **After `start`, the feature is a capability, not a promise of a frame.** Send exactly one frame when the bound folder first becomes ready, and only if:
  - the reload completed,
  - all regexes are valid,
  - the projection succeeded.

  Otherwise send no frame. That covers a missing, unreadable, invalid or unrepresentable user list. Don't `fail()`; other facts keep flowing.
- **App requirement (not verified):** if `exclusions` was accepted but no frame arrives, the app keeps its pinned static snapshot. Reading spec line 137 that way is my interpretation; please confirm it in the app's receiver before relying on it.
- Nothing else is needed: no new config, timer, Graph call, native path output or handshake change, and the frozen vectors stay as they are.

## Tests (cheap, no heavy checks)
- **Native self-differential** in `testexcludedfiles.cpp`:
  - Build two `ExcludedFiles`, one from the raw list and one from the projected list (via `addManualExclude`).
  - Assert identical `fullPatternMatch` and `traversalPatternMatch` results on this corpus: `Icon\r`, `Icon\rx`, `Icon`, `Iconr`, `Icon\\r`, `ICON\r`, `a/Icon\r`, `Icon\r/child`. Run each as both file and directory.
  - Repeat with the full real system list plus the Atum list.
- **Native, new cases:**
  - a user list with `[z-a]` → `exclusionPatterns()` returns nothing;
  - `\rfoo` and `\nfoo` → dropped, and the rest still exports;
  - a user list that is listed but deleted → nothing;
  - the preflight list projects to exactly the pinned expected array, including `]Icon[\\r]*`.
- **Shared vector for JS:** put the corpus above, with native's verdicts, in a vendored jsonl (`{pattern, path, isDir, excluded}`). The app's existing JS test then asserts `driveExclusionExpression` gives the same answers. Include `]`-prefixed, trailing-`/`, `[!…]`, `\*` and case-folded rows. This is the only test that actually proves native and JS agree.
