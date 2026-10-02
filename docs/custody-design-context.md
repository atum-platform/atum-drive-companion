# Focused credential custody design context

User asked to finish cloud agents/Atum Drive/cloud execution. This is the existing
public GPL companion fork, independent of proprietary Atum. Platform S2 has real
PKCE/openid/refresh and two-human exact file proofs, but unchanged upstream Desktop
logs out/sets ready without acknowledging async secure-store write/delete results.
Production/paid activation is outside this checkpoint.

Frozen DC-2: refresh token only in OS credential store. Enrollment/rotation is not
durably ready until secure-store write succeeds. Serialize rotation. Failed/lost
write pauses new sync with edits/journal retained, no plaintext fallback or
resurrection of consumed tokens. Recover through proved MAS tolerance or fresh
browser sign-in, never unlimited replay.
Frozen DC-4: sign-out/account switch stops new sync immediately, keeps local
files, acknowledges secure-store deletion before claiming removal. Failure stays
visible/pending, forbids another account reusing the binding, leaves files/dirty
journal recoverable. Issued writes may finish and must be reconciled.

Confirmed paths: HttpCredentials::refreshFinished sets ready/access then void
persist; HttpCredentialsGui::asyncAuthResult likewise; constructor for enrollment
starts ready; SetupWizardAccountBuilder calls persist asynchronously.
CredentialManager::remove deletes credential index before DeletePasswordJob
result, and failure only logs. AccountState::signOutByUi immediately SignedOut.

Proposed narrow approach for critique:
- Existing credential-index QSettings keeps no token data. Add explicit persisted
pending-write/rotation and pending-delete markers there, flushed and checked
before consumption of a refresh token/deletion; failures fail closed.
- Keep pending delete keys until confirmed NoError/EntryNotFound, deduplicate
concurrent delete jobs and deny reads of pending entries. Do not add a parallel
credential storage backend.
- HTTP ready/access unavailable during write or cleanup. Treat an interrupted
rotation as fresh sign-in required rather than replaying an indexed consumed RT.
Only publish fetched/ready after keychain success. Existing OAuth object remains
single in-flight; callbacks carry a generation fence to stop logout/write races.
- Persist logout state before async cleanup; reconnect is disabled while cleanup
pending, with visible pending/failure status and a bounded user-triggered retry.
Keep existing SignedOut sync gate rather than a speculative policy manager.
- Fresh sign-in/enrollment must wait for secure-store success too, preserving the
existing HTTP/model tool narrow waist and not changing OAuth scopes/client.

Please inspect the exact listed sources and advise the smallest correct plan,
with state matrix and concrete tests. No implementation, no launch/source approval
verdict, no nested reviewers, no runtime/environment/keychain or SDK directory reads.
