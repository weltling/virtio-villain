#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
"""Host tests for optional Linux builds."""

import contextlib
import hashlib
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
import importlib.machinery
import importlib.util
import io
import json
import os
from pathlib import Path
import shutil
import subprocess
import tarfile
import tempfile
import threading
import unittest
from unittest import mock


ROOT = Path(__file__).resolve().parent.parent
LOADER = importlib.machinery.SourceFileLoader("vv_kernel_build", str(ROOT / "build-kernel"))
SPEC = importlib.util.spec_from_loader(LOADER.name, LOADER)
BUILD = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(BUILD)

FIXTURE_MAKEFILE = """\
.PHONY: allnoconfig vmlinux bzImage Image
allnoconfig:
\tmkdir -p "$(O)"
\tcp "$(KCONFIG_ALLCONFIG)" "$(O)/.config"
vmlinux:
\tprintf 'fixture image' > "$(O)/vmlinux"
bzImage:
\tmkdir -p "$(O)/arch/x86/boot"
\tprintf 'fixture image' > "$(O)/arch/x86/boot/bzImage"
Image:
\tmkdir -p "$(O)/arch/arm64/boot"
\tprintf 'fixture image' > "$(O)/arch/arm64/boot/Image"
"""


def write_archive(path, members, mode="w:xz"):
    with tarfile.open(path, mode) as bundle:
        for name, content in members.items():
            data = content.encode()
            entry = tarfile.TarInfo(name)
            entry.size = len(data)
            entry.mode = 0o644
            bundle.addfile(entry, io.BytesIO(data))


@unittest.skipUnless(shutil.which("make"), "Make required for kernel build fixtures")
class ResultTests(unittest.TestCase):
    def test_base_config_adjustments_are_reported_but_requirements_stay_strict(self):
        for required_root, available in ((False, True), (True, True), (True, False)):
            with self.subTest(required_root=required_root, available=available), \
                    tempfile.TemporaryDirectory() as work:
                root = Path(work)
                source = root / "source"
                source.mkdir()
                adjustments = (
                    "s/CONFIG_GCC_VERSION=130200/CONFIG_GCC_VERSION=160201/;"
                    "s/CONFIG_GCC_PLUGINS=y/# CONFIG_GCC_PLUGINS is not set/;"
                    "s/# CONFIG_CRYPTO_KRB5 is not set/CONFIG_CRYPTO_KRB5=m/")
                if not available:
                    adjustments += ";s/CONFIG_MSHV_ROOT=y/# CONFIG_MSHV_ROOT is not set/"
                (source / "Makefile").write_text(FIXTURE_MAKEFILE.replace(
                    '\tcp "$(KCONFIG_ALLCONFIG)" "$(O)/.config"\n',
                    '\tcp "$(KCONFIG_ALLCONFIG)" "$(O)/.config"\n'
                    f'\tsed -i \'{adjustments}\' "$(O)/.config"\n'))
                base = root / "base.config"
                base.write_text(
                    "CONFIG_GCC_VERSION=130200\nCONFIG_GCC_PLUGINS=y\n"
                    "CONFIG_CRYPTO_KRB5=n\nCONFIG_MSHV_ROOT=y\n")
                options = [
                    "--source", str(source), "--base-config", str(base),
                    "--output", str(root / "output"), "--cache-dir", str(root / "cache"),
                    "--offline", "--arch", "x86_64"]
                if required_root:
                    required = root / "required.config"
                    required.write_text(
                        "CONFIG_MSHV_ROOT=y\n# CONFIG_GCC_PLUGINS is not set\n")
                    options += ["--config", str(required)]
                args = BUILD.parse_args(options)
                errors = io.StringIO()
                with contextlib.redirect_stderr(errors):
                    if not available:
                        with self.assertRaisesRegex(RuntimeError, "CONFIG_MSHV_ROOT=y"):
                            BUILD.build(args)
                        self.assertFalse((root / "output/images").exists())
                        continue
                    BUILD.build(args)
                text = errors.getvalue()
                self.assertIn("CONFIG_GCC_VERSION=130200 -> 160201", text)
                self.assertIn("CONFIG_CRYPTO_KRB5=n -> m", text)
                if required_root:
                    self.assertNotIn("CONFIG_GCC_PLUGINS", text)
                    required.write_text("# CONFIG_GCC_PLUGINS is not set\n")
                    with self.assertRaisesRegex(RuntimeError, "Build inputs changed"):
                        BUILD.build(args)
                else:
                    self.assertIn("CONFIG_GCC_PLUGINS=y -> n", text)
                self.assertTrue((root / "output/images/bzImage-x86_64").is_file())

    def test_result_reports_actual_images_for_both_supported_architectures(self):
        for arch, boot_image, names in (
                ("x86_64", "bzImage", {"vmlinux", "bzImage"}),
                ("aarch64", "Image", {"Image"})):
            with self.subTest(arch=arch), tempfile.TemporaryDirectory() as work:
                root = Path(work)
                source = root / "source"
                source.mkdir()
                (source / "Makefile").write_text(FIXTURE_MAKEFILE)
                config = root / "config"
                config.write_text("CONFIG_FIXTURE=y\n")
                output = root / "configured-output"
                report = root / "selected/result.json"
                BUILD.build(BUILD.parse_args([
                    "--source", str(source), "--config", str(config),
                    "--output", str(output), "--cache-dir", str(root / "cache"),
                    "--offline", "--arch", arch, "--result", str(report)]))
                data = json.loads(report.read_text())
                self.assertEqual(data["arch"], arch)
                self.assertEqual(data["boot_image"], boot_image)
                self.assertEqual(set(data["images"]), names)
                for image in data["images"].values():
                    path = Path(image["path"])
                    self.assertEqual(path.parent, output / "images")
                    self.assertEqual(path.read_text(), "fixture image")
                    self.assertEqual(BUILD.sha256(path), image["sha256"])
                self.assertEqual(data, json.loads(
                    (output / f"images/build-{arch}.json").read_text()))

    def test_failed_build_does_not_publish_a_result(self):
        with tempfile.TemporaryDirectory() as work:
            root = Path(work)
            source = root / "source"
            source.mkdir()
            (source / "Makefile").write_text(FIXTURE_MAKEFILE)
            config = root / "config"
            config.write_text("CONFIG_FIXTURE=y\n")
            report = root / "result.json"
            report.write_text("last successful result\n")
            args = BUILD.parse_args([
                "--source", str(source), "--config", str(config), "--offline",
                "--output", str(root / "output"), "--cache-dir", str(root / "cache"),
                "--arch", "x86_64", "--result", str(report)])
            with mock.patch.object(BUILD.subprocess, "run",
                                   side_effect=subprocess.CalledProcessError(1, "make")), \
                 self.assertRaises(subprocess.CalledProcessError):
                BUILD.build(args)
            self.assertEqual(report.read_text(), "last successful result\n")

    def test_result_cannot_overwrite_a_produced_kernel_image(self):
        with tempfile.TemporaryDirectory() as work:
            root = Path(work)
            source = root / "source"
            source.mkdir()
            (source / "Makefile").write_text(FIXTURE_MAKEFILE)
            config = root / "config"
            config.write_text("CONFIG_FIXTURE=y\n")
            image = root / "output/images/bzImage-x86_64"
            args = BUILD.parse_args([
                "--source", str(source), "--config", str(config), "--offline",
                "--output", str(root / "output"), "--cache-dir", str(root / "cache"),
                "--arch", "x86_64", "--result", str(image)])
            with self.assertRaisesRegex(RuntimeError, "Result path collides"):
                BUILD.build(args)
            self.assertEqual(image.read_text(), "fixture image")


class ReleaseTests(unittest.TestCase):
    def test_latest_selects_numeric_stable_not_mainline_rc_or_eol(self):
        releases = [
            {"moniker": moniker, "version": version, "iseol": eol,
             "released": {"timestamp": 123}}
            for moniker, version, eol in (
                ("mainline", "8.0", False), ("stable", "7.3-rc1", False),
                ("stable", "7.2.9", False), ("stable", "7.2.12", False),
                ("stable", "7.4", True))]
        with mock.patch.object(BUILD.urllib.request, "urlopen", return_value=io.BytesIO(
                json.dumps({"releases": releases}).encode())) as request:
            self.assertEqual(BUILD.latest_stable(),
                             {"version": "7.2.12", "source_date_epoch": 123})
        request.assert_called_once_with(BUILD.RELEASES_URL, timeout=30)

    def test_bad_release_metadata_fails(self):
        for data in ([], {}, {"releases": []},
                     {"releases": [{"moniker": "stable", "version": "7.2.12"}]}):
            with self.subTest(data=data), \
                    mock.patch.object(BUILD.urllib.request, "urlopen",
                                      return_value=io.BytesIO(json.dumps(data).encode())), \
                    self.assertRaises(RuntimeError):
                BUILD.latest_stable()

    def test_checksum_matches_exact_archive_name(self):
        digest = "a" * 64
        lines = (f"{'b' * 64}  linux-7.2.12.tar.gz\n"
                 f"{digest}  linux-7.2.12.tar.xz\n")
        with mock.patch.object(BUILD.urllib.request, "urlopen",
                               return_value=io.BytesIO(lines.encode())) as request:
            self.assertEqual(BUILD.kernel_checksum("7.2.12"), digest)
        request.assert_called_once_with(
            "https://cdn.kernel.org/pub/linux/kernel/v7.x/sha256sums.asc", timeout=30)
        with mock.patch.object(BUILD.urllib.request, "urlopen",
                               return_value=io.BytesIO(b"no checksum\n")), \
                self.assertRaisesRegex(RuntimeError, "No SHA256"):
            BUILD.kernel_checksum("7.2.12")

    def test_default_latest_and_pinned_selection_are_independent(self):
        manifest = json.loads((BUILD.CONFIG_DIR / "manifest.json").read_text())
        self.assertEqual(manifest["default"], "latest-stable")
        with mock.patch.object(BUILD, "latest_stable",
                               return_value={"version": "7.2.12", "source_date_epoch": 12}), \
                mock.patch.object(BUILD, "kernel_checksum", return_value="a" * 64):
            release = BUILD.resolve_release(BUILD.parse_args([]), manifest)
        self.assertEqual(release["version"], "7.2.12")
        self.assertEqual(release["sha256"], "a" * 64)
        for options in (["--pinned", "--offline"],
                        ["--version", manifest["version"], "--offline"]):
            with self.subTest(options=options), \
                    mock.patch.object(BUILD.urllib.request, "urlopen") as request:
                release = BUILD.resolve_release(BUILD.parse_args(options), manifest)
            self.assertEqual(release["version"], manifest["version"])
            self.assertEqual(release["sha256"], manifest["sha256"])
            request.assert_not_called()
        manifest["default"] = "pinned"
        with mock.patch.object(BUILD.urllib.request, "urlopen") as request:
            release = BUILD.resolve_release(BUILD.parse_args(["--offline"]), manifest)
        self.assertEqual(release["version"], manifest["version"])
        request.assert_not_called()

    def test_offline_version_with_supplied_digest_does_not_resolve_online(self):
        manifest = json.loads((BUILD.CONFIG_DIR / "manifest.json").read_text())
        options = ["--version", "6.12.100", "--sha256", "a" * 64, "--offline"]
        with mock.patch.object(BUILD.urllib.request, "urlopen") as request:
            release = BUILD.resolve_release(BUILD.parse_args(options), manifest)
        request.assert_not_called()
        self.assertEqual(release["version"], "6.12.100")
        self.assertEqual(release["sha256"], "a" * 64)

    def test_offline_resolution_never_falls_back(self):
        manifest = json.loads((BUILD.CONFIG_DIR / "manifest.json").read_text())
        for options in (["--offline"], ["--latest-stable", "--offline"],
                        ["--version", "6.12.100", "--offline"]):
            with self.subTest(options=options), \
                    mock.patch.object(BUILD.urllib.request, "urlopen") as request, \
                    self.assertRaisesRegex(RuntimeError, "network access|requires --sha256"):
                BUILD.resolve_release(BUILD.parse_args(options), manifest)
            request.assert_not_called()


class ConfigTests(unittest.TestCase):
    def test_unchanged_base_config_does_not_warn(self):
        errors = io.StringIO()
        with contextlib.redirect_stderr(errors):
            BUILD.verify_config("CONFIG_MSHV_ROOT=y\n", "CONFIG_MSHV_ROOT=y\n",
                                allow_adjustments=True)
        self.assertEqual(errors.getvalue(), "")

    def test_requested_values_are_retained(self):
        requested = 'CONFIG_VFIO=y\nCONFIG_NR_CPUS=64\nCONFIG_LOCALVERSION="-vv1"\n'
        BUILD.verify_config(requested, requested + "# generated\n")

    def test_missing_or_changed_values_fail(self):
        for actual in ("", "CONFIG_VFIO=m\n", "CONFIG_VFIO=n\n"):
            with self.subTest(actual=actual):
                with self.assertRaisesRegex(RuntimeError, "CONFIG_VFIO=y"):
                    BUILD.verify_config("CONFIG_VFIO=y\n", actual)

    def test_disabled_options_cannot_be_enabled(self):
        requested = "# CONFIG_MODULES is not set\n"
        BUILD.verify_config(requested, requested)
        BUILD.verify_config(requested, "")
        with self.assertRaisesRegex(RuntimeError, "CONFIG_MODULES"):
            BUILD.verify_config(requested, "CONFIG_MODULES=y\n")

    def test_configs_merge_in_order(self):
        with tempfile.TemporaryDirectory() as work:
            base, override = Path(work) / "base", Path(work) / "override"
            base.write_text("CONFIG_FOO=y\nCONFIG_BAR=3\n")
            override.write_text("# CONFIG_FOO is not set\nCONFIG_BAR=5\n")
            self.assertEqual(BUILD.merge_configs([base, override]),
                             "# CONFIG_FOO is not set\nCONFIG_BAR=5\n")

    def test_invalid_config_fails(self):
        with self.assertRaisesRegex(RuntimeError, "Invalid configuration line"):
            BUILD.config_values("not a configuration\n")


class SourceTests(unittest.TestCase):
    def test_download_url_uses_kernel_major_version(self):
        for version in ("6.18.58", "7.3"):
            with self.subTest(version=version), \
                    tempfile.TemporaryDirectory() as work:
                cache = Path(work)
                fixture = cache / "input"
                write_archive(fixture, {f"linux-{version}/Makefile": "# Linux\n"})
                payload = fixture.read_bytes()
                manifest = {"version": version,
                            "sha256": BUILD.sha256(fixture)}

                def download(url, path):
                    Path(path).write_bytes(payload)

                with mock.patch.object(BUILD, "download", side_effect=download) as fetch:
                    source = BUILD.checked_source(cache, manifest)
                self.assertEqual(source, cache / "sources" / manifest["sha256"])
                self.assertTrue((source / "Makefile").is_file())
                major = version.split(".", 1)[0]
                fetch.assert_called_once_with(
                    f"https://cdn.kernel.org/pub/linux/kernel/v{major}.x/"
                    f"linux-{version}.tar.xz",
                    cache / f"linux-{version}.tar.xz.part")

    def test_sha256(self):
        with tempfile.TemporaryDirectory() as work:
            path = Path(work) / "data"
            path.write_bytes(b"abc")
            self.assertEqual(BUILD.sha256(path), hashlib.sha256(b"abc").hexdigest())

    def test_bad_cached_source_is_not_extracted(self):
        with tempfile.TemporaryDirectory() as work:
            cache = Path(work)
            archive = cache / "linux-test.tar.xz"
            archive.write_bytes(b"corrupt")
            manifest = {"version": "test", "sha256": "0" * 64}
            with mock.patch.object(BUILD.subprocess, "run") as extract:
                with self.assertRaisesRegex(RuntimeError, "SHA256 mismatch"):
                    BUILD.checked_source(cache, manifest)
                extract.assert_not_called()

    def test_bad_and_interrupted_downloads_are_not_cached(self):
        for interrupted in (False, True):
            def download(url, path):
                Path(path).write_bytes(b"partial")
                if interrupted:
                    raise OSError("network failed")
            with self.subTest(interrupted=interrupted), \
                    tempfile.TemporaryDirectory() as work:
                cache = Path(work)
                manifest = {"version": "test", "sha256": "0" * 64}
                with mock.patch.object(BUILD, "download", download):
                    with self.assertRaises((RuntimeError, OSError)):
                        BUILD.checked_source(cache, manifest)
                self.assertFalse((cache / "linux-test.tar.xz").exists())
                self.assertFalse((cache / "linux-test.tar.xz.part").exists())

    def test_checked_source_reuse(self):
        with tempfile.TemporaryDirectory() as work:
            cache = Path(work)
            archive = cache / "linux-test.tar.xz"
            write_archive(archive, {"linux/Makefile": "# Linux\n"})
            manifest = {"version": "test",
                        "sha256": BUILD.sha256(archive)}
            with mock.patch.object(BUILD, "download") as fetch:
                source = BUILD.checked_source(cache, manifest)
                self.assertEqual(BUILD.checked_source(cache, manifest), source)
                fetch.assert_not_called()

    def test_changed_preloaded_release_gets_a_different_source_tree(self):
        with tempfile.TemporaryDirectory() as work:
            cache = Path(work)
            archive = cache / "linux-7.2.12.tar.xz"
            sources = []
            for text in ("# original\n", "# replacement\n"):
                write_archive(archive, {"linux/Makefile": text})
                release = {"version": "7.2.12", "sha256": BUILD.sha256(archive)}
                source = BUILD.checked_source(cache, release, offline=True)
                sources.append(source)
                self.assertEqual((source / "Makefile").read_text(), text)
            self.assertNotEqual(*sources)

    def test_archive_formats_and_root_layouts(self):
        with tempfile.TemporaryDirectory() as work:
            cache = Path(work)
            for mode in ("w", "w:gz", "w:bz2", "w:xz"):
                for prefix in ("", "custom-linux/"):
                    with self.subTest(mode=mode, prefix=prefix):
                        archive = cache / "input"
                        write_archive(archive, {prefix + "Makefile": "# kernel\n"}, mode)
                        source, digest = BUILD.archive_source(cache, archive)
                        self.assertEqual((source / "Makefile").read_text(), "# kernel\n")
                        self.assertEqual(digest, BUILD.sha256(archive))

    def test_bad_archives_do_not_install_sources(self):
        with tempfile.TemporaryDirectory() as work:
            cache = Path(work)
            archive = cache / "input"
            for members in ({"one/file": "one", "two/file": "two"},
                            {"linux/README": "no Makefile"}):
                write_archive(archive, members)
                with self.subTest(members=members), self.assertRaises(RuntimeError):
                    BUILD.archive_source(cache, archive)
                self.assertFalse((cache / "sources").exists())
            archive.write_bytes(b"corrupt archive")
            with self.assertRaises(tarfile.ReadError):
                BUILD.archive_source(cache, archive)

    def test_archive_extraction_rejects_escaping_paths_and_links(self):
        with tempfile.TemporaryDirectory() as work:
            cache = Path(work)
            archive = cache / "input"
            for link in (False, True):
                with tarfile.open(archive, "w") as bundle:
                    entry = tarfile.TarInfo("linux/link" if link else "../escaped")
                    if link:
                        entry.type = tarfile.SYMTYPE
                        entry.linkname = "../../escaped"
                    bundle.addfile(entry)
                with self.subTest(link=link), self.assertRaises(tarfile.FilterError):
                    BUILD.archive_source(cache, archive)
                self.assertFalse((cache / "escaped").exists())
                self.assertFalse((cache / "sources").exists())

    def test_offline_preloaded_release_and_missing_cache(self):
        with tempfile.TemporaryDirectory() as work:
            cache = Path(work)
            archive = cache / "linux-7.2.12.tar.xz"
            write_archive(archive, {"linux/Makefile": "# Linux\n"})
            release = {"version": "7.2.12", "sha256": BUILD.sha256(archive)}
            with mock.patch.object(BUILD.urllib.request, "urlopen") as request:
                source = BUILD.checked_source(cache, release, offline=True)
                self.assertTrue((source / "Makefile").is_file())
                archive.unlink()
                with self.assertRaisesRegex(RuntimeError, "Offline source archive is missing"):
                    BUILD.checked_source(cache, release, offline=True)
            request.assert_not_called()

    @unittest.skipUnless(shutil.which("git"), "Git unavailable")
    def test_git_branches_tags_commits_updates_and_dirty_cache(self):
        with tempfile.TemporaryDirectory() as work:
            repository, cache = Path(work) / "repo", Path(work) / "cache"
            repository.mkdir()
            cache.mkdir()
            subprocess.run(["git", "init", "-b", "test", str(repository)],
                           check=True, capture_output=True)

            def git(*args):
                return BUILD.git_output("-C", repository, *args)

            (repository / "Makefile").write_text("# first\n")
            git("add", "Makefile")
            git("-c", "user.name=Test", "-c", "user.email=test@example.invalid",
                "-c", "commit.gpgsign=false", "commit", "-m", "First")
            first = git("rev-parse", "HEAD")
            git("-c", "user.name=Test", "-c", "user.email=test@example.invalid",
                "-c", "tag.gpgsign=false", "tag", "-a", "v1", "-m", "Version one")
            for ref in ("test", "v1", first):
                source, metadata = BUILD.git_source(cache, str(repository), ref)
                self.assertEqual(metadata["commit"], first)
                self.assertEqual((source / "Makefile").read_text(), "# first\n")
                self.assertGreater(metadata["source_date_epoch"], 0)
            (repository / "Makefile").write_text("# second\n")
            git("add", "Makefile")
            git("-c", "user.name=Test", "-c", "user.email=test@example.invalid",
                "-c", "commit.gpgsign=false", "commit", "-m", "Second")
            source, metadata = BUILD.git_source(cache, str(repository), "test")
            self.assertNotEqual(metadata["commit"], first)
            self.assertEqual((source / "Makefile").read_text(), "# second\n")
            (source / "Makefile").write_text("# modified\n")
            with self.assertRaisesRegex(RuntimeError, "local changes"):
                BUILD.git_source(cache, str(repository), "test")
            subprocess.run(["git", "-C", str(source), "restore", "Makefile"],
                           check=True)
            subprocess.run(["git", "-C", str(source), "-c", "user.name=Test",
                            "-c", "user.email=test@example.invalid",
                            "-c", "commit.gpgsign=false", "commit",
                            "--allow-empty", "-m", "Local change"],
                           check=True, capture_output=True)
            with self.assertRaisesRegex(RuntimeError, "local changes"):
                BUILD.git_source(cache, str(repository), "test")
            with self.assertRaises(subprocess.CalledProcessError):
                BUILD.git_source(cache, str(repository), "missing-ref")


class BuildTests(unittest.TestCase):
    def test_entry_point_runs_from_another_directory(self):
        with tempfile.TemporaryDirectory() as work:
            result = subprocess.run(
                [str(ROOT / "build-kernel"), "--help"], cwd=work,
                capture_output=True, text=True, check=True)
        self.assertIn("--latest-stable", result.stdout)
        self.assertIn("--offline", result.stdout)
        self.assertEqual(BUILD.ROOT, ROOT)
        self.assertEqual(BUILD.CONFIG_DIR, ROOT / "kernel")

    def test_cli_rejects_incomplete_or_invalid_requests(self):
        for args in (["--source", "src"], ["--git-url", "repo"],
                     ["--ref", "main"], ["--config", "config"],
                     ["--jobs", "0"],
                     ["--source-date-epoch", "-1"],
                     ["--git-url", "repo", "--ref=-bad"],
                     ["--source", "src", "--git-url", "repo"],
                     ["--latest-stable", "--pinned"],
                     ["--version", "../bad"], ["--version", "7.3-rc1"],
                     ["--sha256", "a" * 64],
                     ["--version", "7.3", "--sha256", "invalid"],
                     ["--archive", "input"], ["--url", "https://example.invalid/input"],
                     ["--url", "file:///input", "--config", "config", "--output", "out"]):
            with self.subTest(args=args), mock.patch("sys.stderr"), \
                    self.assertRaises(SystemExit) as error:
                BUILD.parse_args(args)
            self.assertEqual(error.exception.code, 2)

    def test_local_git_and_pinned_builds(self):
        manifest = json.loads((BUILD.CONFIG_DIR / "manifest.json").read_text())
        for arch in ("x86_64", "aarch64"):
            for mode in ("pinned", "latest", "version", "local", "git"):
                custom = mode in ("local", "git")
                with self.subTest(arch=arch, mode=mode), \
                        tempfile.TemporaryDirectory() as work:
                    root = Path(work)
                    source = root / "linux"
                    source.mkdir()
                    (source / "Makefile").write_text("# fake Linux\n")
                    config = root / "custom.config"
                    config.write_text('CONFIG_CUSTOM=y\nCONFIG_LOCALVERSION="-custom"\n')
                    options = ["--arch", arch, "--source-date-epoch", "123",
                               "--cross-compile", "test-cross-"]
                    if custom:
                        options += ["--config", str(config),
                                    "--output", str(root / "out")]
                    if mode == "local":
                        options += ["--source", str(source)]
                    elif mode == "git":
                        options += ["--git-url", "example", "--ref", "branch"]
                    elif mode == "pinned":
                        options += ["--pinned"]
                    elif mode == "version":
                        options += ["--version", manifest["version"]]
                    args = BUILD.parse_args(options)
                    commands = []

                    def make(command, env, check):
                        commands.append(command)
                        self.assertTrue(check)
                        self.assertEqual(env["SOURCE_DATE_EPOCH"], "123")
                        output = Path(next(arg[2:] for arg in command
                                           if arg.startswith("O=")))
                        if command[-1] == "allnoconfig":
                            shutil.copyfile(env["KCONFIG_ALLCONFIG"],
                                            output / ".config")
                        else:
                            for image in ("vmlinux", "arch/x86/boot/bzImage",
                                          "arch/arm64/boot/Image"):
                                path = output / image
                                path.parent.mkdir(parents=True, exist_ok=True)
                                path.write_bytes(b"kernel image")

                    with mock.patch.object(BUILD, "ROOT", root), \
                            mock.patch.object(
                                BUILD, "latest_stable",
                                return_value={"version": manifest["version"],
                                              "source_date_epoch": 456}), \
                            mock.patch.object(BUILD, "kernel_checksum", return_value="a" * 64), \
                            mock.patch.object(BUILD, "checked_source",
                                              return_value=source) as download, \
                            mock.patch.object(
                                BUILD, "git_source",
                                return_value=(source, {"commit": "abc",
                                                       "source_date_epoch": 456})) as fetch, \
                            mock.patch.object(BUILD.subprocess, "run", make):
                        BUILD.build(args)
                        if custom:
                            download.assert_not_called()
                        else:
                            download.assert_called_once()
                        if mode == "git":
                            fetch.assert_called_once_with(
                                root / "target/guest-kernel", "example", "branch")
                        else:
                            fetch.assert_not_called()
                        expected_root = (root / "out" if custom else
                                         root / "target/guest-kernel"
                                         / f"{manifest['version']}-{manifest['revision']}")
                        images = expected_root / "images"
                        text = (images / f"config-{arch}").read_text()
                        self.assertEqual("CONFIG_CUSTOM=y" in text, custom)
                        self.assertEqual("CONFIG_VIRTIO=y" in text, not custom)
                        metadata = json.loads((images / f"build-{arch}.json").read_text())
                        self.assertEqual(metadata["source_date_epoch"], 123)
                        if mode == "git":
                            self.assertEqual(metadata["commit"], "abc")
                        self.assertIn("CROSS_COMPILE=test-cross-", commands[0])
                        self.assertEqual(commands[1][-2:] if arch == "x86_64"
                                         else commands[1][-1:],
                                         ["vmlinux", "bzImage"] if arch == "x86_64"
                                         else ["Image"])
                        for line in (images / f"SHA256SUMS-{arch}").read_text().splitlines():
                            digest, name = line.split()
                            self.assertEqual(BUILD.sha256(images / name), digest)
                        if custom:
                            config.write_text("CONFIG_CHANGED=y\n")
                            with self.assertRaisesRegex(RuntimeError, "inputs changed"):
                                BUILD.build(args)

    def test_source_and_output_must_be_separate(self):
        with tempfile.TemporaryDirectory() as work:
            root = Path(work)
            (root / "Makefile").write_text("# Linux\n")
            config = root / "test.config"
            config.write_text("CONFIG_TEST=y\n")
            for output in (root, root / "nested", root.parent):
                args = BUILD.parse_args(
                    ["--source", str(root), "--config", str(config),
                     "--output", str(output), "--arch", "x86_64"])
                with mock.patch.object(BUILD, "ROOT", root / "project"), \
                        self.assertRaisesRegex(RuntimeError, "separate trees"):
                    BUILD.build(args)

    def test_rejected_config_stops_before_compilation(self):
        with tempfile.TemporaryDirectory() as work:
            root = Path(work)
            source = root / "linux"
            source.mkdir()
            (source / "Makefile").write_text("# Linux\n")
            config = root / "test.config"
            config.write_text("CONFIG_TEST=y\n")
            args = BUILD.parse_args(
                ["--source", str(source), "--config", str(config),
                 "--output", str(root / "out"), "--arch", "x86_64"])

            def reject(command, **kwargs):
                (root / "out/x86_64/.config").write_text("")

            with mock.patch.object(BUILD, "ROOT", root), \
                    mock.patch.object(BUILD.subprocess, "run",
                                      side_effect=reject) as make, \
                    self.assertRaisesRegex(RuntimeError, "CONFIG_TEST=y"):
                BUILD.build(args)
            self.assertEqual(make.call_count, 1)
            self.assertFalse((root / "out/images").exists())

    def test_offline_git_is_rejected_before_fetch(self):
        with tempfile.TemporaryDirectory() as work:
            root = Path(work)
            config = root / "config"
            config.write_text("CONFIG_TEST=y\n")
            args = BUILD.parse_args([
                "--git-url", "example", "--ref", "branch", "--offline",
                "--config", str(config), "--output", str(root / "out")])
            with mock.patch.object(BUILD, "ROOT", root), \
                    mock.patch.object(BUILD, "git_source") as fetch, \
                    self.assertRaisesRegex(RuntimeError, "prepared local checkout"):
                BUILD.build(args)
            fetch.assert_not_called()

    def test_missing_config_fails_before_resolving_a_source(self):
        args = BUILD.parse_args(["--config", "/missing/kernel/config", "--output", "/unused"])
        with mock.patch.object(BUILD.urllib.request, "urlopen") as request, \
                self.assertRaises(FileNotFoundError):
            BUILD.build(args)
        request.assert_not_called()


@unittest.skipUnless(shutil.which("make"), "Make unavailable")
class FixtureBuildTests(unittest.TestCase):
    def test_make_config_selection_and_argument_precedence(self):
        with tempfile.TemporaryDirectory() as work:
            root = Path(work)
            source = root / "source"
            source.mkdir()
            (source / "Makefile").write_text(FIXTURE_MAKEFILE)
            wrapper = root / "build-kernel"
            wrapper.write_text(
                "#!/usr/bin/env python3\nimport runpy\n"
                f"runpy.run_path({str(ROOT / 'build-kernel')!r}, run_name='__main__')\n")
            wrapper.chmod(0o755)
            base, fragment, last = (root / name for name in (
                "base.config", "fragment.config", "last.config"))
            base.write_text("CONFIG_FIXTURE=m\n")
            fragment.write_text("CONFIG_FIXTURE=y\n")
            last.write_text("# CONFIG_FIXTURE is not set\n")
            for index, expected in enumerate(("m", "y", "n")):
                with self.subTest(expected=expected):
                    output = root / f"output-{index}"
                    arguments = (f"--source {source} --output {output} "
                                 f"--cache-dir {root / 'cache'} --offline --jobs 2")
                    variables = []
                    if index == 0:
                        arguments += f" --config {base}"
                    else:
                        variables.append(f"KERNEL_CONFIGS={base} {fragment}")
                    if index == 2:
                        arguments += f" --config {last}"
                    subprocess.run([
                        "make", "--no-print-directory", "-C", str(root),
                        "-f", str(ROOT / "Makefile"), "guest-kernel",
                        *variables, f"KERNEL_ARGS={arguments}",
                    ], check=True, capture_output=True, text=True, timeout=20)
                    config = (output / "images/config-x86_64").read_text()
                    self.assertEqual(BUILD.config_values(config)["CONFIG_FIXTURE"], expected)
                    self.assertTrue((output / "images/bzImage-x86_64").is_file())

    def test_make_and_builder_help_explain_config_selection(self):
        commands = (
            ["make", "--no-print-directory", "-C", str(ROOT), "help"],
            [str(ROOT / "build-kernel"), "--help"])
        for command in commands:
            with self.subTest(command=command):
                result = subprocess.run(command, check=True, capture_output=True,
                                        text=True, timeout=20)
                text = " ".join(result.stdout.split())
                for expected in ("--output", "RUSTC", "Later files override earlier values"):
                    self.assertIn(expected, text)
                if command[0] == "make":
                    self.assertIn("KERNEL_CONFIGS", text)
                    self.assertIn("KERNEL_ARGS", text)
                else:
                    self.assertIn("--config", text)

    def test_explicit_rust_compiler_reaches_configuration_and_compilation(self):
        for compiler in (None, "false"):
            with self.subTest(compiler=compiler), tempfile.TemporaryDirectory() as work:
                root = Path(work)
                source = root / "source"
                source.mkdir()
                (source / "Makefile").write_text(
                    "RUSTC = rustc\n" + FIXTURE_MAKEFILE
                    + "\n.PHONY: record-compiler\n"
                    "allnoconfig vmlinux bzImage: record-compiler\n"
                    "record-compiler:\n"
                    '\tmkdir -p "$(O)"\n'
                    '\tprintf "%s\\n" "$(RUSTC)" >> "$(O)/compiler.log"\n')
                config = root / "config"
                config.write_text("CONFIG_FIXTURE=y\n")
                args = BUILD.parse_args([
                    "--source", str(source), "--config", str(config),
                    "--output", str(root / "output"), "--cache-dir", str(root / "cache"),
                    "--offline", "--arch", "x86_64", "--jobs", "2"])
                environment = dict(os.environ)
                for name in ("MAKEFLAGS", "MAKEOVERRIDES", "RUSTC"):
                    environment.pop(name, None)
                if compiler is not None:
                    environment["RUSTC"] = compiler
                with mock.patch.dict(os.environ, environment, clear=True):
                    BUILD.build(args)
                recorded = (root / "output/x86_64/compiler.log").read_text().splitlines()
                self.assertEqual(recorded, [compiler or "rustc"] * 2)
                self.assertTrue((root / "output/images/bzImage-x86_64").is_file())

    def test_preloaded_official_releases_accept_independent_configs(self):
        with tempfile.TemporaryDirectory() as work:
            root = Path(work)
            cache = root / "cache"
            cache.mkdir()
            archive = cache / "linux-9.9.tar.xz"
            write_archive(archive, {"linux/Makefile": FIXTURE_MAKEFILE})
            digest = BUILD.sha256(archive)
            profile = root / "profile"
            profile.mkdir()
            (profile / "manifest.json").write_text(json.dumps({
                "default": "latest-stable", "version": "9.9", "revision": "test",
                "sha256": digest, "source_date_epoch": 123}))
            config = root / "supplied.config"
            config.write_text("CONFIG_PRELOADED=y\n")
            for selector in (["--pinned"], ["--version", "9.9", "--sha256", digest]):
                options = selector + [
                    "--offline", "--config", str(config), "--output", str(root / "out"),
                    "--cache-dir", str(cache), "--arch", "x86_64"]
                with self.subTest(selector=selector), \
                        mock.patch.object(BUILD, "CONFIG_DIR", profile), \
                        mock.patch.object(BUILD.urllib.request, "urlopen") as request:
                    BUILD.build(BUILD.parse_args(options))
                request.assert_not_called()
                images = root / "out/images"
                self.assertEqual((images / "config-x86_64").read_text(),
                                 "CONFIG_PRELOADED=y\n")
                metadata = json.loads((images / "build-x86_64.json").read_text())
                self.assertEqual(metadata["version"], "9.9")
                self.assertEqual(metadata["sha256"], digest)
                self.assertTrue(metadata["checksum_verified"])

    def test_pipeline_preloaded_archive_and_config_are_used_without_network(self):
        with tempfile.TemporaryDirectory() as work:
            root = Path(work)
            archive = root / "preloaded.tar.gz"
            config = root / "preloaded.config"
            write_archive(archive, {"custom-source/Makefile": FIXTURE_MAKEFILE}, "w:gz")
            config.write_text("CONFIG_PIPELINE=y\n")
            digest = BUILD.sha256(archive)
            config_digest = BUILD.sha256(config)
            archive.chmod(0o444)
            config.chmod(0o444)
            args = BUILD.parse_args([
                "--offline", "--archive", str(archive), "--config", str(config),
                "--sha256", digest.upper(), "--output", str(root / "out"),
                "--cache-dir", str(root / "cache"), "--arch", "x86_64"])
            with mock.patch.object(BUILD.urllib.request, "urlopen") as request:
                BUILD.build(args)
                BUILD.build(args)
            request.assert_not_called()
            self.assertEqual(BUILD.sha256(archive), digest)
            self.assertEqual(BUILD.sha256(config), config_digest)
            images = root / "out/images"
            self.assertEqual((images / "config-x86_64").read_text(), "CONFIG_PIPELINE=y\n")
            metadata = json.loads((images / "build-x86_64.json").read_text())
            self.assertEqual(metadata["sha256"], digest)
            self.assertTrue(metadata["checksum_verified"])
            self.assertEqual((images / "bzImage-x86_64").read_text(), "fixture image")
            for line in (images / "SHA256SUMS-x86_64").read_text().splitlines():
                checksum, name = line.split()
                self.assertEqual(BUILD.sha256(images / name), checksum)

    def test_http_download_and_offline_cache_reuse(self):
        with tempfile.TemporaryDirectory() as work:
            root = Path(work)
            archive = root / "input.tar"
            write_archive(archive, {"Makefile": FIXTURE_MAKEFILE}, "w")
            payload = archive.read_bytes()
            digest = BUILD.sha256(archive)
            requests = []

            class Handler(BaseHTTPRequestHandler):
                def do_GET(self):
                    requests.append(self.path)
                    self.send_response(200)
                    self.send_header("Content-Length", str(len(payload)))
                    self.end_headers()
                    self.wfile.write(payload)

                def log_message(self, *args):
                    pass

            server = ThreadingHTTPServer(("127.0.0.1", 0), Handler)
            thread = threading.Thread(target=server.serve_forever, daemon=True)
            thread.start()
            config = root / "config"
            config.write_text("CONFIG_HTTP=y\n")
            url = f"http://127.0.0.1:{server.server_port}/kernel"
            options = ["--url", url, "--sha256", digest, "--config", str(config),
                       "--output", str(root / "out"), "--cache-dir", str(root / "cache"),
                       "--arch", "aarch64"]
            try:
                BUILD.build(BUILD.parse_args(options))
            finally:
                server.shutdown()
                server.server_close()
                thread.join(timeout=5)
            self.assertFalse(thread.is_alive())
            self.assertEqual(requests, ["/kernel"])
            with mock.patch.object(BUILD.urllib.request, "urlopen") as request:
                BUILD.build(BUILD.parse_args(options + ["--offline"]))
            request.assert_not_called()
            metadata = json.loads((root / "out/images/build-aarch64.json").read_text())
            self.assertEqual(metadata["url"], url)
            self.assertTrue(metadata["checksum_verified"])
            self.assertEqual(metadata["sha256"], digest)
            unverified = [arg for arg in options if arg not in ("--sha256", digest)]
            with mock.patch.object(BUILD.urllib.request, "urlopen") as request, \
                    mock.patch("sys.stderr") as stderr:
                BUILD.build(BUILD.parse_args(unverified + ["--offline"]))
            request.assert_not_called()
            self.assertIn("no expected SHA256", "".join(
                str(call.args[0]) for call in stderr.write.call_args_list))
            metadata = json.loads((root / "out/images/build-aarch64.json").read_text())
            self.assertFalse(metadata["checksum_verified"])
            cached = next((root / "cache/downloads").glob("*/archive"))
            cached.write_bytes(b"corrupt")
            with mock.patch.object(BUILD.urllib.request, "urlopen") as request, \
                    self.assertRaisesRegex(RuntimeError, "SHA256 mismatch"):
                BUILD.build(BUILD.parse_args(options + ["--offline"]))
            request.assert_not_called()
            cached.unlink()
            with mock.patch.object(BUILD.urllib.request, "urlopen") as request, \
                    self.assertRaisesRegex(RuntimeError, "Offline source archive is missing"):
                BUILD.build(BUILD.parse_args(options + ["--offline"]))
            request.assert_not_called()


if __name__ == "__main__":
    unittest.main()
