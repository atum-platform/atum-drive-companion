Job ecd6ca8e-8d2a-4b9a-ae3b-6fa32a62dcbc; route 440dd75a-bf69-4d40-aaa7-47015f8674cb. Read before implementation. Advisory source inspection was denied by the provider sandbox; the coordinator verifies seams locally. This is not a source approval.

# Identity/OEM Checkpoint — Focused Architecture Consultation

**Scope:** Identity/OEM only. Planning advice, no edits, no secrets accessed.
**Basis:** Caller-supplied `docs/atum-integration-design-context.md` only. Direct filesystem inspection of GPL Theme/OAuth/OEM source was denied by sandbox (`external_directory: deny`), so `file:line` pointers below are **caller-asserted seams to confirm**, not independently verified.

## 1. Recommendation: one immutable optional Theme profile, fail-closed before grant

Add a single optional immutable `Theme`-level identity profile, e.g.:

```text
struct PinnedIdentity {
  exact DriveOrigin;
  exact Issuer;      // https origin + path, string-equality
  exact ClientID;   // Atum public loopback client
  exact Scopes;     // openid-only set, e.g. `openid` (+ offline_access iff pinned)
}
```

Rules:

* Absent → keep exact current upstream behavior. No regression.
* Present → strict mode. Any absent/mismatched discovery fact = fail **before opening browser grant**. No fallback to user-entered server, no DCR, no hardcoded endpoint override, no dynamic-registration switch.
* Atum build supplies production profile; separate explicit disposable-test build supplies test profile. Never both in one binary. Compile-time constants, no runtime mutation, no prefs override.
* Reuse seams caller cites: existing OAuth engine + native credential custody, existing `OEM.cmake / THEME_CLASS` brand surface, existing real-OAuth tests (`FakeAM + captured browser URL + real loopback listener`).

Do not build a policy engine. This is 4 string-equality checks at 3 existing choke points.

## 2. Concrete trust boundaries

| # | Boundary | Untrusted input | Enforcement |
|---|---|---|---|
| B1 | WebFinger/discovery → client | `subject, issuer, client_id, scopes, OIDC metadata` | Exact equality vs pinned profile. Fail closed if missing/differs. Check `issuer` field itself, not just that endpoints are HTTPS — this is the gap called out in baseline. |
| B2 | Discovery endpoints → browser/token code | `authorization_endpoint, token_endpoint, userinfo_endpoint, jwks_uri` | Must be `https://` **and** origin == pinned issuer origin. Validate before browser launch **and** before token POST. Disable/fail on cross-origin redirect from those URLs. |
| B3 | Browser loopback → app | `code, state, redirect_uri, port` | Keep existing random IPv4 loopback + `state` check. `redirect_uri` must be constructed loopback only; reject foreign `redirect_uri` or `code` delivered to wrong port. No renderer token transfer. |
| B4 | Token/UserInfo → enrollment | `scope, id_token.iss/sub, userinfo.sub` | `scope` response == pinned (no silent broadening/narrowing). `id_token.iss == pinned issuer`, `id_token.sub == userinfo.sub`. Only then expose enrollment to secure-store acknowledgement path. |
| B5 | Refresh → continued access | `refreshed scope/iss/sub, token endpoint` | Refresh must reuse pinned token endpoint origin + same client. Re-check `scope/iss/sub` on refresh; mismatch → stop, require re-auth, propagate auth-failure to secure-store/logout fence. Do not persist broadened scopes. |
| B6 | Logs/renderer/build → secrets | `code, code_verifier, refresh/access tokens, id_token` | Redact raw callback `code` and token replies in debug logs **before** enabling brand build. This is a ship-blocker, not polish. |

## 3. Blockers mapped to your five questions

1. **Missing/mismatched discovery:** Baseline accepts WebFinger client/scopes/issuer overrides unconditionally and does not check discovery `issuer`. Blocker: implement `CheckPinnedDiscovery(fetched) -> fail` before any `OpenBrowser()`. Check all five: Drive origin, `issuer`, `client_id`, `scopes` exact set, endpoint origins.
2. **Endpoint redirects:** Baseline only HTTPS-checks endpoints. Blocker: add origin-pin + no-cross-origin-redirect for auth/token/userinfo. Specifically: resolve discovery via pinned issuer URL only; reject if `authorization_endpoint` origin != issuer origin.
3. **Token subject checks:** Baseline has no stated `sub` binding to enrollment. Blocker: require `id_token.iss/sub` + `userinfo.sub` equality before enrollment is marked available. Mismatch = discard tokens, no secure-store write.
4. **Refresh propagation:** Existing refresh flow will otherwise bypass one-time grant checks. Must act on every refresh: same endpoint/client/scope/sub check; surface failure to existing rotation/logout fences already reviewed.
5. **Debug secret logging:** Baseline risk of `code`/token-reply logs via existing OAuth debug. Must act first: central redaction helper, audit `FakeAM`/verbose OAuth logs, gate brand build on it. No full token/code in logs, crash dumps, or renderer messages.

## 4. Minimal vertical implementation / test sequence

Keep in order; each step is testable without the next:

1. **Redact + lock seams:** Add log redaction for `code/token/id_token` responses. Add test: trigger OAuth debug path, assert no raw secrets in output. Gate for all below.
2. **Profile struct + OEM wiring:** Add optional immutable struct to Theme; wire `OEM.cmake/THEME_CLASS` for Atum-prod vs disposable-test. Test: default build (absent) behaves unchanged; pinned build exposes profile immutably.
3. **Pre-grant pin check:** Insert guard in OAuth start path after discovery fetch, before browser open: exact match on `issuer/client/scopes/DriveOrigin + endpoint-origin`. Tests on real state machine with controlled discovery (extend `FakeAM`): missing field, wrong issuer, wrong client, extra scope, http endpoint, cross-origin endpoint → all refuse browser launch.
4. **Token/UserInfo binding:** After code exchange, check `scope` equality, `iss`, `sub==userinfo.sub`. Tests: swapped `sub`, wrong `iss`, broadened `scope` → discard + no enrollment signal to secure-store.
5. **Refresh binding:** Apply same checks to refresh path + propagate failure to auth status. Test: refresh returns different `scope/sub` or different endpoint origin → auth-invalid, no silent persist.
6. **Controlled-discovery path test, then live disposable:** Run 2–5 against local pinned discovery stub; only then one live disposable sign-in with test profile. Production profile sign-in is **not** this checkpoint.

## 5. Explicitly out of scope (dependent, not parallel)

* Root/lifecycle: canonical root enrollment/inventory, foreign-binding refusal, suffix-folder refusal, journal exclusion handoff, account/root/dirty/conflict/auth/cleanup status. Depends on this checkpoint + built artifact.
* Build/acceptance: public OEM assets, reproducible build, local Developer ID sign/notarize without key export, two-Mac acceptance, production activation as governed operation.

**Exit criteria for identity/OEM checkpoint:** default behavior unchanged when profile absent; pinned build fails closed on all B1–B6 mismatches with no browser/token/logs leak; refresh mismatch invalidates; disposable-test path test passes before any live sign-in.

