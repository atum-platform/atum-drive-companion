# Atum identity and OEM checkpoint

This is the next dependent vertical slice after the custody PR. It adds an
immutable optional Theme identity profile with a real Atum OEM consumer. The
unbranded build retains the existing OAuth flow. Production and S2 are explicit
compile-time profiles with different application/bundle/credential services.
The public loopback client requests only openid; no dynamic registration or
preferences can substitute its Drive origin, issuer, client or scopes.

Enrollment checks exact WebFinger subject/issuer/client/scopes, OIDC issuer and
HTTPS endpoint origins, then ID-token issuer/audience/canonical MAS subject and
fresh pinned UserInfo agreement. Only verified credentials reach the already
reviewed secure-store acknowledgment path. Discovery, token and UserInfo HTTP
redirects are refused. Normal system-browser upstream SSO remains supported.
Refresh requires a new rotating credential, the same scope and subject, and
fresh UserInfo. MAS's actual S2 refresh omits a new ID token, so enrollment
metadata is retained and checked against the fresh access token's UserInfo.
Failures retain the existing rotation/logout fence and queued files.

The inherited server resolver returns a slash-terminated URL even without a
redirect; that would fail exact-origin enrollment. The pinned resolver keeps
the compiled Drive origin, refuses foreign input before a network call, and
refuses redirects/TLS exceptions. A locally refused CoreJob can therefore have
no network reply; its existing error/result completion semantics are preserved.

Atum HTTP request/body logging is disabled in the logger regardless of debug
preferences or environment. Raw OAuth loopback codes and ID-token payload logs
were removed. The OEM contains original CC0 generated monogram assets, public
source/build instructions and GPL notices. VFS, auto-update, crash uploading
and update notification are disabled for this explicit profile.

Actual initial verification passed 46 OAuth cases with zero failures/skips:
legacy OAuth plus pinned grant and rotating refresh state machines, including
real loopback HTTP. The refresh path exposed a FakePayloadReply fixture bug:
bytesAvailable ignored Qt's buffered peek bytes, so CheckServerJob saw a falsely
empty response after logging. The fixture now includes the base reply buffer.
Final verification adds empty enrollment refresh-token refusal and five pinned
wizard-resolution cases: 52 OAuth/resolver cases pass, zero failures/skips. The
native custody suite also passes all 16 cases, zero failures/skips. The separate
AtumDriveTest S2 application builds successfully. Computer control timed out
while opening it; this is not a live UI/sign-in or packaged acceptance receipt.
Two newly added assertions initially failed due to test setup (a fake TLS reply
never emitted completion and an empty token wrongly expected UserInfo). Both
were corrected; production refusals were unchanged.

Native builds use the official Craft SDK (Qt 6.11.1, QtKeychain 0.16.0), macOS
minimum 14, RelWithDebInfo and the explicit S2 profile. Compilation is limited
to two jobs: simultaneous unrestricted rebuilds caused excessive swap on the
nearly-full internal Mac disk. Only task-owned build trees were interrupted;
public dependencies/builds stay on the owned external APFS image.

The fork baseline 9bf87d5 reports version 4.0.1; the frozen launch contract's
official Desktop proof pins 4.0.0 at 9f701e6. Candidate identity/OEM source is
not silently substituted for that release pin. Release reconciliation, exact
source packaging, signing/notarization and two-Mac acceptance remain gates.
This checkpoint also does not claim canonical root adoption/inventory, serialized
update handoff, complete lifecycle status, production activation or G8 closure.

Routing: identity architecture job ecd6ca8e-8d2a-4b9a-ae3b-6fa32a62dcbc and
decision 440dd75a-bf69-4d40-aaa7-47015f8674cb were read/acknowledged/closed.
It used supplied context only because provider filesystem access was denied;
its advisory is not source approval. OEM implementation decision
929a43fb-5be9-4b7b-8def-f2dec04762ef enforced direct work. Root exploration
ac834e8e-b9b1-4d1a-a25c-febaedcb5ca8 also enforced direct because Codex quota
was exhausted, and is closed. One assembly source review follows final focused
verification; it receives explicit source files rather than depending on direct
provider filesystem reads.
