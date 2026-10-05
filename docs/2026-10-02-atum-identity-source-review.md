# Atum identity/OEM assembly review — 2026-10-02

Reviewed checkpoint: `4ee0ead`, base `bbe2fcb`.
Primary job: `2cb6396d-de3b-45bc-b96e-3c80e113c43b`.
Enforced decision: `701c7ca0-9491-4479-99ee-988315141bc0`.
Provider: OpenCode Go / Muse Spark. Complete retained result was read.

The verdict is SHIP for this source checkpoint only. No introduced blocker was
reported. The independent review confirms the compiled Drive origin, issuer,
public client and openid scope, canonical subject and fresh UserInfo binding,
rotating refresh handling, redirect/TLS refusal, log suppression, Theme profile
precedence, and preservation of unbranded behavior. It did not execute tests.

Native coordinator verification passed 52 OAuth/resolver cases and 16 custody
cases without failures or skips. The live Atum S2 app subsequently completed a
fresh grant and uploaded files before and after initial access-token expiry;
independent downloads matched their bytes. The live receipt records the build
provenance and remaining root/lifecycle limitations separately.

Nonblocking review notes include missing explicit mutation rows for token type,
audience and a refresh error string; qrc icon mapping; default empty signal
arguments; and null-reply resolver caller assumptions. These are checked against
the source separately. Actual MAS grant/refresh scope echo is exercised by the
successful live path. No follow-up review is justified by a blocking finding.

Coordinator checks: oauth.h result arguments are empty QString defaults.
The generated S2 theme.qrc maps
`:/client/AtumDriveTest/theme/universal/wizard_logo.svg` to the original OEM SVG,
matching the callback template. ServerUrlSetupWizardState consumes only the
resolver's success/error/result signals and never dereferences its nullable
reply. Resolver internal reply use occurs only after a real network job is
created. These close the review's corresponding missing-evidence notes.

Root enrollment/inventory, serialized update handoff, lifecycle status,
4.0.0/4.0.1 release-pin reconciliation, packaging, signing/notarization,
two-Mac acceptance, production activation and G8 remain separate open gates.
This checkpoint is committed locally; public publication was rejected by
automatic approval review pending explicit approval of this source payload.

## Complete retained reviewer response

**Atum identity/OEM source checkpoint 4ee0ead vs bbe2fcb — read-only review**

No introduced fail-open found. All B1-B6 pins are exact and fail-closed before grant/token/enrollment.

**1. [Info] No credential disclosure in source — Introduced PASS**
Files: `oem/atum/OEM.cmake:19`, `oem/atum/atumprofile.h.in:3-5`, `src/libsync/creds/oauth.cpp:356,435`, `src/libsync/httplogger.cpp:142`
Only public values in tree: origins `https://drive/spike-drive.atumplatform.com`, issuers `https://auth/spike-auth.atumplatform.com/`, public loopback client `01JDRVCMPNXQ7R8S9T0V2W3X4Y`. No secret, refresh/access, private key.
Logs: loopback `peek` with `code` removed → `Received loopback callback`; wrong-user `IdToken.toJson()` removed; `HttpLogger::logRequest` early-returns when `oauthIdentityProfile()` present regardless of `lcNetworkHttp` debug/env. Repro: `grep -rn "peek\|toJson.*idToken\|code_verifier" src/libsync/creds/oauth.cpp` shows no raw secret log.

**2. [Info] B1 WebFinger subject/issuer/client/scopes exact — Introduced PASS**
File: `src/libsync/creds/oauth.cpp:691-698, identityProfileMatchesServer:253-259`
`_serverUrl==driveOrigin + https + no userinfo/query/fragment + empty path + pinnedEndpoint(issuer,issuer) + client non-empty + scopes==openid` gates `startAuthentication` and `fetchWellKnown` before socket/browser/network.
WebFinger then requires `subject==_serverUrl.toString()`, `issuerUrl==issuer` exact, `client_id==clientId` exact, `scopes array size==1 && [0]==openid`. Missing/foreign → `AuthenticationRequiredError`. No DCR/override bypass.

**3. [Info] B2 OIDC issuer + endpoint origin + redirect refusal — Introduced PASS**
File: `src/libsync/creds/oauth.cpp:51-62,726-753`, `postTokenRequest:485-488`, `verifyTokenIdentity UserInfo:279-288`
`pinnedEndpoint()`: valid + `https` + empty userinfo + no query/fragment + `host==issuer.host` + `port(443)==issuer.port(443)`.
`issuer` field checked exact, not just HTTPS. `authorization/token/userinfo` all must pass. All private QNAM reqs set `ManualRedirectPolicy`: webfinger, well-known, token POST, UserInfo GET. `successfulPinnedReply()` requires `NoError + HTTP 200`, so 301/302/manual-redirect fails. `updateDynamicRegistration` disabled when `_identity`, `oauthOverrideAuthUrl` ignored when `_identity`.

**4. [Info] B3 Loopback/state preserved, SSO not broken — Pre-existing retained**
File: `src/libsync/creds/oauth.cpp:320-360,840-855`
Random `127.0.0.1:0` + PKCE-S256 + `state` equality retained. `isUrlValid()` allows `https`/`oauthtest`; pinned auth endpoint is `https` so passes. Upstream SSO redirects occur in system browser via `QDesktopServices::openUrl`, not in pinned QNAM client. No renderer token transfer.

**5. [Info] B4 Token iss/aud/sub + fresh UserInfo binding — Introduced PASS**
File: `src/libsync/creds/oauth.cpp:261-288,404-460`
Requires `scope==openid` exact, `accessToken` non-empty, `token_type==Bearer`, `IdToken.isValid()`, `iss==issuer` exact, ULID `^[0-7][0-9A-HJKMNP-TV-Z]{25}$`, `aud.contains(clientId)`, continuity `(_idToken valid => sub equal)`, `pinnedEndpoint(_userInfoEndpoint)`. Then fresh `GET userinfo` with `DontAddCredentials + ManualRedirect + Bearer access` must return `200 + JSON NoError + sub==idToken.sub`. Only `verified==true` calls `setIdToken+finalize→result(LoggedIn)` → secure-store `persist`. Else `result(Error)`, no enrollment. `finalize(..., _identity?QUrl():messageUrl)` suppresses attacker-controlled `message_url`.

**6. [Info] B5 Refresh rotation/subject/UserInfo, MAS omits ID token handled — Introduced PASS**
File: `src/libsync/creds/oauth.cpp:936-993`
Any `error` string → `refreshError`. Then requires `200 + contains refresh_token + new non-empty + new!=old`. `candidate = enrolled; if contains id_token parse JWT else retain`. `verifyTokenIdentity` re-checks scope/iss/sub/aud + fresh UserInfo with new access. Failure → `refreshError(AuthenticationRequiredError)` → existing `HttpCredentials` rotation/logout fence, no persist. `AccountBasedOAuth::fetchWellKnown:863-885` still runs `CheckServerJob` but gated by same `identityProfileMatchesServer`; bypass impossible because subsequent pinned discovery uses `_serverUrl==driveOrigin`, not redirected URL.

**7. [Info] Resolver pin + TLS-exception refusal — Introduced PASS**
File: `src/gui/newwizard/jobs/resolveurljobfactory.cpp:44-68,129-132`
`url!=driveOrigin` refused with queued `CoreJob(nullptr)` before any `GET`, zero requests. Else `status.php` with `ManualRedirect`. Success only if `NoError + 200 + reply->url()==concat(driveOrigin,status.php)`, result is compiled `driveOrigin` (avoids slash-terminated inherited quirk). `sslErrors` early-return when pinned; no `TlsErrorDialog`/`caCertificateAccepted`. Test asserts `acceptedCa==0`.

**8. [Info] Theme outranks injected profile; prod/S2 separate binaries — Introduced PASS**
Files: `src/libsync/creds/oauth.cpp:237-240`, `resolveurljobfactory.cpp:36-38`, `oem/atum/OEM.cmake:5-20`, `oem/atum/atumtheme.h:9-14`, `src/libsync/theme.h:27-33`, `src/libsync/globalconfig.cpp:14-17`
`_identity(Theme::oauthIdentityProfile() ? Theme::... : move(identity))` in both factories. Unbranded `Theme::oauthIdentityProfile()->nullopt` retains legacy; Atum `AtumTheme` returns compiled `{driveOrigin,issuer,clientId,openid}`, `oidcEnableDynamicRegistration=false`, `oauthClientSecret` empty. `OEM.cmake` `FATAL_ERROR` unless `production|s2` with distinct origins/bundle IDs/executables/cred-service (`appName`). `GlobalConfig::serverUrl()` returns `driveOrigin` when pinned, ignoring prefs. `HttpCredentials::refreshAccessToken` constructs `AccountBasedOAuth` without explicit identity but inherits Theme pin — no bypass.

**9. [Info] Legacy unbranded behavior unchanged — Introduced PASS (no regression)**
Files: same guards `if(_identity)` throughout `oauth.cpp`, resolver, `httplogger.cpp`, `globalconfig.cpp`, `abstractcorejob.cpp:84-89`
When `nullopt`: `identityProfileMatchesServer true`, `verifyTokenIdentity true`, original `NoLessSafeRedirectPolicy`, redirect dialog, TLS dialog, DCR/override, `message_url`, HTTP logging all retained. `CoreJob nullptr` guard is backward-compatible. `theme.cpp:189-200` also fixes pre-existing `RichText` `%3→%2` bug.

**10. [Minor] Introduced robustness gaps, fail-closed not fail-open**
- `oauth.cpp:79` icon path now `:/client/%1/...arg(appName())`. Unbranded `OpenCloud` same as before; Atum `AtumDrive/Test` embedding not proven in supplied source; S2 builds but live UI not exercised (timeout acknowledged). Missing-proof: Atum `qrc` mapping. Availability only, empty icon still logs in.
- `oauth.h:result` default `token=` redacted in view; assumed `QString()` like `refreshToken`. Missing-proof, no evidence of secret default.
- Tests lack explicit pinned mutations for `token_type!=Bearer`, `aud mismatch`, `refresh error string`. Code fail-closed; coverage gap only. `testoauth.cpp:testPinnedIdentity/Refresh` covers 12+7+9 mutations + `unexpectedRequests==0` + `ManualRedirect` + `Bearer` + loopback `127.0.0.1`.
- `scope` omission (MAS returns no `scope`) would fail-closed (`""!=openid`). Safe direction; confirm MAS always echoes `openid` on grant+refresh.
- `CoreJob(nullptr)` callers: pinned foreign path uses `invokeMethod(job,...Queued)` with `job` as context — auto-cancels if deleted, safe. No evidence wizard dereferences `job->reply()` for resolver; `checkServerJob->reply()->error()` is `CheckServerJob`, always network-backed. Recommend grep audit before wizard reuse, no crash in supplied test.

**11. [Info] Pre-existing risks retained, not introduced**
- JWT signature not locally verified; trust via pinned HTTPS + fresh UserInfo `sub` agreement. Acceptable per stated model, same as upstream.
- TLS via system CA, no cert pinning; pinned path refuses custom-CA dialog but trusts system roots.
- Async `verifyTokenIdentity`/`webfinger` use `connect(...,this,...)`; early `OAuth` delete cancels callback (no UAF) but may hang without `result/refreshError` — pre-existing pattern.
- `FakePayloadReply::bytesAvailable=testutils/syncenginetestutils.cpp:723` fix (`+QNetworkReply::bytesAvailable()`) is test-only, correct per Qt.

**12. [Info] Explicitly out of scope, not claimed**
Root enrollment/inventory, serialized update handoff, full lifecycle status, `9f701e6@4.0.0` vs `9bf87d5@4.0.1` reconciliation, exact packaging, signing/notarization, two-Mac acceptance, production activation/G8. No source substitution for release pin. Custody 16 cases and OAuth/resolver 52 cases claimed passing; source-only review did not re-execute.

**Verdict: SHIP for this source checkpoint only, not launch completion.**
Pinned origin/issuer/client/scope/subject, redirect/TLS refusal, UserInfo binding, rotating refresh, log redaction, Theme-precedence, legacy preservation all evidence-backed with no introduced blocker. Remaining gates in (12) stay closed.
