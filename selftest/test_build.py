#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
"""Shared build metadata, Make dispatch and explicit runner builds."""

import contextlib
import io
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import textwrap
import unittest
from unittest.mock import patch

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "lib/python"))
from villain import appliance  # noqa: E402
from villain.build import BuildSpec, KernelResult  # noqa: E402


@unittest.skipUnless(shutil.which("make") and shutil.which("gcc"), "Make and GCC required")
class BuildTests(unittest.TestCase):
    def setUp(self):
        temporary = tempfile.TemporaryDirectory()
        self.addCleanup(temporary.cleanup)
        self.root = Path(temporary.name) / "suite"
        (self.root / "lib").mkdir(parents=True)
        (self.root / "main.c").write_text(
            '#include <stdio.h>\n#include "generated.h"\n'
            'int main(void) { puts("EX001\\t" DESCRIPTION); return 0; }\n')
        (self.root / "lib/suite.py").write_text(textwrap.dedent('''\
            from pathlib import Path
            from villain.appliance import Suite
            from villain.build import BuildSpec, KernelResult
            ROOT = Path(__file__).resolve().parents[1]
            def prepare(context):
                if (ROOT / "fail").exists():
                    raise RuntimeError("Fixture preparation failed")
                context.directory.mkdir(parents=True, exist_ok=True)
                (context.directory / "generated.h").write_text(
                    '#define DESCRIPTION "Example"\\n')
                with (ROOT / "events").open("a") as stream:
                    stream.write("prepare\\n")
            def kernel(context):
                image = Path(context.variables.get(
                    "KERNEL_DEST", context.target / "produced-kernel")).resolve()
                image.parent.mkdir(parents=True, exist_ok=True)
                image.write_bytes(b"fixture kernel")
                with (ROOT / "events").open("a") as stream:
                    stream.write("kernel\\n")
                return KernelResult(image, "x86_64", "bzImage")
            class ExampleSuite(Suite):
                def list_tests(self, binary):
                    return {"EX001": "Example"}
            def create_suite():
                spec = BuildSpec(
                    root=ROOT, sources=lambda root: [root / "main.c"],
                    directory=Path("guest"), cc="gcc", prepare=prepare,
                    cppflags=lambda context: [f"-I{context.directory}"],
                    kernel=kernel)
                return ExampleSuite("example", "example-v1", Path("unused"),
                                    Path("unused"), build_spec=spec)
            '''))
        (self.root / "Makefile").write_text(
            ".DEFAULT_GOAL := initramfs\nSUITE := $(CURDIR)\n"
            f"include {ROOT}/lib/suite.mk\n")

    def make(self, *goals, public=False, **kwargs):
        command = ["make", "--no-print-directory", "-C",
                   str(ROOT if public else self.root)]
        if public:
            command.append(f"SUITE={self.root}")
        return subprocess.run([*command, *goals], text=True, capture_output=True,
                              check=True, timeout=30, **kwargs)

    def test_both_make_directions_share_outputs_and_metadata(self):
        self.make("init", public=True)
        suite = appliance.load_suite(self.root)
        self.assertEqual(suite.binary, self.root / "target/guest/init")
        self.assertTrue(suite.binary.is_file())
        self.assertFalse(suite.initramfs.exists())
        self.make("initramfs")
        self.assertTrue(suite.initramfs.is_file())
        self.assertEqual(appliance.catalog(suite.binary), {"EX001": "Example"})

    def test_parallel_goals_prepare_once_and_do_not_build_kernel_implicitly(self):
        result = self.make("-j8", "init", "initramfs")
        self.assertNotIn("jobserver unavailable", result.stderr)
        self.assertEqual((self.root / "events").read_text().splitlines(), ["prepare"])
        self.assertTrue((self.root / "target/guest/initramfs.cpio.gz").is_file())
        self.assertFalse((self.root / "target/produced-kernel").exists())

    def test_default_goal_packages_archive_from_either_direction(self):
        for public in (False, True):
            with self.subTest(public=public):
                self.make(public=public)
                self.assertTrue((self.root / "target/guest/initramfs.cpio.gz").is_file())

    def test_custom_output_and_kernel_action_agree_in_both_directions(self):
        for public in (False, True):
            with self.subTest(public=public):
                self.make("initramfs", "kernel", "TARGET=custom", public=public)
                suite = appliance.load_suite(self.root)
                args = appliance.parse_args(suite, ["--build-dir", "custom"])
                self.assertEqual(args.binary, self.root / "custom/guest/init")
                self.assertEqual(args.initramfs, self.root / "custom/guest/initramfs.cpio.gz")
                self.assertEqual(args.kernel, self.root / "custom/produced-kernel")
                self.assertFalse((self.root / "target").exists())

    def test_explicit_compiler_and_flag_overrides_are_forwarded(self):
        result = self.make("init", "CC=gcc", "CFLAGS=-static -O0 -Wall -Werror", "V=1")
        self.assertIn("gcc -std=gnu23 -static -O0 -Wall -Werror", result.stdout)

    def test_invalid_compiler_produces_a_failing_build(self):
        with self.assertRaises(subprocess.CalledProcessError):
            self.make("init", "CC=/nonexistent/fixture-compiler")
        self.assertFalse((self.root / "target/guest/init").exists())

    def test_dry_run_has_no_preparation_or_output_side_effects(self):
        for public in (False, True):
            with self.subTest(public=public):
                result = self.make("-n", "initramfs", "kernel", public=public)
                self.assertIn("dry run", result.stdout)
                self.assertFalse((self.root / "events").exists())
                self.assertFalse((self.root / "target").exists())

    def test_runner_builds_before_catalog_and_launch(self):
        self.make("kernel", "TARGET=custom")
        captured = []

        def launch(suite, args, selected, catalog):
            captured.append(args)
            self.assertEqual(selected, ["EX001"])
            self.assertEqual(catalog, {"EX001": "Example"})
            self.assertTrue(args.binary.is_file())
            self.assertTrue(args.initramfs.is_file())
            return 0

        with patch.object(appliance, "launch", side_effect=launch):
            status = appliance.main([
                "--suite", str(self.root), "--build", "--build-dir", "custom", "EX001"])
        self.assertEqual(status, 0)
        self.assertEqual(len(captured), 1)
        self.assertEqual(captured[0].kernel, self.root / "custom/produced-kernel")
        self.assertEqual((self.root / "events").read_text().splitlines(),
                         ["kernel", "prepare"])

    def test_runner_without_build_does_not_prepare_artifacts(self):
        self.make("kernel")
        with contextlib.redirect_stderr(io.StringIO()), \
             patch.object(appliance, "launch") as run, \
             self.assertRaises(SystemExit) as error:
            appliance.main(["--suite", str(self.root)])
        self.assertEqual(error.exception.code, 2)
        run.assert_not_called()
        self.assertFalse((self.root / "target/guest").exists())

    def test_failed_preparation_stops_before_launch_with_one_infrastructure_error(self):
        self.make("kernel")
        (self.root / "fail").touch()
        errors = io.StringIO()
        with contextlib.redirect_stderr(errors), patch.object(appliance, "launch") as run:
            status = appliance.main(["--suite", str(self.root), "--build"])
        self.assertEqual(status, 2)
        self.assertEqual(errors.getvalue().count("[INFRA]"), 1)
        self.assertIn("Fixture preparation failed", errors.getvalue())
        run.assert_not_called()

    def test_invalid_runner_options_do_not_start_builds(self):
        self.make("kernel")
        for options in (
                ["--build", "--list"], ["--build", "--jobs", "0"],
                ["--build", "--binary=/some/init"],
                ["--build", "--initramfs", "/some/archive"]):
            with self.subTest(options=options), \
                 contextlib.redirect_stderr(io.StringIO()), \
                 self.assertRaises(SystemExit) as error:
                appliance.main(["--suite", str(self.root), *options])
            self.assertEqual(error.exception.code, 2)
            self.assertFalse((self.root / "target/guest").exists())

    def test_explicit_kernel_override_is_preserved(self):
        kernel = self.root / "external-kernel"
        kernel.write_bytes(b"kernel")
        suite = appliance.load_suite(self.root)
        args = appliance.parse_args(suite, [
            "--build", "--build-dir", "custom", "--kernel", str(kernel)])
        self.assertEqual(args.kernel, kernel)
        self.assertEqual(args.initramfs, self.root / "custom/guest/initramfs.cpio.gz")
        self.assertFalse(args.initramfs.exists())

    def test_environment_target_selects_the_same_artifacts_without_building(self):
        with patch.dict(os.environ, {"TARGET": "alternate"}):
            suite = appliance.load_suite(self.root)
        self.assertEqual(suite.binary, self.root / "alternate/guest/init")
        self.assertIsNone(suite.kernel)
        self.assertIsNone(suite.build_spec.context("alternate").kernel_image)
        self.assertFalse((self.root / "alternate").exists())

    def test_listing_with_custom_outputs_does_not_build(self):
        with contextlib.redirect_stdout(io.StringIO()) as output:
            status = appliance.main([
                "--suite", str(self.root), "--list", "--build-dir", "custom"])
        self.assertEqual(status, 0)
        self.assertIn("EX001\tExample", output.getvalue())
        self.assertFalse((self.root / "events").exists())
        self.assertFalse((self.root / "custom").exists())

    def test_custom_outputs_preserve_explicit_binary_and_archive_without_build(self):
        self.make("kernel", "TARGET=custom")
        archive = self.root / "external-archive"
        archive.write_bytes(b"archive")
        binary = self.root / "external-init"
        suite = appliance.load_suite(self.root)
        args = appliance.parse_args(suite, [
            "--build-dir", "custom", "--binary", str(binary),
            "--initramfs", str(archive)])
        self.assertEqual(args.binary, binary)
        self.assertEqual(args.initramfs, archive)
        self.assertEqual(args.kernel, self.root / "custom/produced-kernel")

    def test_kernel_selection_follows_actual_output_in_fresh_processes(self):
        image = self.root / "elsewhere/renamed-kernel"
        for public in (False, True):
            with self.subTest(public=public):
                self.make("kernel", f"KERNEL_DEST={image}", public=public)
                suite = appliance.load_suite(self.root)
                args = appliance.parse_args(suite, ["--build"])
                self.assertEqual(args.kernel, image)
                self.assertIsNone(suite.kernel)
                self.assertFalse((self.root / "target/produced-kernel").exists())

    def test_explicit_kernel_and_listing_bypass_invalid_kernel_record(self):
        output = self.root / "target"
        output.mkdir()
        (output / "kernel-result.json").write_text("invalid")
        kernel = self.root / "manual-image"
        kernel.write_bytes(b"kernel")
        suite = appliance.load_suite(self.root)
        args = appliance.parse_args(suite, ["--build", "--kernel", str(kernel)])
        self.assertEqual(args.kernel, kernel)
        self.assertIsNone(appliance.parse_args(suite, ["--list"]).kernel)
        with self.assertRaisesRegex(RuntimeError, "Invalid kernel result"):
            appliance.parse_args(suite, ["--build"])

    def test_missing_kernel_record_never_guesses_a_filename(self):
        output = self.root / "target/images"
        output.mkdir(parents=True)
        (output / "bzImage-x86_64").write_bytes(b"unrecorded kernel")
        suite = appliance.load_suite(self.root)
        with contextlib.redirect_stderr(io.StringIO()) as errors, \
             self.assertRaises(SystemExit):
            appliance.parse_args(suite, ["--build"])
        self.assertIn("No recorded kernel build result", errors.getvalue())

    def test_unsupported_build_and_kernel_actions_are_explicit_errors(self):
        suite = appliance.Suite("example", "example-v1", Path("init"), Path("archive"))
        with self.assertRaisesRegex(RuntimeError, "does not provide build metadata"):
            appliance.parse_args(suite, ["--list", "--build-dir", "custom"])
        context = BuildSpec(self.root, lambda root: []).context()
        for action, message in (
                ("kernel", "kernel build"), ("prepare", "preparation"),
                ("init", "source list is empty"), ("unknown", "Invalid suite build")):
            with self.subTest(action=action), self.assertRaisesRegex(RuntimeError, message):
                context.build([action])


class KernelResultTests(unittest.TestCase):
    def setUp(self):
        temporary = tempfile.TemporaryDirectory()
        self.addCleanup(temporary.cleanup)
        self.root = Path(temporary.name)
        self.context = BuildSpec(self.root, lambda root: []).context()
        self.image = self.root / "configured/output-with-no-conventional-name"
        self.image.parent.mkdir()
        self.image.write_bytes(b"produced kernel")

    def report(self, data):
        def run(command, **kwargs):
            Path(command[-1]).write_text(json.dumps(data))
        return patch.object(self.context, "run", side_effect=run)

    def test_reported_path_and_format_are_used_for_both_architectures(self):
        for arch, kind in (("x86_64", "bzImage"), ("aarch64", "Image")):
            data = {"arch": arch, "boot_image": kind,
                    "images": {kind: {"path": str(self.image)}}}
            with self.subTest(arch=arch), self.report(data):
                result = self.context.build_kernel(["--source", "fixture"])
            self.assertEqual(result, KernelResult(self.image, arch, kind))

    def test_explicit_image_kind_uses_that_reported_artifact(self):
        data = {"arch": "x86_64", "boot_image": "bzImage",
                "images": {"vmlinux": {"path": str(self.image)}}}
        with self.report(data):
            result = self.context.build_kernel([], image="vmlinux")
        self.assertEqual(result.image, self.image)
        self.assertEqual(result.format, "vmlinux")

    def test_unavailable_requested_image_is_an_explicit_error(self):
        data = {"arch": "aarch64", "boot_image": "Image",
                "images": {"Image": {"path": str(self.image)}}}
        with self.report(data), \
             self.assertRaisesRegex(RuntimeError, "did not produce requested image format"):
            self.context.build_kernel([], image="bzImage")

    def test_wrong_architecture_does_not_replace_previous_selection(self):
        self.context.spec.kernel = lambda context: KernelResult(
            self.image, "x86_64", "bzImage")
        self.context.build(["kernel"])
        recorded = (self.context.target / "kernel-result.json").read_bytes()
        self.context.spec.kernel = lambda context: context.build_kernel(
            [], expected_arch="x86_64", image="bzImage")
        data = {"arch": "aarch64", "boot_image": "Image",
                "images": {"Image": {"path": str(self.image)}}}
        with self.report(data), self.assertRaisesRegex(
                RuntimeError, "expected x86_64, actual aarch64"):
            self.context.build(["kernel"])
        self.assertEqual((self.context.target / "kernel-result.json").read_bytes(), recorded)
        self.assertEqual(self.context.kernel_image, self.image)

    def test_unsupported_architecture_and_malformed_reports_are_rejected(self):
        for data in (
                [], {}, {"arch": "x86_64", "images": {}},
                {"arch": "riscv64", "boot_image": "Image",
                 "images": {"Image": {"path": str(self.image)}}},
                {"arch": "x86_64", "boot_image": "bzImage",
                 "images": {"bzImage": {"path": "relative-image"}}}):
            with self.subTest(data=data), self.report(data), self.assertRaises(RuntimeError):
                self.context.build_kernel([])

    def test_missing_builder_report_does_not_use_an_old_selection(self):
        with patch.object(self.context, "run"), \
             self.assertRaisesRegex(RuntimeError, "Cannot read kernel builder result"):
            self.context.build_kernel([])

    def test_missing_artifact_is_not_recorded_as_a_successful_build(self):
        self.context.spec.kernel = lambda context: KernelResult(
            self.root / "missing", "x86_64", "bzImage")
        with self.assertRaisesRegex(RuntimeError, "did not produce a nonempty image"):
            self.context.build(["kernel"])
        self.assertFalse((self.context.target / "kernel-result.json").exists())
