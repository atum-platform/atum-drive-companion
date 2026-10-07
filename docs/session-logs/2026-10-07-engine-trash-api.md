# S4-9 pinned trash API evidence

Approved Plan4 section2 source research, session `plan4-drive-2026-10-06`. Read-only native exploration route `29fa3af9-b1ae-49b5-a011-f4cd653d3618` completed; feedback sent once. No host operation, server change, engine trash implementation/advertisement, production action or synthetic-realm run occurred.

## Exact source and ownership

Public OpenCloud `v7.2.4` resolves to `1770793f2657e153836c32d32dd6d256b8531d3d`. Source was inspected through the read-only GitHub API. The actual local ingress source is the Plan4 S4-2 platform worktree, not the older platform checkout. Its `services/drive-ingress/src/ingress.ts` lines355–390,429–462 and735–759 restrict DAV routes to the published personal space, validate MOVE destinations and refuse infinite-depth PROPFIND. A-HOST owns deployment/synthetic-realm/runtime evidence; C-DRV owns ingress/product server work. This lane does not change those surfaces.

## Verified protocol

The [pinned DAV trash handler](https://github.com/opencloud-eu/opencloud/blob/1770793f2657e153836c32d32dd6d256b8531d3d/vendor/github.com/opencloud-eu/reva/v2/internal/http/services/owncloud/ocdav/trashbin.go) lists with `PROPFIND Depth:1` under `/dav/spaces/trash-bin/<personalSpaceId>/`. It returns one207 containing all root entries; there is no server pagination/order guarantee. Properties include private href/key/original-location and safe presentation basename, deletion seconds/date, size/type. A future engine must bound response bytes/rows, validate all private hrefs remain under the enrolled own-space base, sort by checked deletion milliseconds, and create private page snapshots/cursors plus random process/account/root-bound item handles. No href, key, original path or token crosses stdout.

Restore uses `MOVE` from the private trash href to the original relative location under `/dav/spaces/<same personalSpaceId>/`. The public origin/space must match the existing account. `Overwrite: F` is mandatory: absent overwrite defaults true, and true may delete the existing destination. The handler returns412 for a destination found at its precheck; 201 for successful creation,404 for missing item and409 for missing parent. No delete/purge or replace operation is permitted by Plan4.

## Concrete server safety dependency

The [pinned POSIX restore implementation](https://github.com/opencloud-eu/opencloud/blob/1770793f2657e153836c32d32dd6d256b8531d3d/vendor/github.com/opencloud-eu/reva/v2/pkg/storage/fs/posix/trashbin/trashbin.go#L316) calls `os.Rename(trashPath, restorePath)` after the handler's separate Stat precheck. A concurrently created target can therefore be replaced. The decomposedfs path uses a symlink creation and may have different collision semantics; configuration comments identifying POSIX do not prove which restore path is actually live. `Overwrite:F` plus a client suffix/retry is not evidence of race-safe keep-both on the POSIX path. Root independently read the exact restore and handler code and confirmed this boundary.

Before engine `trash` may be advertised, A-HOST/C-DRV must establish the actual pinned runtime driver/path and provide a reviewed atomic no-replace/keep-both operation, with a disposable-realm test where a target is created after the precheck. Both source and restored files must survive with their bytes intact. This cannot be safely fixed by an engine-only check-then-MOVE sequence.

The pinned restore handler/POSIX code also provides no restore-specific quota check or507 guarantee. Defensive mapping of an actually received507 to `no_room` is possible, but it is not proof the required outcome exists. The server join needs a precise restore quota/accounting contract and real no-room evidence without changing the approved1GiB quota or168h retention. Other/unknown errors remain failed, never empty/success.

## Next executable source path

After the server guarantee is supplied, use a dedicated bounded authenticated DAV job and a private engine controller. Bind records/cursors/jobs to the existing validated account, owner, root generation and single personal space; invalidate on disconnect/forget/root loss. Serialize mutations, retire a handle after definite success, never retry an unknown write outcome automatically. Focused FakeAM CTest will verify own-space requests, explicit overwrite prevention, response validation/caps, pagination, cursor expiry, 201/404/412/507 and stale completions. FakeAM cannot prove server atomicity; the required loopback proof belongs to the approved replacement realm.

S4-8 complete exclusions and facts can proceed independently. This evidence closes the previously unverified trash API question; it does not complete S4-9 or full S4-7 Recently deleted acceptance.
