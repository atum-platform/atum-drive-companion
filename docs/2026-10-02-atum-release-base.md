# Companion release-base correction

The fork's root snapshot 9bf87d5 has VERSION 4.0.1 and product changes beyond
upstream v4.0.0. Upstream release metadata still identifies v4.0.0 at
9f701e6db2e101cb3247d3331647a60266e83b0f as the latest released base. The published
Atum identity/custody/root overlay through f84074a was tested against that newer
snapshot, so its source review is not a claim that the release pin was met.

This isolated correction restores product source, VERSION, translations, GUI
fixtures and the Craft dependency shelf from the released base, while retaining
the public Atum overlay and the fork's CI adaptations. Git's three-way merge
conflicted only in Folder and OAuth. Folder keeps Atum's enrollment-aware local
path check while dropping the unreleased webUrl property. OAuth keeps the pinned
issuer/HTTP-200 guards and identity override, and restores upstream v4.0.0 legacy
scope/prompt initialization for unbranded clients. One inherited whitespace error
in a GUI feature table is normalized. CI workflow files and Woodpecker files are
operator adaptations and intentionally are not restored with the product tree.

Verification on the released base: the branded build compiles with the pinned
Qt 6.11.1/LibreGraph 1.0.7/Keychain 0.16 SDK. The root chooser widget suite passes
9 cases, root binding 17, folder 8, exclusions 16, and focused root-loss sync
engine 3. An unbranded Debug beta build with assertions enabled passes all 52
OAuth cases, including the beta callback regression, and 16 real Cocoa Keychain
cases. Generic CTest passes 33 suites under offscreen Qt; the remaining watcher
suite requires the native Cocoa event dispatcher and passes all 11 cases there.
The offscreen failure was not changed in source or counted as a pass.

This checkpoint also carries the root chooser visibility and beta OAuth resource
fixes from df91cd7 and the synthetic request-body peek fix from 328ba91. The
v4.0.0 OAuth legacy initialization is retained. Product delta against v4.0.0
contains the known Atum overlay, operator CI/docs and one inherited whitespace
normalization. One independent released-base assembly review and exact-head
hosted CI remain gates. Live root acceptance and a signed, distributed release
remain separate; no runtime or release completion is inferred from these tests.
