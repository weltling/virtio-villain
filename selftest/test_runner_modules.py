#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
"""Shared runner primitives without VM or device prerequisites."""

import io
from concurrent.futures import Future, ThreadPoolExecutor
import json
import os
from pathlib import Path
import shutil
import signal
import struct
import subprocess
import sys
import tempfile
import threading
import unittest
from unittest.mock import Mock, patch

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "lib/python"))
from villain import cli, console, execution, images, process, vmm  # noqa: E402

TEST_IMAGE_SIZE = 16 * 1024 * 1024


class ModuleTests(unittest.TestCase):
    def test_entry_points_import_shared_helpers_without_loading_other_executables(self):
        check = (
            "import runpy, sys\n"
            "from unittest.mock import patch\n"
            "entry = runpy.run_path(sys.argv[1], run_name='runner_import_test')\n"
            "from villain import cli, console, execution, images, initramfs, process, vmm\n"
            "assert not any(name.startswith('vv_run') for name in sys.modules)\n"
            "assert entry['images'] is images\n"
            "assert entry['process'] is process\n"
            "if sys.argv[1].endswith('/run'):\n"
            "    assert entry['detect_vmm'] is vmm.detect_vmm\n"
            "    assert entry['print_banner'] is cli.print_banner\n"
            "    assert entry['_canonicalize_tests'] is cli.canonicalize_tests\n"
            "    assert entry['drain_stream'] is console.drain_stream\n"
            "    assert entry['execution'] is execution\n"
            "elif sys.argv[1].endswith('/run-fuzz'):\n"
            "    assert entry['print_banner'] is cli.print_banner\n"
            "    assert entry['execution'] is execution\n"
            "    with patch.object(initramfs, 'build') as build:\n"
            "        entry['build_initramfs']('fuzz', 'archive')\n"
            "        build.assert_called_once_with('fuzz', 'archive', "
            "strip='strip', compresslevel=1)\n"
            "    with patch.object(vmm, 'fetch_kernel', return_value='/cache/kernel') as fetch:\n"
            "        with patch.object(entry['subprocess'], 'run', "
            "side_effect=AssertionError('Another runner must not be executed')):\n"
            "            assert entry['_fetch_kernel']() == '/cache/kernel'\n"
            "        fetch.assert_called_once_with()\n"
            "    with patch.object(vmm, 'fetch_kernel', "
            "side_effect=RuntimeError('Bad checksum')):\n"
            "        try: entry['_fetch_kernel']()\n"
            "        except SystemExit as error: assert str(error) == 'Bad checksum'\n"
            "        else: raise AssertionError('Acquisition error was ignored')\n"
            "    with patch.object(vmm.CloudHypervisor, 'build_launch_cmd', "
            "return_value=['/vmm']) as launch:\n"
            "        command = entry['_fuzz_vmm_cmd']("
            "'/vmm', '/kernel', '/initramfs', '/disk', '/pmem', '/work', 2, '128M')\n"
            "        launch.assert_called_once_with(\n"
            "            vmm.LaunchConfig(cpus=2, memory='128M',\n"
            "                kernel='/kernel', initramfs='/initramfs',\n"
            "                cmdline='console=hvc0 initcall_blacklist=virtio_blk_init'),\n"
            "            memory_hotplug_size='256M', memory_hotplug_method='virtio-mem',\n"
            "            serial='off', console='null', seccomp=False)\n"
            "        assert command[0] == '/vmm' and '--disk' in command\n"
            "else:\n"
            "    assert entry['run_guest'].__defaults__ == (vmm,)\n")
        for entry in ("run", "run-fuzz", "run-perf"):
            with self.subTest(entry=entry):
                result = subprocess.run(
                    [sys.executable, "-c", check, str(ROOT / entry)],
                    cwd="/", capture_output=True, text=True, timeout=10)
                self.assertEqual(result.returncode, 0, result.stderr)

    def test_fetch_kernel_utility_prints_only_a_path_or_an_explicit_error(self):
        check = (
            "import runpy, sys\n"
            "from unittest.mock import patch\n"
            f"sys.path.insert(0, {str(ROOT / 'lib/python')!r})\n"
            "from villain import vmm\n"
            "entry, mode = sys.argv[1:]\n"
            "options = ({'return_value': '/cache/verified/kernel'} if mode == 'pass' else\n"
            "           {'side_effect': RuntimeError('Kernel checksum failed')})\n"
            "sys.argv = [entry, 'fetch-kernel']\n"
            "with patch.object(vmm, 'fetch_kernel', **options):\n"
            "    runpy.run_path(entry, run_name='__main__')\n")
        for mode in ("pass", "fail"):
            with self.subTest(mode=mode):
                result = subprocess.run(
                    [sys.executable, "-c", check, str(ROOT / "run"), mode],
                    cwd="/", capture_output=True, text=True, timeout=10)
                self.assertEqual(result.returncode, 0 if mode == "pass" else 1)
                self.assertEqual(result.stdout,
                                 "/cache/verified/kernel\n" if mode == "pass" else "")
                self.assertEqual(result.stderr,
                                 "" if mode == "pass" else "Kernel checksum failed\n")

    def test_sidecar_helpers_import_without_loading_runner_entry_points(self):
        check = (
            "import sys\n"
            "sys.path.insert(0, sys.argv[1] + '/lib/python')\n"
            "from villain import msix_host, scsi_helpers, sidecar_helpers\n"
            "assert msix_host.qmp is sidecar_helpers.qmp\n"
            "assert scsi_helpers.qmp is sidecar_helpers.qmp\n"
            "assert not any(name.startswith('vv_run') for name in sys.modules)\n")
        result = subprocess.run(
            [sys.executable, "-I", "-c", check, str(ROOT)],
            cwd="/", capture_output=True, text=True, timeout=10)
        self.assertEqual(result.returncode, 0, result.stderr)

    def test_sidecar_loading_does_not_change_module_search_paths(self):
        check = (
            "from pathlib import Path\n"
            "import runpy, sys\n"
            "root = Path(sys.argv[1])\n"
            "entry = runpy.run_path(str(root / 'run'), run_name='runner_import_test')\n"
            "search_path = list(sys.path)\n"
            "sidecars = sorted((root / 'tests').glob('*/*.py'))\n"
            "assert sidecars\n"
            "for sidecar in sidecars:\n"
            "    entry['_load_sidecar'](str(sidecar))\n"
            "    assert sys.path == search_path, str(sidecar)\n")
        result = subprocess.run(
            [sys.executable, "-I", "-c", check, str(ROOT)],
            cwd="/", capture_output=True, text=True, timeout=10)
        self.assertEqual(result.returncode, 0, result.stderr)

    def test_backend_detection_is_owned_by_the_shared_module(self):
        for name, backend in (
                ("cloud-hypervisor", vmm.CloudHypervisor),
                ("qemu-system-x86_64", vmm.Qemu),
                ("openvmm", vmm.OpenVmm)):
            with self.subTest(name=name):
                self.assertIs(type(vmm.detect_vmm(name)), backend)
                self.assertEqual(backend.__module__, "villain.vmm")

    def test_banner_uses_the_checkout_binary_without_changing_output(self):
        destination = io.StringIO()
        with patch.object(cli.subprocess, "check_output", return_value=b"banner\n") as run:
            cli.print_banner(destination)
        self.assertEqual(destination.getvalue(), "banner\n")
        run.assert_called_once_with(
            [str(ROOT / "target/init"), "--banner"], stderr=subprocess.DEVNULL)

    def test_test_ids_resolve_to_registered_spelling_in_request_order(self):
        self.assertEqual(
            cli.canonicalize_tests(["ex002", "EX001", "missing", "eX002"],
                                   ["Ex001", "EX002"]),
            (["EX002", "Ex001", "EX002"], ["missing"]))


class LaunchCommandTests(unittest.TestCase):
    def test_portable_direct_boot_configuration_works_across_all_backends(self):
        config = vmm.LaunchConfig(
            cpus=2, memory="256M", kernel=Path("/kernel"),
            initramfs=Path("/initramfs"), cmdline="console=ttyS0")
        expected = (
            (vmm.CloudHypervisor, [
                "/vmm", "--kernel", "/kernel", "--initramfs", "/initramfs",
                "--cmdline", "console=ttyS0", "--cpus", "boot=2", "--memory", "size=256M"]),
            (vmm.Qemu, [
                "/vmm", "-kernel", "/kernel", "-initrd", "/initramfs",
                "-append", "console=ttyS0", "-m", "256M", "-smp", "2"]),
            (vmm.QemuMicrovm, [
                "/vmm", "-M", "microvm,rtc=off", "-global", "virtio-mmio.force-legacy=false",
                "-kernel", "/kernel", "-initrd", "/initramfs",
                "-append", "console=ttyS0", "-m", "256M", "-smp", "2"]),
            (vmm.OpenVmm, [
                "/vmm", "-k", "/kernel", "-r", "/initramfs",
                "-c", "console=ttyS0", "-m", "256M", "-p", "2"]))
        for backend_type, command in expected:
            with self.subTest(backend=backend_type.name), \
                 patch.object(vmm.platform, "machine", return_value="x86_64"), \
                 patch.dict(os.environ, {"VV_CH_DEBUG": "1"}), \
                 patch.object(vmm.CloudHypervisor, "_next_cid",
                              side_effect=AssertionError("Fixture CID must not be allocated")), \
                 patch.object(vmm.Qemu, "_next_cid",
                              side_effect=AssertionError("Fixture CID must not be allocated")):
                self.assertEqual(backend_type("/vmm").build_launch_cmd(config), command)

    def test_portable_settings_reject_backend_cpu_syntax_and_invalid_resources(self):
        for cpus in ("boot=2", 0, -1, True):
            with self.subTest(cpus=cpus), self.assertRaisesRegex(ValueError, "CPU count"):
                vmm.LaunchConfig(cpus=cpus, memory="256M")
        for memory in ("", " ", None):
            with self.subTest(memory=memory), self.assertRaisesRegex(ValueError, "Memory size"):
                vmm.LaunchConfig(cpus=2, memory=memory)

    def test_backend_launch_support_must_be_explicit(self):
        config = vmm.LaunchConfig(cpus=2, memory="256M")
        with self.assertRaisesRegex(NotImplementedError, "launch command building"):
            vmm.VmmBackend("/unknown-vmm").build_launch_cmd(config)
        config = vmm.LaunchConfig(cpus=2, memory="256M", firmware=Path("/firmware"))
        with self.assertRaisesRegex(ValueError, "openvmm firmware boot"):
            vmm.OpenVmm("/vmm").build_launch_cmd(config)

    def test_qemu_firmware_launch_has_no_direct_boot_or_virtio_fixtures(self):
        config = vmm.LaunchConfig(cpus=2, memory="256M", firmware=Path("/firmware"))
        with patch.object(vmm.platform, "machine", return_value="x86_64"):
            command = vmm.Qemu("/vmm").build_launch_cmd(config)
        self.assertEqual(command, [
            "/vmm", "-bios", "/firmware", "-m", "256M", "-smp", "2"])

    def test_qemu_launch_adapters_preserve_explicit_aarch64_platform_options(self):
        config = vmm.LaunchConfig(
            cpus=2, memory="256M", kernel=Path("/kernel"), initramfs=Path("/initramfs"))
        for backend_type in (vmm.Qemu, vmm.QemuMicrovm):
            with self.subTest(backend=backend_type.name), \
                 patch.object(vmm.platform, "machine", return_value="aarch64"):
                command = backend_type("/vmm").build_launch_cmd(
                    config, kvm=True, nographic=True, no_reboot=True)
                expected = ["/vmm", "-M", "virt", "-cpu", "host"]
                if backend_type is vmm.QemuMicrovm:
                    expected += ["-global", "virtio-mmio.force-legacy=false"]
                expected += [
                    "-enable-kvm", "-kernel", "/kernel", "-initrd", "/initramfs",
                    "-nographic", "-no-reboot", "-m", "256M", "-smp", "2"]
                self.assertEqual(command, expected)

    def test_all_virtio_builders_delegate_boot_and_resource_configuration(self):
        for backend_type in (vmm.CloudHypervisor, vmm.Qemu, vmm.QemuMicrovm, vmm.OpenVmm):
            with self.subTest(backend=backend_type.name), \
                 patch.object(vmm.platform, "machine", return_value="x86_64"):
                backend = backend_type("/vmm")
                with patch.object(backend, "build_launch_cmd", return_value=["/vmm"]) as launch:
                    command = backend.build_cmd(
                        "/kernel", "/initramfs", "/disk.raw", "console=ttyS0",
                        {"cpus": 2, "memory": "128M", "fuzz_device": "rng", "mmio_device": "rng"})
                launch.assert_called_once()
                self.assertEqual(launch.call_args.args, (vmm.LaunchConfig(
                    cpus=1 if backend_type is vmm.QemuMicrovm else 2, memory="128M",
                    kernel="/kernel", initramfs="/initramfs", cmdline="console=ttyS0"),))
                for flag in ("--kernel", "--cpus", "--memory", "-kernel", "-m", "-smp", "-k", "-p"):
                    self.assertNotIn(flag, command)

    def test_ch_virtio_builder_enables_acpi_on_aarch64(self):
        for arch in ("x86_64", "aarch64"):
            for cmdline in ("console=ttyAMA0 vv.transport=pci vv.test=B0201", "", None):
                with self.subTest(arch=arch, cmdline=cmdline), \
                     patch.object(vmm.platform, "machine", return_value=arch):
                    command = vmm.CloudHypervisor("/vmm").build_cmd(
                        "/kernel", "/initramfs", "/disk.raw", cmdline, {"fuzz_device": "rng"})
                    actual = (command[command.index("--cmdline") + 1]
                              if "--cmdline" in command else None)
                    expected = cmdline
                    if arch == "aarch64":
                        expected = f"{cmdline} acpi=on" if cmdline else "acpi=on"
                    self.assertEqual(actual, expected)

    def test_firmware_launch_has_no_implicit_virtio_fixtures_or_host_policy(self):
        backend = vmm.CloudHypervisor("/vmm")
        with patch.dict(os.environ, {"VV_CH_DEBUG": "1"}), \
             patch.object(backend, "_next_cid",
                          side_effect=AssertionError("Fixture CID must not be allocated")):
            command = backend.build_launch_cmd(
                vmm.LaunchConfig(cpus=2, memory="2G", firmware=Path("/boot/firmware.fd")),
                nested=True, serial="tty", console="off")
        self.assertEqual(command, [
            "/vmm", "--firmware", "/boot/firmware.fd",
            "--cpus", "boot=2,nested=on", "--memory", "size=2G",
            "--serial", "tty", "--console", "off"])

    def test_direct_boot_and_optional_debug_and_seccomp_are_explicit(self):
        backend = vmm.CloudHypervisor("/vmm")
        for seccomp in (False, True):
            with self.subTest(seccomp=seccomp):
                command = backend.build_launch_cmd(
                    vmm.LaunchConfig(
                        cpus=1, memory="128M", kernel=Path("/kernel"),
                        initramfs=Path("/initramfs"), cmdline="console=ttyS0"),
                    serial="off", console="null", seccomp=seccomp, debug=True)
                self.assertEqual(command, [
                    "/vmm", "-vv", "--kernel", "/kernel", "--initramfs", "/initramfs",
                    "--cmdline", "console=ttyS0", "--cpus", "boot=1",
                    "--memory", "size=128M", "--serial", "off", "--console", "null",
                    "--seccomp", "true" if seccomp else "false"])
                self.assertTrue(all(isinstance(argument, str) for argument in command))

    def test_virtio_builder_preserves_queue_limits_and_fixture_configuration(self):
        backend = vmm.CloudHypervisor("/vmm")
        options = {
            "cpus": 1, "memory": "256M", "blk_queues": 8, "net_queues": 4,
            "pmem_path": "/pmem", "fs_socket": "/fs", "api_socket": "/api"}
        with patch.dict(os.environ, {"VV_CH_DEBUG": ""}), \
             patch.object(backend, "_next_cid", return_value=23):
            command = backend.build_cmd(
                "/kernel", "/initramfs", "/disk.raw", "console=ttyS0", options)
        self.assertEqual(command, [
            "/vmm", "--kernel", "/kernel", "--initramfs", "/initramfs",
            "--cmdline", "console=ttyS0", "--cpus", "boot=8",
            "--memory", "size=256M,shared=on,hotplug_size=256M,hotplug_method=virtio-mem",
            "--serial", "tty", "--console", "null", "--seccomp", "false",
            "--disk", "path=/disk.raw,image_type=raw,num_queues=8,iommu=virtio",
            "--net", "tap=,num_queues=4", "--vsock", "cid=23,socket=/disk.vsock",
            "--balloon", "size=0", "--watchdog", "--rng", "src=/dev/urandom",
            "--pmem", "file=/pmem,size=128M,discard_writes=on",
            "--fs", "tag=vvfs,socket=/fs,num_queues=1,queue_size=128",
            "--api-socket", "path=/api"])
        self.assertEqual(options["cpus"], 1)


class ImageTests(unittest.TestCase):
    def test_raw_creation_has_exact_size_and_zero_contents_without_external_tools(self):
        with tempfile.TemporaryDirectory() as work:
            path = Path(work) / "disk.raw"
            for size in (0, 1, TEST_IMAGE_SIZE):
                with self.subTest(size=size), patch.object(images.subprocess, "run") as run:
                    self.assertIsNone(images.create(path, size))
                    self.assertEqual(path.stat().st_size, size)
                    with path.open("rb") as stream:
                        self.assertEqual(stream.read(4096), bytes(min(size, 4096)))
                        stream.seek(max(0, size - 4096))
                        self.assertEqual(stream.read(), bytes(min(size, 4096)))
                    run.assert_not_called()

    def test_raw_replacement_does_not_retain_old_bytes(self):
        with tempfile.TemporaryDirectory() as work:
            path = Path(work) / "disk.raw"
            path.write_bytes(b"old contents")
            images.create_raw(path, 8)
            self.assertEqual(path.read_bytes(), bytes(8))

    def test_exclusive_raw_creation_preserves_existing_files(self):
        with tempfile.TemporaryDirectory() as work:
            path = Path(work) / "disk.raw"
            images.create_raw(path, 8, exclusive=True)
            path.write_bytes(b"retained contents")
            with self.assertRaises(FileExistsError):
                images.create_raw(path, 16, exclusive=True)
            self.assertEqual(path.read_bytes(), b"retained contents")

    def test_invalid_sizes_and_formats_fail_before_modifying_files(self):
        with tempfile.TemporaryDirectory() as work, \
             patch.object(images.subprocess, "run") as run:
            path = Path(work) / "disk.raw"
            path.write_bytes(b"retained contents")
            for size in (-1, 1.5, True):
                for image_format in images.FORMATS:
                    with self.subTest(size=size, image_format=image_format), \
                         self.assertRaisesRegex(ValueError, "Image size"):
                        images.create(path, size, image_format)
            with self.assertRaisesRegex(ValueError, "Unsupported disk image format"):
                images.create(path, 16, "unknown")
            self.assertEqual(path.read_bytes(), b"retained contents")
            run.assert_not_called()

    def test_container_commands_preserve_formats_sizes_and_fixed_vhd(self):
        for image_format, qemu_format in (("qcow2", "qcow2"), ("vhd", "vpc"), ("vhdx", "vhdx")):
            with self.subTest(image_format=image_format), \
                 patch.object(images.subprocess, "run") as run:
                path = Path(f"disk.{image_format}")
                images.create(path, TEST_IMAGE_SIZE, image_format)
                command = ["qemu-img", "create", "-f", qemu_format]
                if image_format == "vhd":
                    command += ["-o", "subformat=fixed"]
                command += [str(path), str(TEST_IMAGE_SIZE)]
                run.assert_called_once_with(command, check=True, capture_output=True)

    def test_filesystem_and_external_tool_errors_are_not_hidden(self):
        denied = PermissionError("Image is not writable")
        with patch("builtins.open", side_effect=denied), \
             self.assertRaises(PermissionError) as caught:
            images.create_raw("/disk.raw", 16)
        self.assertIs(caught.exception, denied)
        for error in (FileNotFoundError("qemu-img is missing"),
                      subprocess.CalledProcessError(1, ["qemu-img"], stderr=b"Disk error")):
            with self.subTest(error=type(error).__name__), \
                 patch.object(images.subprocess, "run", side_effect=error), \
                 self.assertRaises(type(error)) as caught:
                images.create("/disk.qcow2", 16, "qcow2")
            self.assertIs(caught.exception, error)

    @unittest.skipUnless(shutil.which("qemu-img"), "qemu-img required")
    def test_native_formats_preserve_capacity_and_subformats(self):
        with tempfile.TemporaryDirectory() as work:
            for image_format, qemu_format in (
                    ("raw", "raw"), ("qcow2", "qcow2"), ("vhd", "vpc"), ("vhdx", "vhdx")):
                with self.subTest(image_format=image_format):
                    path = Path(work) / f"disk.{image_format}"
                    reference = Path(work) / f"reference.{image_format}"
                    size = TEST_IMAGE_SIZE
                    images.create(path, size, image_format)
                    if image_format == "raw":
                        with reference.open("wb") as stream:
                            stream.truncate(size)
                    else:
                        command = ["qemu-img", "create", "-f", qemu_format]
                        if image_format == "vhd":
                            command += ["-o", "subformat=fixed"]
                        subprocess.run(command + [str(reference), str(size)],
                                       check=True, capture_output=True)
                    info = json.loads(subprocess.check_output([
                        "qemu-img", "info", "-f", qemu_format, "--output=json", str(path)]))
                    expected = json.loads(subprocess.check_output([
                        "qemu-img", "info", "-f", qemu_format, "--output=json", str(reference)]))
                    self.assertEqual(info["format"], qemu_format)
                    self.assertEqual(info["virtual-size"], expected["virtual-size"])
                    self.assertEqual(path.stat().st_size, reference.stat().st_size)
                    if image_format == "vhd":
                        with path.open("rb") as stream:
                            stream.seek(-512, os.SEEK_END)
                            footer = stream.read(512)
                        self.assertEqual(footer[:8], b"conectix")
                        self.assertEqual(struct.unpack_from(">I", footer, 60)[0], 2)


class ConsoleTests(unittest.TestCase):
    def test_file_following_retains_split_utf8_and_the_final_bytes(self):
        with tempfile.TemporaryDirectory() as work:
            path = Path(work) / "console"
            path.write_bytes(b"First \xe2")
            destination = io.StringIO()
            follower = console.FileFollower(path, destination)
            follower.pump()
            self.assertEqual(destination.getvalue(), "First ")
            with path.open("ab") as stream:
                stream.write(b"\x82\xac\nLast \xff")
            follower.close()
            self.assertEqual(destination.getvalue(), "First \u20ac\nLast \ufffd")
            self.assertTrue(follower.stream.closed)
            self.assertEqual(path.read_bytes(), b"First \xe2\x82\xac\nLast \xff")

    def test_file_following_preserves_raw_console_bytes(self):
        with tempfile.TemporaryDirectory() as work:
            path = Path(work) / "console"
            content = b"Guest \xff\x00\n" * 1000
            path.write_bytes(content)
            destination = Mock()
            destination.buffer = io.BytesIO()
            follower = console.FileFollower(path, destination)
            follower.pump()
            follower.close()
            self.assertEqual(destination.buffer.getvalue(), content)
            destination.write.assert_not_called()

    def test_file_follower_closes_the_log_when_output_fails(self):
        with tempfile.TemporaryDirectory() as work:
            path = Path(work) / "console"
            path.write_bytes(b"Guest output\n")

            class BrokenOutput(io.StringIO):
                def write(self, text):
                    raise OSError("Console sink failed")

            follower = console.FileFollower(path, BrokenOutput())
            with self.assertRaisesRegex(OSError, "Console sink failed"):
                follower.close()
            self.assertTrue(follower.stream.closed)
            self.assertEqual(path.read_bytes(), b"Guest output\n")

    def test_drain_retains_bytes_and_streams_diagnostics_without_rewriting(self):
        content = b"diagnostic\xff\n" * 600
        captured = []
        live = io.BytesIO()
        with patch.object(console.sys, "stderr") as stderr:
            stderr.buffer = live
            console.drain_stream(io.BytesIO(content), captured, True)
        self.assertEqual(b"".join(captured), content)
        self.assertEqual(live.getvalue(), content)
        self.assertEqual(
            console.merge_streams("VV-RESULT token EX001 PASS\n", captured),
            "VV-RESULT token EX001 PASS\n\n" + content.decode(errors="replace"))

    def test_separate_capture_keeps_guest_markers_intact(self):
        guest = b"VV-RESULT token EX001 PASS\n" * 1000
        diagnostic = b"VMM diagnostic\n" * 1000
        command = [
            sys.executable, "-c",
            "import os\n"
            "for _ in range(1000):\n"
            "    os.write(1, b'VV-RESULT token EX001 PASS\\n')\n"
            "    os.write(2, b'VMM diagnostic\\n')\n"]
        with subprocess.Popen(
                command, stdin=subprocess.DEVNULL, stdout=subprocess.PIPE,
                stderr=subprocess.PIPE, start_new_session=True) as child:
            errors = []
            reader = threading.Thread(
                target=console.drain_stream, args=(child.stderr, errors, False))
            reader.start()
            try:
                output = child.stdout.read()
                child.wait(timeout=5)
            finally:
                if child.poll() is None:
                    process.kill_process(child)
                reader.join(timeout=5)
            self.assertFalse(reader.is_alive())
            self.assertEqual(output, guest)
            self.assertEqual(b"".join(errors), diagnostic)


class ProcessTests(unittest.TestCase):
    def test_capture_preserves_streams_exit_status_and_an_isolated_session(self):
        command = [
            sys.executable, "-c",
            "import os\n"
            "os.write(1, b'guest\\xff\\n' * 20000)\n"
            "os.write(2, b'diagnostic\\xfe\\n' * 20000)\n"
            "print(os.getsid(0) == os.getpid(), flush=True)\n"
            "raise SystemExit(3)\n"]
        result = process.capture(command, timeout=5)
        self.assertEqual(result.args, command)
        self.assertEqual(result.returncode, 3)
        self.assertEqual(result.stdout, b"guest\xff\n" * 20000 + b"True\n")
        self.assertEqual(result.stderr, b"diagnostic\xfe\n" * 20000)

    def test_capture_timeout_retains_both_streams_and_reaps_the_process_group(self):
        script = (
            "import ctypes, os, signal, subprocess, sys\n"
            "from villain import process\n"
            "PR_SET_CHILD_SUBREAPER = 36\n"
            "assert ctypes.CDLL(None, use_errno=True).prctl("
            "PR_SET_CHILD_SUBREAPER, 1, 0, 0, 0) == 0\n"
            "code = (\n"
            "    'import os, signal, subprocess, sys\\n'\n"
            "    'child = subprocess.Popen([sys.executable, \"-c\", '\n"
            "    '\"import signal\\\\nsignal.pause()\\\\n\"])\\n'\n"
            "    'print(child.pid, flush=True)\\n'\n"
            "    'os.write(2, b\"diagnostic before timeout\\\\n\")\\n'\n"
            "    'signal.pause()\\n')\n"
            "try:\n"
            "    process.capture([sys.executable, '-u', '-c', code], timeout=1)\n"
            "except subprocess.TimeoutExpired as error:\n"
            "    pid = int(error.stdout)\n"
            "    assert error.stderr == b'diagnostic before timeout\\n'\n"
            "    reaped, status = os.waitpid(pid, 0)\n"
            "    assert reaped == pid and os.waitstatus_to_exitcode(status) == -signal.SIGKILL\n"
            "    assert not process._live_procs\n"
            "else:\n"
            "    raise AssertionError('The deadline was ignored')\n")
        environment = dict(os.environ, PYTHONPATH=str(ROOT / "lib/python"))
        result = subprocess.run(
            [sys.executable, "-c", script], env=environment,
            capture_output=True, text=True, timeout=10)
        self.assertEqual(result.returncode, 0, result.stderr)

    def test_private_terminal_is_owned_by_the_child_and_parent_descriptors_close(self):
        descriptors = []
        openpty = process.pty.openpty

        def terminal():
            pair = openpty()
            descriptors.extend(pair)
            return pair

        command = [
            sys.executable, "-c",
            "import os\n"
            "with open('/dev/tty', 'rb') as terminal:\n"
            "    print(os.isatty(0), os.isatty(terminal.fileno()))\n"]
        with patch.object(process.pty, "openpty", side_effect=terminal):
            result = process.capture(command, terminal=True, timeout=5)
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual(result.stdout, b"True True\n")
        self.assertEqual(len(descriptors), 2)
        for descriptor in descriptors:
            with self.assertRaises(OSError):
                os.fstat(descriptor)

    def test_spawn_failure_closes_private_terminal_descriptors(self):
        descriptors = []
        openpty = process.pty.openpty

        def terminal():
            pair = openpty()
            descriptors.extend(pair)
            return pair

        with patch.object(process.pty, "openpty", side_effect=terminal), \
             self.assertRaises(FileNotFoundError):
            process.capture(["/nonexistent-villain-executable"], terminal=True)
        self.assertEqual(len(descriptors), 2)
        for descriptor in descriptors:
            with self.assertRaises(OSError):
                os.fstat(descriptor)

    def test_capture_interrupt_and_read_error_kill_and_unregister_the_child(self):
        popen = subprocess.Popen
        for error in (KeyboardInterrupt(), OSError("Capture failed")):
            children = []

            def launch(*args, **kwargs):
                child = popen(*args, **kwargs)
                children.append(child)
                return child

            with self.subTest(error=type(error).__name__), \
                 patch.object(process.subprocess, "Popen", side_effect=launch), \
                 patch.object(popen, "communicate", side_effect=error), \
                 self.assertRaises(type(error)):
                process.capture([sys.executable, "-c", "import signal\nsignal.pause()\n"])
            self.assertEqual(len(children), 1)
            child = children[0]
            self.assertEqual(child.poll(), -signal.SIGKILL)
            self.assertNotIn(child, process._live_procs)
            self.assertTrue(child.stdout.closed)
            self.assertTrue(child.stderr.closed)

    def test_stopped_work_never_spawns_a_process_or_terminal(self):
        stop = threading.Event()
        stop.set()
        with patch.object(process.subprocess, "Popen") as popen, \
             patch.object(process.pty, "openpty") as openpty, \
             self.assertRaises(KeyboardInterrupt):
            process.capture(["unused"], terminal=True, stop=stop)
        popen.assert_not_called()
        openpty.assert_not_called()

    def test_stop_during_spawn_reaps_the_child_before_yielding(self):
        stop = threading.Event()
        children = []
        register = process.register_process

        def registered(child, **kwargs):
            register(child, **kwargs)
            children.append(child)
            stop.set()

        with patch.object(process, "register_process", side_effect=registered), \
             self.assertRaises(KeyboardInterrupt):
            process.capture(
                [sys.executable, "-c", "import signal\nsignal.pause()\n"], stop=stop)
        self.assertEqual(len(children), 1)
        self.assertEqual(children[0].poll(), -signal.SIGKILL)
        self.assertNotIn(children[0], process._live_procs)

    def test_inherited_session_cleanup_never_signals_the_parent_process_group(self):
        with patch.object(process.os, "killpg") as killpg:
            with process.spawn(
                    [sys.executable, "-c", "import signal\nsignal.pause()\n"],
                    start_new_session=False) as (child, master):
                self.assertIsNone(master)
                process.kill_all_processes()
                self.assertEqual(child.poll(), -signal.SIGKILL)
            killpg.assert_not_called()
        self.assertNotIn(child, process._live_procs)

    def test_registered_processes_are_killed_and_reaped(self):
        child = subprocess.Popen(
            [sys.executable, "-c", "import time; time.sleep(60)"],
            stdin=subprocess.DEVNULL, stdout=subprocess.DEVNULL,
            stderr=subprocess.DEVNULL, start_new_session=True)
        process.register_process(child)
        try:
            process.kill_all_processes()
            self.assertEqual(child.poll(), -signal.SIGKILL)
        finally:
            process.unregister_process(child)
            if child.poll() is None:
                child.kill()
                child.wait(timeout=5)

    def test_killing_an_already_reaped_process_remains_safe(self):
        child = subprocess.Popen(
            [sys.executable, "-c", "pass"], start_new_session=True,
            stdin=subprocess.DEVNULL, stdout=subprocess.DEVNULL,
            stderr=subprocess.DEVNULL)
        child.wait(timeout=5)
        process.kill_process(child)
        self.assertEqual(child.returncode, 0)


class CompletionQueueTests(unittest.TestCase):
    def test_lazy_submission_has_an_exact_bound_and_releases_consumed_results(self):
        pool = Mock()
        consumed = []

        def items():
            for value in range(10):
                consumed.append(value)
                yield value

        def submit(function, item):
            future = Future()
            future.set_result(function(item))
            return future

        pool.submit.side_effect = submit
        work = execution.CompletionQueue(pool, lambda item: item * 2, items(), limit=3)
        self.assertEqual(consumed, [])
        results = []
        for item, future in work:
            results.append((item, future.result()))
            if len(results) == 1:
                self.assertEqual(len(consumed), 3)
            self.assertLessEqual(len(work.pending), 2)
            self.assertNotIn(future, work.pending)
            self.assertLessEqual(len(consumed) - len(results), 2)
        self.assertEqual(sorted(results), [(value, value * 2) for value in range(10)])
        self.assertEqual(pool.submit.call_count, 10)
        self.assertEqual(work.pending, {})

    def test_completion_order_does_not_wait_for_submission_order(self):
        release = threading.Event()

        def run(item):
            if item == 0:
                if not release.wait(timeout=5):
                    raise RuntimeError("The completed result was not delivered")
            return item

        with ThreadPoolExecutor(max_workers=2) as pool:
            work = execution.CompletionQueue(pool, run, range(2), limit=2)
            try:
                item, future = next(work)
                self.assertEqual((item, future.result()), (1, 1))
            finally:
                release.set()
            self.assertEqual([(item, future.result()) for item, future in work], [(0, 0)])

    def test_stop_drains_submitted_work_without_consuming_new_items(self):
        stop = threading.Event()
        consumed = []

        def items():
            for item in range(8):
                consumed.append(item)
                yield item

        with ThreadPoolExecutor(max_workers=2) as pool:
            work = execution.CompletionQueue(pool, lambda item: item, items(), limit=2, stop=stop)
            first = next(work)
            stop.set()
            results = [first, *work]
        self.assertEqual(consumed, [0, 1])
        self.assertEqual(sorted(future.result() for _, future in results), [0, 1])
        self.assertEqual(work.pending, {})

    def test_interrupted_wait_can_resume_and_drain_for_the_callers_recovery_policy(self):
        stop = threading.Event()
        with ThreadPoolExecutor(max_workers=2) as pool:
            work = execution.CompletionQueue(pool, lambda item: item, range(8), limit=2, stop=stop)
            with patch.object(execution, "wait", side_effect=KeyboardInterrupt), \
                 self.assertRaises(KeyboardInterrupt):
                next(work)
            self.assertEqual(len(work.pending), 2)
            stop.set()
            self.assertEqual(sorted(future.result() for _, future in work), [0, 1])

    def test_worker_errors_remain_on_the_future_without_inventing_a_result(self):
        def run(_):
            raise RuntimeError("Worker failed")

        with ThreadPoolExecutor(max_workers=1) as pool:
            work = execution.CompletionQueue(pool, run, [None], limit=1)
            item, future = next(work)
            self.assertIsNone(item)
            with self.assertRaisesRegex(RuntimeError, "Worker failed"):
                future.result()
            self.assertEqual(list(work), [])

    def test_invalid_limits_fail_before_submitting_work(self):
        for limit in (0, -1, True, 1.5, None):
            with self.subTest(limit=limit), self.assertRaises(ValueError):
                execution.CompletionQueue(Mock(), lambda item: item, [], limit=limit)


if __name__ == "__main__":
    unittest.main()
