# Internal Atum Drive engine

The owner's 2026-10-02 ruling requires one canonical Atum app and siderail. This
repository supplies its internal sync engine; a standalone donor window or
installer is not product acceptance. The released 4.0.0 base carries the reviewed
Atum identity/custody/root overlay and corrected wizard retry lifecycle.

`--atum-engine` starts a background Qt event loop and bounded newline-delimited
JSON over private stdin/stdout. Protocol 1 says hello with fixed OEM issuer/origin;
start accepts only verified native owner issuer/subject and native-owned root and
private config directory. OAuth URL output goes only to native main. Bearers stay
in engine memory/Keychain and never enter this protocol or renderer. Actual sync
states come from the single enrolled personal folder. EOF/stop disables sync and
unloads it. Forget/cleanup removes custody and acknowledges only completed local
secure deletion; cleanup does not require the local root to remain available.

The engine records the owner/account before a first secure write, so interruption
can recover cleanup. Foreign identities, roots, multiple accounts/spaces, malformed
frames and pinned-profile mismatch fail closed. Existing donor widgets and wizard
paths remain for development/regression; Atum launches only the internal mode.
Package its app bundle with LSUIElement=true, include the entire dependency/resource
hash and source commit in the app-owned manifest, and keep its signature intact
while signing the outer Atum app. Release requires Developer ID, licensing/source
notices, exact-source checks and the integrated Atum app's own acceptance.

The local S2 engine/native adapter downloaded eight synthetic files at 18:11 UTC,
using fresh Drive OAuth for disposable B. This predates the final cleanup changes,
uses a harness-selected root and ad-hoc signing, and is not signed distribution or
canonical app UI acceptance. Source build passed; root binding/folder checks passed.
The pinned OEM general OAuth run failed and must be run in generic Debug/Beta;
seven real Keychain checks failed while the Mac was locked. Do not report them
passing. Generic retry verification and the final app sign-in/disconnect/root-loss
path remain pending. Assembly OpenCode review fe795a9e failed on upstream overload;
no independent ship approval was produced. Preserve the checkpoint and do not
release until a usable review and the remaining acceptance gates are complete.
