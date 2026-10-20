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

## Failed-job rerun result

The coordinator performed the one allowed unchanged failed-job rerun. Attempt 2
completed on `2026-10-20` with Windows, Intel macOS and Linux green. Apple
Silicon job
[`118557215969`](https://github.com/atum-platform/atum-drive-companion/actions/runs/38369423758/job/118557215969)
failed at `2026-10-20T09:55:16Z`, after its generic build, 36 native tests and
portable package had succeeded. This is not another infrastructure retry case.

The H2 step first persisted these Craft blueprint arguments:

```text
args=-DOEM_THEME_DIR=.../oem/atum -DATUM_DRIVE_PROFILE=h2
```

It then invoked Craft with `--no-cache`. The pinned Craft core treats that flag
only as a binary-cache control; it does not ignore the installed-package record.
The complete job log therefore returned the decisive line immediately, without
running configure, make or install:

```text
*** opencloud/opencloud-desktop is up to date, nothing to do ***
```

The later proof correctly found no `AtumDriveH2` target output. The generic
build had produced only
`bin/OpenCloud Beta.app/Contents/MacOS/OpenCloud Beta`; its packaged copy lived
under the Craft archive/image tree. Widening the recursive search to those
trees would select the generic or a stale artifact and would not prove an H2
compile.

## Local correction and verification

The local recovery branch now forces the configured second build with the
pinned Craft `--ignoreInstalled` option while retaining `--no-cache`. Before
that build it removes only the exact expected H2 bundle in the target-specific
Craft build directory, so a no-op cannot pass on a stale executable. The proof
uses the deterministic CMake output
`bin/AtumDriveH2.app/Contents/MacOS/AtumDriveH2`, requires `lipo -archs` to be
exactly `arm64`, validates the generated H2 header and bundle plist, and runs
that executable's real `--atum-engine` hello/stop path. The current-run H2
archive is also deleted before packaging and addressed by its exact build-number
path instead of choosing the first or newest glob match.

A disposable fixture placed a valid H2 target at that exact output and a second
same-named stale candidate elsewhere in the build tree. The proof passed the
valid target with the expected closed identity, protocol and four features. It
then failed with `compiled H2 engine is missing from target output path` when
the exact target was temporarily absent, despite the distractor remaining.
Additional focused checks passed:

- the production/S2/H2 profile contract checker;
- Python parsing of the proof harness;
- YAML parsing of the workflow; and
- `git diff --check`.

No full local native build was attempted: only 1.5 GiB was available on the
Data volume, below the task's 3 GiB ceiling. No CI was triggered or rerun by
this worker. Hosted proof necessarily follows coordinator assembly, review and
push; the draft PR still points to unchanged head
`69f567e3016757c0cc946240d522bc34f66478e4`.

## Remaining acceptance

Do not infer a green result from the three successful attempt-2 jobs or an older
head. A future assembled PR head must execute and pass all four jobs before
merge readiness is considered.

The recovery does not close the acceptance gates retained in
`docs/session-logs/2026-10-07-engine-completion.md`:

- **Exact-head exclusions evidence:** attempt 2 ran the hosted build, formatting,
  analysis and full native suites successfully on PR head `69f567e301`; the
  workflow is nevertheless red at the H2 proof, and those results do not cover
  this local recovery change. The future assembled head needs its own complete
  checks.
- **Real binary and realm:** approved replacement-realm acceptance must exercise
  the compiled engine through folder-ready/stdout/accepted-only behavior. The
  prior H2 preparation receipts are not that result and do not authorize realm
  extension, release, signing or production activation.
- **Trash and quota:** engine trash remains unadvertised until the actual
  runtime proves atomic non-overwrite/keep-both restore behavior and the restore
  quota outcomes. No client-only restore fallback is accepted.

Application source and dependency pins remain unchanged. This follow-up changes
only the H2 workflow/build-proof path and this receipt; it does not alter engine
runtime behavior or weaken any live acceptance gate.
