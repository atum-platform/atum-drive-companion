# Atum companion integration checkpoint

This public GPL fork has reviewed secure-store acknowledgement and rotation/logout
fences. Atum needs one real OEM build for its dedicated public loopback OAuth client.
No proprietary implementation or credentials belong in this repository.

Parallel MECE workstreams:
1. Identity/OEM source: an optional Theme-level pinned identity profile with real
   Atum consumer; strict Drive origin, WebFinger subject/issuer/client/openid-only
   scope equality, pinned OIDC discovery and endpoint origin, no DCR, no token/code
   logs. Reuse the existing OAuth engine and native credential custody.
2. Root/lifecycle source: explicit canonical root enrollment and inventory; refuse
   foreign/ambiguous saved binding and suffix-folder creation; reuse upstream journal
   exclusion with serialized update handoff; expose non-secret account/root/dirty/
   conflict/auth/cleanup status. Depends on identity source and the built artifact.
3. Build/acceptance: public OEM assets and reproducible build, then local Developer
   ID signing/notarization on the existing MacBook without exporting its key. Full
   real two-Mac acceptance follows the reviewed build. Production activation remains
   a distinct governed operation.

The first implementation checkpoint is identity/OEM only, with real OAuth path
tests against controlled discovery before a live disposable sign-in. Root adoption
and lifecycle are subsequent dependent slices, not parallel changes.

Observed baseline: Theme provides client ID, dynamic registration switch, random
IPv4 loopback ports and hardcoded endpoint override, but no pinned discovery policy.
OAuth currently accepts WebFinger client/scopes/issuer overrides unconditionally;
its OIDC endpoints are HTTPS-checked but discovery issuer is not checked. Default
scope is openid offline_access email profile. Existing tests drive the real OAuth
state machine using FakeAM, captured browser URLs and a real loopback listener.
Existing OEM.cmake/THEME_CLASS is the brand extension surface. Keep default upstream
behavior when the optional profile is absent.

Proposed narrow design: optional immutable Theme profile containing exact Drive
origin, issuer, client and scopes; Atum build provides production profile, and an
explicit separate disposable-test build profile. Fail before browser grant if any
discovery fact is absent or differs. Discovery authorization/token/UserInfo URLs
must use the pinned issuer HTTPS origin; no user-entered server alternative or DCR.
Token response scopes and ID-token issuer/subject must match; verify UserInfo subject
before making the enrollment available to the secure-store acknowledgement path.
No renderer token transfer or invented OAuth engine. Debug logging must redact raw
callback codes/token replies before enabling the brand build.

Review the design for real trust-boundary gaps and an appropriately small vertical
slice. Specifically identify when endpoint pinning, redirect handling, token subject
checks and existing refresh flow must act. Do not propose a speculative policy engine.
