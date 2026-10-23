#!/usr/bin/env python3
"""Focused macOS fixtures for the H2 package recipe and proof."""

from __future__ import annotations

import importlib.util
import json
import plistlib
import platform
import shutil
import subprocess
import tarfile
import tempfile
import unittest
from pathlib import Path


REPOSITORY = Path(__file__).resolve().parents[1]
PROFILE = REPOSITORY / "test/fixtures/atum-drive-profiles.json"
PROOF = REPOSITORY / ".github/workflows/prove-atum-engine-hello.py"
PATCHER = REPOSITORY / ".github/workflows/prepare-atum-h2-package.py"


def load_module(name: str, path: Path):
    spec = importlib.util.spec_from_file_location(name, path)
    if spec is None or spec.loader is None:
        raise RuntimeError(f"could not load {path}")
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


class BlueprintPatchTests(unittest.TestCase):
    def test_exact_anchors_enable_only_explicit_h2_packaging(self) -> None:
        patcher = load_module("prepare_atum_h2_package", PATCHER)
        source = "".join(
            (
                patcher.OPTION_ANCHOR,
                patcher.EXECUTABLE_ANCHOR,
                patcher.APP_NAME_ANCHOR,
            )
        )
        patched = patcher.patched_source(source)

        self.assertIn('registerOption("atumH2Package", False)', patched)
        self.assertIn('if self.subinfo.options.dynamic.atumH2Package:', patched)
        self.assertIn('"AtumDriveH2"', patched)
        self.assertIn('return "opencloud_beta" if self.subinfo.options.dynamic.buildBeta else "opencloud"', patched)

    def test_source_drift_fails_closed(self) -> None:
        patcher = load_module("prepare_atum_h2_package_drift", PATCHER)
        source = "".join(
            (
                patcher.OPTION_ANCHOR,
                patcher.EXECUTABLE_ANCHOR.replace("opencloud_beta", "changed"),
                patcher.APP_NAME_ANCHOR,
            )
        )

        with self.assertRaisesRegex(SystemExit, "exact application executable anchor"):
            patcher.patched_source(source)


@unittest.skipUnless(platform.system() == "Darwin" and platform.machine() == "arm64", "requires Apple Silicon macOS")
class PackagedEngineProofTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        required = ("clang", "codesign", "lipo", "otool")
        missing = [tool for tool in required if shutil.which(tool) is None]
        if missing:
            raise unittest.SkipTest(f"missing macOS developer tools: {', '.join(missing)}")

        cls.temporary = tempfile.TemporaryDirectory(prefix="atum-drive-h2-package-test-")
        cls.root = Path(cls.temporary.name)
        cls.bundle = cls.root / "AtumDriveH2.app"
        cls.executable = cls.bundle / "Contents/MacOS/AtumDriveH2"
        cls.frameworks = cls.bundle / "Contents/Frameworks"
        cls.executable.parent.mkdir(parents=True)
        cls.frameworks.mkdir(parents=True)

        contract = json.loads(PROFILE.read_text(encoding="utf-8"))
        h2 = contract["profiles"]["h2"]
        cls.header = cls.root / "atumprofile.h"
        cls.header.write_text(
            "\n".join(
                (
                    f'#define ATUM_DRIVE_ORIGIN "{h2["origin"]}"',
                    f'#define ATUM_MAS_ISSUER "{h2["issuer"]}"',
                    f'#define ATUM_OAUTH_CLIENT_ID "{contract["oauthClientId"]}"',
                    "",
                )
            ),
            encoding="utf-8",
        )
        with (cls.bundle / "Contents/Info.plist").open("wb") as handle:
            plistlib.dump(
                {
                    "CFBundleExecutable": "AtumDriveH2",
                    "CFBundleIdentifier": h2["bundleIdentifier"],
                },
                handle,
            )

        library_source = cls.root / "fixture.c"
        library_source.write_text("int atum_fixture_value(void) { return 7; }\n", encoding="utf-8")
        main_source = cls.root / "main.c"
        hello = json.dumps(
            {
                "features": ["exclusions", "progress", "quota", "sync"],
                "issuer": h2["issuer"],
                "kind": "hello",
                "origin": h2["origin"],
                "protocol": 1,
                "taskFiles": True,
            },
            separators=(",", ":"),
        )
        c_hello = hello.replace("\\", "\\\\").replace('"', '\\"')
        main_source.write_text(
            "\n".join(
                (
                    "#include <stdio.h>",
                    "extern int atum_fixture_value(void);",
                    "int main(void) {",
                    "    char input[64];",
                    "    if (atum_fixture_value() != 7 || fgets(input, sizeof(input), stdin) == NULL) return 2;",
                    f'    puts("{c_hello}");',
                    "    return 0;",
                    "}",
                    "",
                )
            ),
            encoding="utf-8",
        )

        cls.execute(
            "clang",
            "-arch",
            "arm64",
            "-dynamiclib",
            str(library_source),
            "-Wl,-install_name,@rpath/libatumfixture.dylib",
            "-o",
            str(cls.frameworks / "libatumfixture.dylib"),
        )
        cls.execute(
            "clang",
            "-arch",
            "arm64",
            str(main_source),
            f"-L{cls.frameworks}",
            "-latumfixture",
            "-Wl,-rpath,@executable_path/../Frameworks",
            "-o",
            str(cls.executable),
        )
        cls.execute("codesign", "--force", "--sign", "-", str(cls.frameworks / "libatumfixture.dylib"))
        cls.execute("codesign", "--force", "--deep", "--sign", "-", str(cls.bundle))

        cls.archive = cls.root / "atum-drive-h2-fixture.tar.xz"
        cls.write_archive(cls.archive, cls.bundle)
        cls.build_dir = cls.root / "build"
        shutil.copytree(cls.bundle, cls.build_dir / "bin/AtumDriveH2.app")

    @classmethod
    def tearDownClass(cls) -> None:
        cls.temporary.cleanup()

    @staticmethod
    def execute(*command: str) -> subprocess.CompletedProcess[str]:
        return subprocess.run(command, check=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)

    @classmethod
    def write_archive(cls, archive: Path, *bundles: Path) -> None:
        with tarfile.open(archive, mode="w:xz") as package:
            for bundle in bundles:
                package.add(bundle, arcname=bundle.name)

    def prove(self, archive: Path) -> subprocess.CompletedProcess[str]:
        return subprocess.run(
            [
                "python3",
                str(PROOF),
                "--profile",
                str(PROFILE),
                "--header",
                str(self.header),
                "--archive",
                str(archive),
                "--expected-architecture",
                "arm64",
            ],
            check=False,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            text=True,
        )

    def test_real_macho_bundle_dependency_signature_and_hello_pass(self) -> None:
        dependencies = "\n".join(self.execute("otool", "-L", str(self.executable)).stdout.splitlines()[1:])
        self.assertIn("@rpath/libatumfixture.dylib", dependencies)
        self.assertNotIn(str(self.root), dependencies)

        result = self.prove(self.archive)

        self.assertEqual(result.returncode, 0, result.stderr)
        receipt = json.loads(result.stdout)
        self.assertEqual(receipt["artifact"], "package")
        self.assertEqual(receipt["bundleIdentifier"], "com.atumplatform.drive.h2")
        self.assertEqual(receipt["result"], "pass")

    def test_build_output_proof_still_uses_the_exact_target(self) -> None:
        result = subprocess.run(
            [
                "python3",
                str(PROOF),
                "--profile",
                str(PROFILE),
                "--header",
                str(self.header),
                "--build-dir",
                str(self.build_dir),
            ],
            check=False,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            text=True,
        )

        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual(json.loads(result.stdout)["artifact"], "build")

    def test_old_generic_bundle_cannot_hide_beside_h2_bundle(self) -> None:
        generic = self.root / "OpenCloud Beta.app"
        shutil.copytree(self.bundle, generic)
        archive = self.root / "mixed-apps.tar.xz"
        self.write_archive(archive, self.bundle, generic)

        result = self.prove(archive)

        self.assertNotEqual(result.returncode, 0)
        self.assertIn("expected only AtumDriveH2.app", result.stderr)

    def test_tampered_package_signature_fails(self) -> None:
        tampered = self.root / "tampered/AtumDriveH2.app"
        shutil.copytree(self.bundle, tampered)
        with (tampered / "Contents/MacOS/AtumDriveH2").open("ab") as handle:
            handle.write(b"tampered")
        archive = self.root / "tampered.tar.xz"
        self.write_archive(archive, tampered)

        result = self.prove(archive)

        self.assertNotEqual(result.returncode, 0)
        self.assertIn("signature verification failed", result.stderr)


if __name__ == "__main__":
    unittest.main()
