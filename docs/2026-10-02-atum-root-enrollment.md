# Atum root enrollment continuation

This branch follows the reviewed identity/OEM checkpoint e16c00f (source 4ee0ead).
The identity branch remains separately reviewable and is waiting for explicit
public-source publication approval. No new root implementation is included in
that pending approval.

## Parallel MECE workstreams

1. Coordinator: root/space/binding/journal preservation in the GPL companion.
   Inventory, binding and ownership are ordered dependencies; test integration
   follows assembled code and is not parallel implementation.
2. Existing custody CI branch: only hosted test environment repair and validation;
   no identity/root source changes. Linux Secret Service preflight now passes at
   ef42a30, full four-platform checks remain running.
3. Platform acceptance branch: redacted live S2 byte/expiry evidence, already
   committed/pushed at 8e7853be7. It does not publish this companion source.

Native exploration decision 6dba8ae1-0b81-4da8-9312-d484440b2d7a enforced direct
work because Codex quota was exhausted; it completed and feedback closed. Root
design decision 79a4f2f6-d01d-47de-8f84-be4f32927bf2 routes OpenCode because the
architecture provider's quota was exhausted. Its bounded consultation receives
public upstream/fork source and generic root requirements, without credentials.
The result is advice and must be read before implementation decisions. The
coordinator owns one assembled source review after real I/O verification.

## Confirmed boundaries and next implementation

Upstream FolderMan::addFolder calls ensureJournalGone before constructing a
Folder; removeFolder calls wipeForRemoval, which deletes journal state. Initial
sync iterates all returned spaces and uses a suffix-search folder allocator.
These are not safe Atum resume, account change or personal-root semantics.

The first root slice will use exactly one personal space and the explicitly
enrolled root. It will refuse foreign/ambiguous bindings without wiping or
renaming, preserve an unavailable root as a visible error, and keep credential,
journal, runtime/database and purchased skill/source material excluded. Empty
fresh enrollment and matching-binding resume precede explicit nonempty inventory
adoption. Unknown nonempty roots must remain refused until that approval UX is
implemented; they must never be silently called accepted.

The real exclusive-journal proof already shows an owner prevents same-path and
symlink-alias access, but two simultaneous recovery waiters can remain mutually
busy after an owner crash. A canonical nonblocking owner guard before journal
opening/mutation is warranted for the Atum profile; its lifetime must extend
through shutdown and journal close. It must not become a generic policy layer.

State matrix: fresh empty -> explicit enrollment; matching binding -> resume;
foreign/corrupt/unknown nonempty -> refuse and preserve; owner busy -> visible
pause; missing/unreadable root -> unavailable, no deletion inference; OS credential
cleanup pending -> no sync; new identity -> distinct binding. Actual code and
verification are still pending. Lifecycle status/interface, explicit inventory
approval, release-pin reconciliation, packaging/signing, two-Mac proof and G8
remain open.
