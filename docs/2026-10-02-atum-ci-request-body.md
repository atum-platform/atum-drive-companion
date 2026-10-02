# OAuth fixture request-body correction

Exact-head run 37029461402 built the arm64 macOS client, then aborted in
TestOAuth::testPinnedIdentity(accepted). The new synthetic token handler consumed
the outgoing QBuffer before FakeAM installed HttpLogger. With HTTP logging enabled,
the upstream logger correctly asserted that the request body must still be at
position zero. The other three platform jobs were cancelled by matrix fail-fast;
their check labels were not independent build failures.

Both new pinned-authentication and refresh fixtures now inspect the request with
peek instead of consuming it. This preserves the real request-body/logger contract
without disabling assertions or logging, and changes no product behavior.

The unbranded native OAuth suite passed all 52 Qt cases with HTTP logging enabled,
zero failures/errors/skips. Receipt: /private/tmp/atum-oauth-ci-fix-01a0fad5.xml.
The focused fix needs the next exact-head hosted run before merge. The released
4.0.0 base correction remains isolated on feat/drive-release-pin.
