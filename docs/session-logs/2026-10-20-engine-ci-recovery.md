# Drive exact-head CI recovery receipt

This receipt covers draft engine PR
[#4](https://github.com/atum-platform/atum-drive-companion/pull/4) at exact
head `69f567e3016757c0cc946240d522bc34f66478e4`. The isolated recovery
worktree is `/Users/NM/Projects/atum-drive-companion-worktrees/ci-recovery-1020`
on branch `fix/engine-ci-recovery`. The older primary-checkout revision
`e9836a471cb89164636cf3dafa40b2ab1106b057` is not current-head evidence.

## Failure classification

GitHub run
[`38369423758`](https://github.com/atum-platform/atum-drive-companion/actions/runs/38369423758)
was created for PR head `69f567e3016757c0cc946240d522bc34f66478e4`.
The hosted jobs checked out GitHub's merge commit `1bdd4e31dfdec53b2c4770f3c3b8a49dfccc3dc0`,
which merges that head into base `489d9312238ade24230224bfdf0e963f57f0e805`.
All four matrix jobs failed before dependency setup, compilation, formatting or
tests:

- Windows 2022 and both macOS targets reached `Clone CraftMaster`; each request
  to `https://invent.kde.org/kde/craftmaster.git` returned HTTP 502 between
  `2026-10-13T08:28:08Z` and `2026-10-13T08:28:21Z`.
- Linux failed while GitHub initialized its job container, before checkout. It
  retried the exact pinned image manifest three times between
  `2026-10-13T08:28:01Z` and `2026-10-13T08:28:15Z`; every registry request
  returned HTTP 502.
- `fail-fast: false` behaved as intended: it retained all four independent
  failures. The runner images were Windows 2022 `20261004.326.1`, macOS 26
  arm64 `20260907.0351.1` for both Craft targets, and Ubuntu 24.04
  `20261004.327.1`.

The simultaneous Git and registry 502 responses are a transient KDE upstream
availability failure, not a broken repository URL, missing image tag/digest or
authentication failure. No source or CI configuration change is justified.
In particular, Linux already uses the immutable manifest digest proven by the
preceding successful run; a workflow step cannot retry a job-container pull
that happens before any step starts.

## Read-only recovery evidence

At `2026-10-20 16:25:24 +07` (`2026-10-20 09:25:24 UTC`), metadata-only live
probes found:

- `git ls-remote https://invent.kde.org/kde/craftmaster.git HEAD` succeeds at
  `8a19d8fcab223b8b27126d98a46ed824b5948665`.
- The registry returns the normal anonymous Bearer challenge, then HTTP 200 for
  `invent-registry.kde.org/sysadmin/ci-images/craft-appimage-alma9` at
  `sha256:41dd698b4c6c4035ca5216077170006642712f59dab27cbb3dd41e814af8448b`.
  The response's `Docker-Content-Digest` is the same value. No layers were
  pulled.
- All exact shelf revisions remain addressable: KDE Craft blueprints
  `11b07503ab24e84affc5ed9c94093f8b03b05fd4`, OpenCloud blueprints
  `4b5f539521b7a68d51417faffe5278694a68996f`, and Craft core
  `b80e8f8c6c4f9fc8c09464dc8b696bfa625fa12c`.
- The live PR and `refs/pull/4/head` both still resolve to
  `69f567e3016757c0cc946240d522bc34f66478e4`; no newer check supersedes run
  `38369423758`.

## Parent action and remaining acceptance

The coordinator should perform exactly one failed-job rerun of the unchanged
run now that both KDE endpoints have recovered:

```sh
gh run rerun 38369423758 --failed --repo atum-platform/atum-drive-companion
```

Do not infer a green result from an older head. The rerun must execute and pass
all four jobs for the current PR head before merge readiness is considered.

The recovery does not close the acceptance gates retained in
`docs/session-logs/2026-10-07-engine-completion.md`:

- **Exact-head exclusions evidence:** hosted build/format/full CTest must run,
  including the completed native exclusion/protocol coverage on the current
  source. This failed run executed none of it.
- **Real binary and realm:** approved replacement-realm acceptance must exercise
  the compiled engine through folder-ready/stdout/accepted-only behavior. The
  prior H2 preparation receipts are not that result and do not authorize realm
  extension, release, signing or production activation.
- **Trash and quota:** engine trash remains unadvertised until the actual
  runtime proves atomic non-overwrite/keep-both restore behavior and the restore
  quota outcomes. No client-only restore fallback is accepted.

No application source, dependency pin, workflow behavior or test was changed in
this recovery branch; this receipt is the only repository delta.
