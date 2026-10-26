#!/usr/bin/env python3
"""Pure contract tests for the unsigned Atum engine assembly recipe."""

from __future__ import annotations

import hashlib
import json
import os
import shutil
import subprocess
import tempfile
import unittest
from pathlib import Path


REPOSITORY = Path(__file__).resolve().parents[1]
SCRIPTS = REPOSITORY / "scripts"
ASSEMBLE = SCRIPTS / "assemble-atum-engine-unsigned.sh"
VERIFY = SCRIPTS / "verify-atum-engine-bundle.py"
INVENTORY = SCRIPTS / "inventory-atum-engine-bundle.py"
HELLO = SCRIPTS / "hello-stop-atum-engine.py"
BUILDING = REPOSITORY / "BUILDING.md"
FIXTURE = REPOSITORY / "test/fixtures/atum-drive-engine-0.20.17-source-offer.json"
FIXTURE_02019 = REPOSITORY / "test/fixtures/atum-drive-engine-0.20.19-source-offer.json"


class UnsignedRecipeContractTests(unittest.TestCase):
    def setUp(self) -> None:
        self.fixture = json.loads(FIXTURE.read_text(encoding="utf-8"))
        self.fixture_02019 = json.loads(FIXTURE_02019.read_text(encoding="utf-8"))
        self.recipe = ASSEMBLE.read_text(encoding="utf-8")
        self.building = BUILDING.read_text(encoding="utf-8")

    def test_recipe_is_path_parameterized_and_never_signs(self) -> None:
        self.assertNotIn("/Users/", self.recipe)
        self.assertNotIn("/private/tmp/", self.recipe)
        for argument in (
            "--build-dir",
            "--source-dir",
            "--sdk-dir",
            "--output-dir",
            "--compiled-source",
            "--reviewed-head",
            "--source-tag",
        ):
            self.assertIn(argument, self.recipe)
        self.assertIn("-no-codesign", self.recipe)
        self.assertIn("-no-plugins", self.recipe)
        self.assertIn("install_name_tool -delete_rpath", self.recipe)
        self.assertNotIn("codesign --force", self.recipe)
        self.assertNotIn("codesign --sign", self.recipe)
        self.assertNotIn("--sign -", self.recipe)
        self.assertIn("status=not_run_unsigned_bundle_requires_signature", self.recipe)

    @unittest.skipUnless(shutil.which("bash"), "requires bash for syntax/help checks")
    def test_recipe_shell_syntax_and_help(self) -> None:
        subprocess.run(["bash", "-n", str(ASSEMBLE)], check=True)
        result = subprocess.run(
            [str(ASSEMBLE), "--help"], check=True, text=True, stdout=subprocess.PIPE
        )
        self.assertIn("output directory must not exist", result.stdout)
        self.assertIn("never reads signing identities", result.stdout)

    def test_python_helpers_compile(self) -> None:
        for path in (VERIFY, INVENTORY, HELLO):
            compile(path.read_text(encoding="utf-8"), str(path), "exec")

    def test_extra_executable_dependencies_are_rewritten(self) -> None:
        self.assertIn("extra-executable-dependencies-rewritten.tsv", self.recipe)
        self.assertIn('for executable in "$WORK_APP"/Contents/MacOS/*', self.recipe)
        self.assertIn('"$WORK_APP/Contents/MacOS/AtumDrive" && continue', self.recipe)
        self.assertIn("@loader_path/../Frameworks/$library", self.recipe)
        self.assertIn(
            '/usr/bin/install_name_tool -change "$dependency" '
            '"@loader_path/../Frameworks/$library" "$executable"',
            self.recipe,
        )
        self.assertLess(
            self.recipe.index('"$SDK_DIR/bin/macdeployqt" "$WORK_APP"'),
            self.recipe.index("extra-executable-dependencies-rewritten.tsv"),
        )

    def test_known_inventory_contract_is_retained(self) -> None:
        verifier = VERIFY.read_text(encoding="utf-8")
        for plugin in self.fixture["plugins"]:
            self.assertIn(Path(plugin).name, self.recipe)
            self.assertIn(Path(plugin).name, verifier)
        self.assertIn("FinderSyncExt.appex", verifier)
        self.assertIn(self.fixture["unsignedBinarySha256"], self.building)
        self.assertIn(self.fixture["signedBinarySha256"], self.building)
        self.assertIn(self.fixture["runtimeBundleSha256"], self.building)
        self.assertIn(self.fixture["unsignedBundleSha256"], self.building)
        self.assertIn(f"{self.fixture['machoCount']} arm64 Mach-O", self.building)
        self.assertIn(f"{self.fixture['inventoryEntries']} canonical", self.building)

    def test_02019_source_offer_record_is_exact(self) -> None:
        self.assertIn(self.fixture_02019["futureSourceTag"], self.building)
        self.assertIn(self.fixture_02019["compiledSource"], self.building)
        for key in (
            "unsignedBinarySha256",
            "signedBinarySha256",
            "runtimeBundleSha256",
            "unsignedBundleSha256",
        ):
            self.assertIn(self.fixture_02019[key], self.building)
        self.assertIn(f"{self.fixture_02019['machoCount']} arm64 Mach-O", self.building)
        self.assertIn(
            f"{self.fixture_02019['inventoryEntries']} canonical", self.building
        )

    def test_lgpl_assets_stay_on_stable_source_release(self) -> None:
        stable = self.fixture["lgplSourceRelease"]
        self.assertEqual(stable, "drive-engine-0.18")
        for asset in self.fixture["lgplSourceAssets"]:
            self.assertIn(asset, self.building)
        for tag in ("drive-engine-0.20.17", "drive-engine-0.20.19"):
            self.assertNotIn(f"releases/download/{tag}/qt", self.building)
            self.assertNotIn(f"releases/download/{tag}/glib", self.building)
            self.assertNotIn(f"releases/download/{tag}/gettext", self.building)

    def test_inventory_hash_is_sorted_and_relation_based(self) -> None:
        with tempfile.TemporaryDirectory(
            prefix="atum-unsigned-inventory-"
        ) as temporary:
            bundle = Path(temporary) / "AtumDriveEngine.app"
            contents = bundle / "Contents"
            contents.mkdir(parents=True)
            (contents / "b.txt").write_bytes(b"b")
            (contents / "a.txt").write_bytes(b"a")
            os.symlink("a.txt", contents / "link")
            first = Path(temporary) / "first.jsonl"
            second = Path(temporary) / "second.jsonl"
            first_hash = subprocess.check_output(
                [str(INVENTORY), str(bundle), str(first)], text=True
            ).strip()
            second_hash = subprocess.check_output(
                [str(INVENTORY), str(bundle), str(second)], text=True
            ).strip()
            self.assertEqual(first_hash, second_hash)
            self.assertEqual(first.read_bytes(), second.read_bytes())
            lines = first.read_text(encoding="utf-8").splitlines()
            self.assertEqual(
                [json.loads(line)[0] for line in lines],
                ["Contents/a.txt", "Contents/b.txt", "Contents/link"],
            )
            digest = hashlib.sha256()
            for line in lines:
                digest.update((line + "\n").encode())
            self.assertEqual(first_hash, digest.hexdigest())


if __name__ == "__main__":
    unittest.main()
