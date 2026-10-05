## Release-base correction review — `df91` vs released `v4.0.0-9f701e6` beneath Atum overlay

Scope: only Folder root binding merge + OAuth legacy init merge + theme/shelf/ABI delta. Atum custody/identity/root overlay not re-reviewed except interaction. No files edited, no providers launched, source tests not treated as launch evidence.

### Introduced blockers — restore reintroduces failure beneath overlay

**P1 — OAuth double-failure + WebFinger-on-error — `src/gui/newwizard/states/oauthcredentialssetupwizardstate.cpp:21-75`**
Concrete: old `4.0.1` early-returned on `Error/ErrorInsecureUrl` before WebFinger. Restored code runs `WebFingerInstanceLookupJob` for every `OAuth::result`, then `finish()` emits second `evaluationFailed`.
Failure: `ErrorInsecureUrl` triggers useless bearer lookup, then two `evaluationFailed` in one job completion -> `SetupWizardController::changeStateTo` twice, duplicate `OAuth`/`State` churn, wrong message (`Failed to look up instances` masks `requires secured connection`).
Fix: gate WebFinger to `LoggedIn` only; single terminal emit; keep Atum pinned issuer/200 guards untouched.

**P1 — `QmlOAuthCredentials` null-contract narrowed — `src/gui/creds/qmlcredentials.cpp:49-54`, `src/gui/creds/qmlcredentials.h:67`**
Concrete: `if(_oauth)` guard + `// oauth might be null` removed, unconditional `connect(_oauth,...)`.
Failure: any retained caller passing null (old `failed?nullptr:new OAuth` path) now null-deref. Current tree avoids null by always constructing, but public `QmlOAuthCredentials(OAuth*)` API silently changed.
Fix: either restore null guard + `Q_ASSERT`, or make param `not-null` explicit and grep all overlay/tests for null pass.

**P1 — Wizard retry path deleted, auto-loop — `src/gui/newwizard/setupwizardcontroller.cpp:110,128-140`, `oauthcredentialssetupwizardpage.cpp:33`, `oauthcredentialssetupwizardstate.h:26`**
Concrete: `EvaluationRetry`, `requestAuthRestart`, `failed:bool` ctor, `evaluationRetry` signal all removed; `Q_UNREACHABLE()` default added.
Failure: no user-initiated auth restart; `evaluationFailed` recreates same state which immediately `startAuthentication()` -> tight fail-loop hammering IdP, no backoff. Double-emit above amplifies.
Fix: restore explicit retry affordance or backoff + terminal error state; audit `AbstractSetupWizardState::~` removal (`abstractsetupwizardstate.cpp:24`) for `_page` ownership vs `displayPage()` — leak vs double-delete unclear from diff.

**P0 — Build shelf unpinned — `.craft.shelf:10`**
Concrete: `revision = a8e0ae363d5cd2c9e453bb191482637454831b36` -> `revision = 11b07503a` (9-char short).
Failure: non-hermetic, ambiguous fetch; cannot reproduce pinned `Qt6.11.1/LibreGraph1.0.7/Keychain0.16` claim.
Fix: restore full 40-char SHA for `craft-blueprints-kde`; record clean hosted fetch receipt.

**P1 — QML/ABI surface cut without overlay grep — `src/gui/folder.h:59`, `src/gui/folder.cpp:184,860`, `src/gui/qml/FolderDelegate.qml:91,319`, `src/gui/commonstrings.h/cpp`, `src/gui/qmlutils.h/cpp`, `src/gui/protocolwidget.cpp:122-140`**
Concrete: removes `Folder::webUrl/webUrlChanged/_webUrl`, `CommonStrings::copy*ToClipBoard`, `OCUtils::hasKeyboardModifiers/setClipBoard*`. `FolderDelegate` now `folder.openInWebBrowser()` only.
Failure: any Atum QML/C++ still binding `folder.webUrl` or `OCUtils.setClipBoard` throws at runtime / fails compile. Removing `Q_PROPERTY`+signals changes `metaObject`/layout with `MIRALL_SOVERSION 0` unchanged (`VERSION.cmake:4` 4.0.1->4.0.0) — mixed 4.0.1 artifacts break.
Fix: grep overlay for `webUrl|copyUrl|copyFilePath|setClipBoard|hasKeyboardModifiers`; require full rebuild, ABI dump, no mixed deploy. Correct for release parity, blocker until proven.

**P1 — OAuth prompt serialization drops `login`, no empty-IdP fallback — `src/libsync/creds/oauth.cpp:789-799,1023-1030`, `src/libsync/creds/oauth.h:65`**
Concrete: `Q_FLAG`->`Q_ENUM`, `enumValues<>` removed (`src/libsync/common/utility.h:271`), `toString()` hardcoded to `{consent,select_account}`.
Failure: theme requesting `login` parses but never serializes -> wrong account/session; IdP with empty `prompt_values_supported` now keeps both values vs old `consent`-only fallback — compat flip for Atum IdP.
Fix: keep upstream scope/prompt defaults per instruction, but add prompt matrix test (empty/disjoint/`login`/branded override) and document `login` unsupported; do not silently expand.

**P2 — Prompt default globally cached — `src/libsync/creds/oauth.cpp:66-77,256-260`, `src/libsync/theme.cpp:349-357`, `src/libsync/theme.h:234-244`**
Concrete: `static promptValue` captures first `Theme::instance()->openIdConnectPrompt()`.
Failure: branded/OEM/test theme swap after first `OAuth` keeps stale prompt. `52 OAuth` beta pass does not cover multi-theme order.
Fix: remove static cache or key by theme instance; add theme-swap test.

### Pre-existing upstream — disclosed, not Atum-introduced

* `folderwatcher_win.cpp/h`, `cfapiwrapper.cpp/h`, `vfs_cfapi.cpp`, `utility_win.*`, `discovery*.cpp`, `owncloudpropagator.cpp`, `path.cpp`: `v4.0.0` reverts reintroduce upstream fragility — raw `HANDLE` event leak, `QFileInfo::isDir()` inference vs explicit `isDirectory` (`cfapiwrapper.cpp:537`), `qFatal` on unknown exclude (`discovery.cpp:176`), `TOCTOU` open-per-call. Expected for release parity; VFS off per enrollment doc mitigates, but no Windows native VFS/watcher ship without native dispatcher run.
* `theme.cpp:233`: `aboutVersions` drops `productVersion`, mislabels `%2` as kernel — observability loss, upstream.
* `VERSION.cmake:4` downgrade correct, but requires clean rebuild.

### Missing-test gates

Claimed: 9 chooser +17 binding +8 folder +16 exclusions +3 root-loss +52 OAuth +16 Keychain +33 CTest offscreen / 11 watcher native. Insufficient for ship: local only, offscreen vs Cocoa split admitted, no exact-head hosted CI, no `webUrl`/clipboard grep, no prompt/issuer matrix, no shelf reproducibility, no live root acceptance, no signed distribution. Doc itself lists independent assembly review + hosted CI as remaining gates — concur.

### Verdict

**DO NOT SHIP** as releasable 4.0.0 base.

Ship limits: fix P1 wizard double-emit/loop + null contract, pin full Craft SHA + hosted clean build, prove no `webUrl/clipboard/enumValues/Q_FLAG` consumers + full rebuild + ABI check, add OAuth prompt/issuer matrix + QML `openInWebBrowser` binding test + native watcher/VFS runs, then independent assembly review + exact-head hosted CI + live root acceptance + signed distribution separately.

