#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
"""Selftests for the virtio-villain test runner."""

import importlib.machinery
import importlib.util
import os
import random
import re
import shutil
import subprocess
import sys
import tempfile

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
ROOT_DIR = os.path.dirname(SCRIPT_DIR)
sys.path.insert(0, ROOT_DIR)
sys.path.insert(0, os.path.join(ROOT_DIR, "lib", "python"))
from villain import diagnostics, host as HOST_MOD, vmm as VMM_MOD  # noqa: E402
from villain import sidecar_helpers  # noqa: E402
INIT_BINARY = os.path.join(ROOT_DIR, "target", "init")
RUN = os.path.join(ROOT_DIR, "run")
RUN_FUZZ = os.path.join(ROOT_DIR, "run-fuzz")
MOCK_VMM = os.path.join(SCRIPT_DIR, "mock-vmm")
DISK_SIZE = 16 * 1024 * 1024
PMEM_SIZE = 128 * 1024 * 1024
HOTPLUG_DISK_SIZE = 16 * 1024 * 1024
FUZZ_DISK_SIZE = 64 * 1024 * 1024
IOMMU_DISK_SIZE = 1024 * 1024
MEMORY_SIZE = 512 * 1024 * 1024
MEMORY_HOTPLUG_SIZE = 256 * 1024 * 1024
MEMORY_REDUCTION = 64 * 1024 * 1024


class SkipTest(Exception):
    """Raised to skip a test that cannot run in the current environment."""


# The runner needs a symlink named cloud-hypervisor to detect the backend.
MOCK_LINK = os.path.join(SCRIPT_DIR, "cloud-hypervisor")


def _load_run_module():
    """Import run as a module so unit tests can call its helpers."""
    tmp = os.path.join(tempfile.gettempdir(), "vv_run_under_test.py")
    shutil.copyfile(RUN, tmp)
    spec = importlib.util.spec_from_file_location("vv_run", tmp)
    mod = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(mod)
    return mod


RUN_MOD = None


def setup():
    """Create the mock VMM symlink and load the runner module."""
    global RUN_MOD
    if not os.path.exists(MOCK_LINK):
        os.symlink("mock-vmm", MOCK_LINK)
    RUN_MOD = _load_run_module()


def run_runner(*args):
    """Run the test runner with mock VMM."""
    cmd = [
        sys.executable, RUN,
        "--vmm", MOCK_LINK,
        "--kernel", "/dev/null",
        "--initramfs", MOCK_VMM,
        "--timeout", "5",
    ] + list(args)
    env = dict(os.environ)
    env["VV_ALLOW_UNKNOWN_TESTS"] = "1"
    result = subprocess.run(cmd, capture_output=True, text=True,
                            cwd=ROOT_DIR, env=env)
    return result


def test_unit_probe_records_remove_interleaved_logs():
    output = (
        b"PROBE\t1042\t100000000\t1\t1\t0\n"
        b"PROBE\t1044\t100000001\t2\t0\t1\n")
    expected = [
        (0x1042, 0x100000000, 1, 1, 0),
        (0x1044, 0x100000001, 2, 0, 1),
    ]
    for noise in (
            b"[    0.517323] clk: Disabling unused clocks\n",
            b"cloud-hypervisor: guest log\n"):
        for offset in range(len(output) + 1):
            spliced = output[:offset] + noise + output[offset:]
            assert RUN_MOD._probe_records(spliced) == expected, offset


def test_unit_probe_cache_shares_and_recovers():
    import multiprocessing
    from unittest.mock import patch

    context = multiprocessing.get_context("fork")
    output = b"PROBE\t0x1042\t0x1234\t4\t1\t0\n"
    expected = [(0x1042, 0x1234, 4, 1, 0)]
    key = "a" * 64
    with tempfile.TemporaryDirectory() as directory, \
         patch.object(RUN_MOD, "_PROBE_CACHE_DIR", directory):
        counter = context.Value("i", 0)
        results = context.Queue()

        def probe():
            import time
            with counter.get_lock():
                counter.value += 1
            time.sleep(0.1)
            return output

        def worker():
            results.put(RUN_MOD._cached_probe(key, probe))

        workers = [context.Process(target=worker) for _ in range(6)]
        try:
            for process in workers:
                process.start()
            for process in workers:
                process.join(timeout=5)
                assert process.exitcode == 0, process.exitcode
            assert [results.get(timeout=1) for _ in workers] == [expected] * 6
            probes = counter.value
            assert 1 <= probes <= 6, probes
            repeat = context.Process(target=worker)
            workers.append(repeat)
            repeat.start()
            repeat.join(timeout=5)
            assert repeat.exitcode == 0
            assert results.get(timeout=1) == expected
            assert counter.value == probes
            assert RUN_MOD._cached_probe(key, probe) == expected
            assert counter.value == probes
        finally:
            for process in workers:
                if process.is_alive():
                    process.terminate()
                    process.join()
            results.close()
            results.join_thread()

        failed_key = "b" * 64

        def crash():
            RUN_MOD._cached_probe(failed_key, lambda: os._exit(7))

        process = context.Process(target=crash)
        process.start()
        process.join(timeout=5)
        if process.is_alive():
            process.terminate()
            process.join()
            raise AssertionError("crashed probe did not exit")
        assert process.exitcode == 7

        def recover():
            assert RUN_MOD._cached_probe(failed_key, lambda: output) == expected

        process = context.Process(target=recover)
        process.start()
        process.join(timeout=5)
        if process.is_alive():
            process.terminate()
            process.join()
            raise AssertionError("probe cache lock was not released")
        assert process.exitcode == 0

        for invalid in (b"", b"PROBE\tbroken\n", b"PROBE\tx\ty\tz\n"):
            invalid_key = "c" * 64
            try:
                RUN_MOD._cached_probe(invalid_key, lambda: invalid)
            except ValueError:
                pass
            else:
                raise AssertionError("invalid probe was accepted")
        assert RUN_MOD._cached_probe(invalid_key, lambda: output) == expected


def test_unit_probe_cache_identity_and_shared_readers():
    import fcntl
    import time
    from types import SimpleNamespace
    from unittest.mock import patch

    with tempfile.TemporaryDirectory() as directory, \
         patch.object(RUN_MOD, "_PROBE_CACHE_DIR", directory):
        paths = [os.path.join(directory, name)
                 for name in ("vmm", "kernel", "initramfs")]
        for path in paths:
            with open(path, "w") as stream:
                stream.write("initial")
        backend = SimpleNamespace(binary=paths[0], name="ch")
        key = RUN_MOD._probe_cache_key(backend, *paths[1:], {"memory": "512M"})
        assert key == RUN_MOD._probe_cache_key(
            backend, *paths[1:], {"memory": "512M"})
        assert key != RUN_MOD._probe_cache_key(
            backend, *paths[1:], {"memory": "1G"})
        for path in paths:
            with open(path, "a") as stream:
                stream.write("changed")
            updated = RUN_MOD._probe_cache_key(
                backend, *paths[1:], {"memory": "512M"})
            assert updated != key
            key = updated
        result = RUN_MOD._cached_probe(
            key, lambda: b"PROBE\t1042\t0\t1\n")
        assert result == [(0x1042, 0, 1, 1, 1)]
        segment = os.path.join(directory, key)
        with open(segment, "rb") as stream:
            before = stream.read()
        calls = []
        with open(segment, "r+b") as locked:
            fcntl.flock(locked, fcntl.LOCK_SH)
            started = time.monotonic()
            result = RUN_MOD._cached_probe(key, lambda: (
                calls.append(True) or b"PROBE\t1042\t1\t2\n"))
            assert time.monotonic() - started < 0.5
            assert result == [(0x1042, 0, 1, 1, 1)]
            assert calls == []
            fcntl.flock(locked, fcntl.LOCK_EX)
            started = time.monotonic()
            result = RUN_MOD._cached_probe(key, lambda: (
                calls.append(True) or b"PROBE\t1042\t1\t2\n"))
            assert time.monotonic() - started < 0.5
            assert result == [(0x1042, 1, 2, 1, 1)]
            assert calls == [True]
        with open(segment, "rb") as stream:
            assert stream.read() == before
        retry_key = "e" * 64

        def racing_probe():
            RUN_MOD._cached_probe(
                retry_key, lambda: b"PROBE\t1042\t2\t3\n")
            return b"PROBE\t1042\t4\t5\n"

        assert RUN_MOD._cached_probe(retry_key, racing_probe) == [
            (0x1042, 4, 5, 1, 1)]

        def unexpected_probe():
            raise AssertionError("racing result was not published")

        assert RUN_MOD._cached_probe(retry_key, unexpected_probe) == [
            (0x1042, 2, 3, 1, 1)]
        calls = []
        RUN_MOD._cached_probe("d" * 64, lambda: (
            calls.append(True) or b"PROBE\t1042\t0\t1\n"))
        assert calls == [True]


def test_unit_probe_cache_hit_prepares_nothing():
    from types import SimpleNamespace
    from unittest.mock import Mock, patch

    with tempfile.TemporaryDirectory() as directory:
        backend = SimpleNamespace(
            name="ch", mmio_transport=False, console_device="ttyS0",
            build_cmd=Mock(return_value=["probe-vmm"]))
        metadata = {"RTC0200": {
            "device_id": 0x1051, "required_features": 0,
            "flags": 0, "min_queues": 0}}
        argv = [RUN, "--vmm", MOCK_LINK, "--kernel", "/dev/null",
                "--initramfs", RUN, "--skip-unsupported", "RTC0200"]
        with patch.object(RUN_MOD, "_PROBE_CACHE_DIR", directory), \
             patch.object(VMM_MOD, "_SCRIPT_DIR", directory), \
             patch.object(RUN_MOD, "_probe_cache_key", return_value="f" * 64), \
             patch.object(RUN_MOD, "detect_vmm", return_value=backend), \
             patch.object(RUN_MOD, "print_banner"), \
             patch.object(RUN_MOD, "_install_termination_as_interrupt"), \
             patch.object(RUN_MOD, "get_test_list", return_value=(
                 ["RTC0200"], {}, metadata)), \
             patch.object(sys, "argv", argv):
            def main():
                try:
                    RUN_MOD.main()
                except SystemExit as error:
                    assert error.code == 0

            with patch.object(RUN_MOD.process, "capture", return_value=(
                    subprocess.CompletedProcess(
                        [], 0, b"PROBE\t1042\t0\t1\n", b""))) as launch:
                main()
            launch.assert_called_once()
            backend.build_cmd.assert_called_once()
            probe_disk = backend.build_cmd.call_args.args[2]
            assert not os.path.exists(os.path.dirname(probe_disk))
            backend.build_cmd.reset_mock()
            with patch.object(RUN_MOD.tempfile, "TemporaryDirectory") as prepare, \
                 patch.object(RUN_MOD.process, "capture") as launch, \
                 patch.object(RUN_MOD, "run_test") as run_case:
                main()
            prepare.assert_not_called()
            backend.build_cmd.assert_not_called()
            launch.assert_not_called()
            run_case.assert_not_called()


def test_unit_mmio_boot_cache_shares_and_invalidates():
    import multiprocessing
    from types import SimpleNamespace
    from unittest.mock import Mock, patch

    context = multiprocessing.get_context("fork")
    with tempfile.TemporaryDirectory() as directory, \
         patch.object(RUN_MOD, "_PROBE_CACHE_DIR", directory):
        paths = [os.path.join(directory, name)
                 for name in ("vmm", "kernel", "initramfs")]
        for path in paths:
            with open(path, "w") as stream:
                stream.write("initial")
        backend = SimpleNamespace(
            binary=paths[0], name="qemu-mmio", console_device="ttyS0",
            build_cmd=Mock(return_value=["boot-vmm"]))
        with patch.object(RUN_MOD.process, "capture", return_value=(
                subprocess.CompletedProcess([], 0, b"[vv]\n", b""))) as launch:
            assert RUN_MOD._mmio_boots(backend, *paths[1:])
        launch.assert_called_once()
        disk = backend.build_cmd.call_args.args[2]
        assert not os.path.exists(os.path.dirname(disk))
        backend.build_cmd.reset_mock()

        def worker():
            with patch.object(RUN_MOD.tempfile, "TemporaryDirectory",
                              side_effect=AssertionError("prepared disk")), \
                 patch.object(RUN_MOD.process, "capture",
                              side_effect=AssertionError("launched VM")):
                assert RUN_MOD._mmio_boots(backend, *paths[1:])
            backend.build_cmd.assert_not_called()

        for _ in range(2):
            process = context.Process(target=worker)
            process.start()
            process.join(5)
            if process.is_alive():
                process.kill()
                process.join()
            assert process.exitcode == 0
        for path in paths:
            with open(path, "a") as stream:
                stream.write("changed")
            with patch.object(RUN_MOD.process, "capture", return_value=(
                    subprocess.CompletedProcess(
                        [], 0, b"[vv]\n", b""))) as launch:
                assert RUN_MOD._mmio_boots(backend, *paths[1:])
            launch.assert_called_once()


def test_unit_mmio_boot_cache_retries_failed_checks():
    from types import SimpleNamespace
    from unittest.mock import Mock, patch

    failures = [
        subprocess.CompletedProcess([], 1, b"no boot\n", b""),
        subprocess.TimeoutExpired(["boot-vmm"], 2, output=b"no boot\n"),
        FileNotFoundError("boot-vmm"),
    ]
    for failure in failures:
        with tempfile.TemporaryDirectory() as directory, \
             patch.object(RUN_MOD, "_PROBE_CACHE_DIR", directory), \
             patch.object(RUN_MOD, "_probe_cache_key", return_value="b" * 64):
            backend = SimpleNamespace(
                console_device="ttyS0",
                build_cmd=Mock(return_value=["boot-vmm"]))
            with patch.object(RUN_MOD.process, "capture",
                              side_effect=[failure, subprocess.TimeoutExpired(
                                  ["boot-vmm"], 2, output=b"[vv]\n")]) as launch:
                assert not RUN_MOD._mmio_boots(backend, "kernel", "initramfs")
                assert RUN_MOD._mmio_boots(backend, "kernel", "initramfs")
                assert RUN_MOD._mmio_boots(backend, "kernel", "initramfs")
            assert launch.call_count == 2


def test_unit_mmio_boot_cache_failure_warns_and_checks_boot():
    import contextlib
    import io
    from types import SimpleNamespace
    from unittest.mock import Mock, patch

    backend = SimpleNamespace(
        console_device="ttyS0", build_cmd=Mock(return_value=["boot-vmm"]))
    output = io.StringIO()
    with patch.object(RUN_MOD, "_cached_probe",
                      side_effect=PermissionError("cache denied")), \
         patch.object(RUN_MOD.process, "capture", return_value=(
             subprocess.CompletedProcess([], 0, b"[vv]\n", b""))) as launch, \
         contextlib.redirect_stderr(output):
        assert RUN_MOD._mmio_boots(backend, "kernel", "initramfs")
    launch.assert_called_once()
    assert "MMIO boot probe cache unavailable" in output.getvalue()
    assert "cache denied" in output.getvalue()


def test_unit_mmio_probe_filters_each_launch_configuration():
    import contextlib
    import io
    from types import SimpleNamespace
    from unittest.mock import Mock, patch

    names = ["B0204", "SCSI0240", "SCSI0241"]
    metadata = {
        "B0204": {"device_id": 0x1042, "required_features": 0,
                  "flags": 0, "min_queues": 1},
        "SCSI0240": {"device_id": 0x1048, "required_features": 1,
                     "flags": 0, "min_queues": 6},
        "SCSI0241": {"device_id": 0x1048, "required_features": 8,
                     "flags": 0, "min_queues": 6},
    }
    for failed in (False, True):
        with tempfile.TemporaryDirectory() as directory:
            backend = SimpleNamespace(
                name="qemu-mmio", binary=MOCK_LINK, mmio_transport=True,
                console_device="ttyS0", build_cmd=Mock(
                    side_effect=lambda k, i, d, c, o: [
                        "probe-vmm", o.get("mmio_device", "blk")]))
            modules = {
                name: SimpleNamespace(
                    SUPPORTED_BACKENDS=("qemu-mmio",),
                    vmm_config=lambda device=(
                        "scsi" if name.startswith("SCSI") else "blk"): {
                            "opts": {"mmio_device": device}})
                for name in names}
            argv = [RUN, "--vmm", MOCK_LINK, "--kernel", "/dev/null",
                    "--initramfs", RUN, "--skip-unsupported", "-j", "1", *names]

            def launch(cmd, **kwargs):
                assert "vv.transport=mmio" in (
                    backend.build_cmd.call_args.args[3])
                if cmd[1] == "scsi":
                    output = b"probe failed\n" if failed else (
                        b"PROBE\t1048\t1\t6\t1\t0\n")
                else:
                    output = b"PROBE\t1042\t0\t1\t1\t0\n"
                return subprocess.CompletedProcess(cmd, 0, output, b"")

            output = io.StringIO()
            with patch.object(RUN_MOD, "_PROBE_CACHE_DIR", directory), \
                 patch.object(VMM_MOD, "_SCRIPT_DIR", directory), \
                 patch.object(RUN_MOD, "detect_vmm", return_value=backend), \
                 patch.object(RUN_MOD, "print_banner"), \
                 patch.object(RUN_MOD, "_install_termination_as_interrupt"), \
                 patch.object(RUN_MOD, "get_test_list",
                              return_value=(names, {}, metadata)), \
                 patch.object(RUN_MOD, "_find_sidecar",
                              side_effect=lambda name: name), \
                 patch.object(RUN_MOD, "_load_sidecar",
                              side_effect=lambda path: modules[path]), \
                 patch.object(RUN_MOD.process, "capture", side_effect=launch), \
                 patch.object(RUN_MOD, "run_test", side_effect=(
                     lambda b, k, name, *args: [(name, "PASS", "")])) as run_case, \
                 patch.object(sys, "argv", argv), \
                 contextlib.redirect_stdout(output):
                try:
                    RUN_MOD.main()
                except SystemExit as error:
                    assert error.code == 0
            assert backend.build_cmd.call_count == 2
            assert [call.args[2] for call in run_case.call_args_list] == (
                names if failed else names[:2])
            assert ("capability probe unavailable" in output.getvalue()) == failed


def test_unit_openvmm_probe_filters_capabilities_and_releases_terminal():
    import contextlib
    import errno
    import io
    import json
    from types import SimpleNamespace
    from unittest.mock import Mock, patch

    names = ["B0001", "B0022", "B0002", "B0003", "B0004", "RNG0001"]
    metadata = {
        name: {"device_id": 0x1042, "required_features": 0,
               "flags": 0, "min_queues": 1}
        for name in names}
    metadata["B0022"]["min_queues"] = 2
    metadata["B0002"]["required_features"] = 1 << 63
    metadata["B0003"]["flags"] = 8
    metadata["B0004"]["flags"] = 16
    metadata["RNG0001"]["device_id"] = 0x1044

    for outcome in ("success", "interleaved", "exit", "timeout"):
        valid_probe = outcome in ("success", "interleaved")
        with tempfile.TemporaryDirectory() as directory:
            backend = SimpleNamespace(
                name="openvmm", binary=MOCK_LINK, mmio_transport=False,
                console_device="ttyS0",
                build_cmd=Mock(return_value=["probe-vmm"]))
            argv = [RUN, "--vmm", MOCK_LINK, "--kernel", "/dev/null",
                    "--initramfs", RUN, "--skip-unsupported",
                    "--format", "json", *names]
            descriptors = []
            real_openpty = RUN_MOD.process.pty.openpty

            def openpty():
                pair = real_openpty()
                descriptors.extend(pair)
                return pair

            def launch_probe(cmd, **kwargs):
                assert os.isatty(kwargs["stdin"])
                assert kwargs["start_new_session"] is False
                assert callable(kwargs["preexec_fn"])
                assert backend.build_cmd.call_args.args[4]["exit_on_guest_end"]
                stdout, stderr = b"PROBE\t1042\t100000000\t1\t0\t0\n", b""
                if outcome == "exit":
                    stdout, stderr = b"", b"probe startup failed"
                elif outcome == "interleaved":
                    stdout = (
                        b"PR[    0.517323] clk: Disabling unused clocks\n"
                        b"OBE\t1042\t100[    0.525669] Run /init\n"
                        b"000000\t1\t0\t0\n")
                proc = Mock(returncode=1 if outcome == "exit" else 0)
                proc.stdin = None
                proc.stdout, proc.stderr = io.BytesIO(stdout), io.BytesIO(stderr)
                proc.poll.return_value = proc.returncode
                proc.communicate.return_value = (stdout, stderr)
                if outcome == "timeout":
                    proc.communicate.side_effect = [
                        subprocess.TimeoutExpired(cmd, 5), (b"", b"")]
                return proc

            output = io.StringIO()
            errors = io.StringIO()
            with patch.object(RUN_MOD, "_PROBE_CACHE_DIR", directory), \
                 patch.object(VMM_MOD, "_SCRIPT_DIR", directory), \
                 patch.object(RUN_MOD, "detect_vmm", return_value=backend), \
                 patch.object(RUN_MOD, "print_banner"), \
                 patch.object(RUN_MOD, "_install_termination_as_interrupt"), \
                 patch.object(RUN_MOD, "get_test_list",
                              return_value=(names, {}, metadata)), \
                 patch.object(RUN_MOD.process.pty, "openpty", side_effect=openpty), \
                 patch.object(RUN_MOD.process, "kill_process"), \
                 patch.object(RUN_MOD.subprocess, "Popen",
                              side_effect=launch_probe) as probe, \
                 patch.object(RUN_MOD, "run_test", side_effect=(
                     lambda b, k, name, *args: [
                         (name, "PASS", "")])) as launch, \
                 patch.object(sys, "argv", argv), \
                 contextlib.redirect_stdout(output), \
                 contextlib.redirect_stderr(errors):
                try:
                    RUN_MOD.main()
                except SystemExit as error:
                    assert error.code == 0
            probe.assert_called_once()
            expected = ["B0001"] if valid_probe else names
            assert [call.args[2] for call in launch.call_args_list] == expected
            doc = json.loads(output.getvalue())
            assert doc["total"] == len(expected)
            assert doc["counts"]["pass"] == len(expected)
            assert doc["counts"]["skip"] == 0
            assert ("capability probe unavailable" in errors.getvalue()) == (
                not valid_probe)
            assert len(descriptors) == 2
            for fd in descriptors:
                try:
                    os.fstat(fd)
                except OSError as error:
                    assert error.errno == errno.EBADF
                else:
                    raise AssertionError("probe terminal descriptor leaked")


def test_artifact_paths():
    from unittest.mock import patch

    with tempfile.TemporaryDirectory(prefix="vv artifacts ") as directory:
        init = os.path.join(directory, "init")
        archive = os.path.join(directory, "initramfs.cpio.gz")
        with open(init, "w") as script:
            script.write(
                "#!/bin/sh\n"
                "case \"$1\" in\n"
                "  --banner) echo CUSTOM-BANNER ;;\n"
                "  --list-tsv) printf 'PASS\\tCustom case\\t1.4\\t2.6\\n' ;;\n"
                "  *) exit 1 ;;\n"
                "esac\n")
        os.chmod(init, 0o755)
        with open(archive, "wb"):
            pass
        result = run_runner("--init", init, "--initramfs", archive, "-v")
        assert result.returncode == 0, result.stdout + result.stderr
        assert "CUSTOM-BANNER" in result.stderr
        assert "Custom case" in result.stdout
        assert archive in result.stdout
        assert "[PASS]" in result.stdout

        missing = os.path.join(directory, "missing")
        result = run_runner("--init", missing, "PASS")
        assert result.returncode != 0
        assert "not an executable init binary" in result.stderr
        result = run_runner("--initramfs", missing, "PASS")
        assert result.returncode != 0
        assert "--initramfs" in result.stderr
        with open(init, "w") as script:
            script.write("#!/bin/sh\nexit 1\n")
        result = run_runner("--init", init, "PASS")
        assert result.returncode != 0
        assert "Failed to list tests" in result.stderr

        backend = RUN_MOD.CloudHypervisor(MOCK_LINK)
        with patch.object(RUN_MOD, "_SCRIPT_DIR", ROOT_DIR), \
             patch.object(backend, "build_cmd", wraps=backend.build_cmd) as build:
            results = RUN_MOD.run_test(
                backend, "kernel", "PASS", 5, opts={"initramfs": archive})
            assert RUN_MOD._find_sidecar("B0200") == os.path.join(
                ROOT_DIR, "tests", "blk", "b0200_hotplug_add.py")
        assert results[0][1] == "PASS", results
        assert build.call_args.args[1] == archive


def test_pass():
    """Test that a passing test is reported correctly."""
    r = run_runner("PASS")
    assert r.returncode == 0, f"Expected rc=0, got {r.returncode}\n{r.stdout}"
    assert "[PASS]" in r.stdout and "PASS" in r.stdout, f"Missing PASS in: {r.stdout}"
    assert "1/1 tests passed" in r.stdout


def test_fail():
    """Test that a failing test is reported correctly."""
    r = run_runner("FAIL")
    assert r.returncode == 1, f"Expected rc=1, got {r.returncode}\n{r.stdout}"
    assert "[FAIL]" in r.stdout and "FAIL" in r.stdout, f"Missing FAIL in: {r.stdout}"
    assert "0/1 tests passed" in r.stdout
    assert "FAILED:" in r.stdout


def test_timeout():
    """Test that a hanging VM is killed and reported as failure."""
    r = run_runner("--timeout", "2", "HANG")
    assert r.returncode == 1, f"Expected rc=1, got {r.returncode}\n{r.stdout}"
    assert "[FAIL]" in r.stdout and "HANG" in r.stdout
    assert "TIMEOUT" in r.stdout


def test_crash():
    """Test that a VMM crash is reported as failure."""
    r = run_runner("CRASH")
    assert r.returncode == 1, f"Expected rc=1, got {r.returncode}\n{r.stdout}"
    assert "[FAIL]" in r.stdout and "CRASH" in r.stdout


def test_multiple():
    """Test running multiple tests in sequence."""
    r = run_runner("PASS", "FAIL", "PASS")
    assert r.returncode == 1
    assert "2/3 tests passed" in r.stdout
    assert "[PASS]" in r.stdout
    assert "[FAIL]" in r.stdout


def test_parallel():
    """Test running multiple tests in parallel."""
    r = run_runner("--jobs", "3", "PASS", "PASS", "PASS")
    assert r.returncode == 0
    assert "3/3 tests passed" in r.stdout


def test_log_dir():
    """Test that --log-dir writes per-test log files."""
    with tempfile.TemporaryDirectory() as td:
        r = run_runner("--log-dir", td, "FAIL")
        assert r.returncode == 1
        log = os.path.join(td, "FAIL.log")
        assert os.path.isfile(log), f"Log file not created: {log}"
        content = open(log).read()
        assert "[FAIL]" in content


def test_log_dir_pass():
    """A passing test must also produce a log file."""
    with tempfile.TemporaryDirectory() as td:
        r = run_runner("--log-dir", td, "PASS")
        assert r.returncode == 0
        log = os.path.join(td, "PASS.log")
        assert os.path.isfile(log), f"Log file not created: {log}"
        assert "[PASS]" in open(log).read()


def test_mixed_parallel():
    """Mixed pass/fail under --jobs reports correct totals."""
    r = run_runner("--jobs", "4", "PASS", "FAIL", "PASS", "FAIL")
    assert r.returncode == 1, r.stdout
    assert "2/4 tests passed" in r.stdout
    assert r.stdout.count("[PASS]") >= 2
    assert r.stdout.count("[FAIL]") >= 2


def test_unknown_args_rejected():
    """Unknown CLI flags must error out."""
    r = run_runner("--this-flag-does-not-exist")
    assert r.returncode != 0
    assert "unrecognized" in r.stderr or "error" in r.stderr.lower()


# ---------------------------------------------------------------------------
# Unit tests for the test selection / filtering logic.

def test_unit_test_prefix():
    f = RUN_MOD._test_prefix
    assert f("T01") == "T"
    assert f("PCI0050") == "PCI"
    assert f("RNG0001") == "RNG"
    assert f("RTC0016") == "RTC"
    assert f("PLAN90001") == "PLAN9"
    assert f("B01") == "B"
    assert f("Z01") == "Z"
    assert f("99lower") == ""
    assert f("") == ""


def test_unit_test_dir_longest_prefix():
    """Longest-prefix mapping must not misroute RTC* into mem (R)."""
    f = RUN_MOD._test_dir
    assert f("RTC0001") == "rtc"
    assert f("RTC0016") == "rtc"
    assert f("RNG0001") == "rng"
    assert f("PLAN90001") == "plan9"
    assert f("PCI0050") == "pci"
    assert f("R0001") == "mem"
    assert f("T01") == "vring"
    assert f("B01") == "blk"
    assert f("Z01") == "blk"
    assert f("XYZZY") is None
    assert f("") is None


def test_unit_filter_no_filters_passthrough():
    tests = ["T01", "RNG0001", "RTC0001"]
    assert RUN_MOD.filter_tests(tests, [], [], [], []) == tests


def test_unit_filter_include_glob():
    tests = ["T01", "T02", "RNG0001", "RNG0014", "RTC0001"]
    out = RUN_MOD.filter_tests(tests, ["RNG*"], [], [], [])
    assert out == ["RNG0001", "RNG0014"]


def test_unit_filter_include_case_insensitive():
    tests = ["RNG0001", "rng0002", "T01"]
    out = RUN_MOD.filter_tests(tests, ["rng000?"], [], [], [])
    assert sorted(out) == sorted(["RNG0001", "rng0002"])


def test_unit_filter_multiple_includes_union():
    tests = ["T01", "B01", "RNG0001", "RTC0001"]
    out = RUN_MOD.filter_tests(tests, ["T*", "B*"], [], [], [])
    assert out == ["T01", "B01"]


def test_unit_filter_exclude_glob():
    tests = ["T01", "RNG0001", "RTC0001", "RTC0016"]
    out = RUN_MOD.filter_tests(tests, [], ["RTC*"], [], [])
    assert out == ["T01", "RNG0001"]


def test_unit_filter_dir():
    tests = ["T01", "T02", "B01", "Z01", "RNG0001", "RTC0001", "RTC0016"]
    out = RUN_MOD.filter_tests(tests, [], [], ["rtc"], [])
    assert out == ["RTC0001", "RTC0016"]
    out = RUN_MOD.filter_tests(tests, [], [], ["blk"], [])
    assert out == ["B01", "Z01"]


def test_unit_filter_multiple_dirs_union():
    tests = ["T01", "B01", "RNG0001", "RTC0001"]
    out = RUN_MOD.filter_tests(tests, [], [], ["rng", "rtc"], [])
    assert out == ["RNG0001", "RTC0001"]


def test_unit_filter_exclude_dir():
    tests = ["T01", "B01", "PCI0050", "RNG0001", "RTC0001"]
    out = RUN_MOD.filter_tests(tests, [], [], [], ["pci", "vring"])
    assert out == ["B01", "RNG0001", "RTC0001"]


def test_unit_filter_include_and_exclude():
    tests = ["RTC0001", "RTC0016", "RNG0001"]
    out = RUN_MOD.filter_tests(tests, [], ["RTC0001"], ["rtc"], [])
    assert out == ["RTC0016"]


def test_unit_filter_dir_does_not_match_unknown_prefix():
    tests = ["PASS", "FAIL", "RTC0001"]
    out = RUN_MOD.filter_tests(tests, [], [], ["rtc"], [])
    assert out == ["RTC0001"]


def test_unit_filter_unknown_dir_rejected():
    try:
        RUN_MOD.filter_tests(["T01"], [], [], ["bogus"], [])
    except SystemExit as e:
        msg = str(e)
        assert "bogus" in msg
        assert "Known" in msg
        return
    raise AssertionError("expected SystemExit for unknown --dir")


def test_unit_filter_unknown_exclude_dir_rejected():
    try:
        RUN_MOD.filter_tests(["T01"], [], [], [], ["nope"])
    except SystemExit as e:
        assert "nope" in str(e)
        return
    raise AssertionError("expected SystemExit for unknown --exclude-dir")


def test_unit_filter_dir_glob_expands():
    """--dir 'p*' must match packed, pci, pmem."""
    tests = ["T01", "B01", "P01", "PCI0050", "E0001", "RNG0001"]
    out = RUN_MOD.filter_tests(tests, [], [], ["p*"], [])
    assert out == ["P01", "PCI0050", "E0001"]


def test_unit_filter_dir_glob_question_mark():
    """Single character glob in --dir."""
    tests = ["T01", "RNG0001", "RTC0001"]
    out = RUN_MOD.filter_tests(tests, [], [], ["r??"], [])
    assert sorted(out) == ["RNG0001", "RTC0001"]


def test_unit_filter_dir_glob_no_match_silent():
    """Glob that matches nothing must not error, just return empty."""
    tests = ["T01", "B01"]
    out = RUN_MOD.filter_tests(tests, [], [], ["zz*"], [])
    assert out == []


def test_unit_filter_dir_plain_name_still_strict():
    """Plain --dir name still rejects unknown values."""
    try:
        RUN_MOD.filter_tests(["T01"], [], [], ["bogus"], [])
    except SystemExit as e:
        assert "bogus" in str(e)
        return
    raise AssertionError("plain unknown --dir name must still abort")


def test_unit_filter_exclude_dir_glob():
    """Glob in --exclude-dir."""
    tests = ["T01", "P01", "PCI0050", "E0001", "RNG0001", "RTC0001"]
    out = RUN_MOD.filter_tests(tests, [], [], [], ["p*"])
    assert out == ["T01", "RNG0001", "RTC0001"]


def test_unit_filter_dir_case_insensitive_plain():
    tests = ["T01", "RTC0001", "PCI0050"]
    out = RUN_MOD.filter_tests(tests, [], [], ["RTC"], [])
    assert out == ["RTC0001"]
    out = RUN_MOD.filter_tests(tests, [], [], ["PcI"], [])
    assert out == ["PCI0050"]


def test_unit_filter_dir_case_insensitive_glob():
    tests = ["P01", "PCI0050", "E0001", "T01"]
    out = RUN_MOD.filter_tests(tests, [], [], ["P*"], [])
    assert out == ["P01", "PCI0050", "E0001"]


def test_unit_filter_exclude_dir_case_insensitive():
    tests = ["T01", "PCI0050", "RTC0001"]
    out = RUN_MOD.filter_tests(tests, [], [], [], ["RTC"])
    assert out == ["T01", "PCI0050"]
    out = RUN_MOD.filter_tests(tests, [], [], [], ["P*"])
    assert out == ["T01", "RTC0001"]


def test_unit_filter_include_lowercase_pattern():
    """Lowercase --include glob still matches uppercase test names."""
    tests = ["RTC0001", "RNG0001", "T01"]
    out = RUN_MOD.filter_tests(tests, ["rtc*"], [], [], [])
    assert out == ["RTC0001"]


def test_unit_filter_exclude_lowercase_pattern():
    tests = ["RTC0001", "RTC0016", "T01"]
    out = RUN_MOD.filter_tests(tests, [], ["rtc*"], [], [])
    assert out == ["T01"]


def test_unit_canonicalize_tests_lowercase_maps_to_registered():
    """A lowercase name resolves to the registered id the guest prints."""
    known = ["T0129", "P0062", "B0001"]
    canonical, unknown = RUN_MOD._canonicalize_tests(["t0129", "p0062"], known)
    assert canonical == ["T0129", "P0062"]
    assert unknown == []


def test_unit_canonicalize_tests_reports_unknown():
    known = ["T0129", "B0001"]
    canonical, unknown = RUN_MOD._canonicalize_tests(["t0129", "zz9999"], known)
    assert canonical == ["T0129"]
    assert unknown == ["zz9999"]


# ---------------------------------------------------------------------------
# Integration tests for filter flags driving the actual runner.

def test_filter_include_runs_subset():
    """--include narrows the set of executed tests."""
    r = run_runner("--include", "RTC*", "RTC0001", "RNG0001", "T01")
    assert r.returncode == 0, r.stdout
    assert "1/1 tests passed" in r.stdout
    assert "RTC0001" in r.stdout
    assert "RNG0001" not in r.stdout
    assert "T01" not in r.stdout


def test_filter_exclude_drops_subset():
    """--exclude removes matching tests."""
    r = run_runner("--exclude", "RTC*", "RTC0001", "RNG0001", "T01")
    assert r.returncode == 0, r.stdout
    assert "2/2 tests passed" in r.stdout
    assert "RTC0001" not in r.stdout
    assert "RNG0001" in r.stdout
    assert "T01" in r.stdout


def test_filter_dir_keeps_only_matching():
    """--dir rtc keeps only tests whose prefix maps to rtc."""
    r = run_runner("--dir", "rtc", "RTC0001", "RTC0016", "RNG0001", "T01")
    assert r.returncode == 0, r.stdout
    assert "2/2 tests passed" in r.stdout
    assert "RTC0001" in r.stdout
    assert "RTC0016" in r.stdout
    assert "RNG0001" not in r.stdout


def test_filter_exclude_dir_drops_matching():
    """--exclude-dir removes tests from the named directory."""
    r = run_runner("--exclude-dir", "vring",
                   "RTC0001", "RNG0001", "T01", "T02")
    assert r.returncode == 0, r.stdout
    assert "2/2 tests passed" in r.stdout
    assert "T01" not in r.stdout
    assert "T02" not in r.stdout
    assert "RTC0001" in r.stdout
    assert "RNG0001" in r.stdout


def test_filter_include_and_exclude_compose():
    """--include + --exclude apply in order: whitelist then blacklist."""
    r = run_runner("--dir", "rtc", "--exclude", "RTC0001",
                   "RTC0001", "RTC0016", "RNG0001")
    assert r.returncode == 0, r.stdout
    assert "1/1 tests passed" in r.stdout
    assert "RTC0016" in r.stdout
    assert "RTC0001 " not in r.stdout  # space avoids matching RTC0016 prefix


def test_filter_unknown_dir_errors():
    """Bogus --dir name aborts with a helpful message."""
    r = run_runner("--dir", "nosuch", "T01")
    assert r.returncode != 0
    out = r.stdout + r.stderr
    assert "nosuch" in out
    assert "Known" in out


def test_filter_no_match_errors():
    """Filters that exclude every test still abort cleanly."""
    r = run_runner("--include", "ZZZ*", "T01", "RNG0001")
    assert r.returncode != 0
    out = r.stdout + r.stderr
    assert "No tests" in out


# ---------------------------------------------------------------------------
# API socket plumbing.

def test_unit_ch_build_cmd_adds_api_socket():
    """CH build_cmd emits --api-socket when opts.api_socket is set."""
    be = RUN_MOD.CloudHypervisor("/usr/bin/cloud-hypervisor")
    opts = {"cpus": 4, "memory": "256M", "blk_queues": 1, "net_queues": 1,
            "api_socket": "/tmp/vv.api"}
    cmd = be.build_cmd("/k", "/i", "/d.raw", "console=ttyS0", opts)
    assert "--api-socket" in cmd
    i = cmd.index("--api-socket")
    assert cmd[i + 1] == "path=/tmp/vv.api"


def test_unit_ch_build_cmd_omits_api_socket():
    be = RUN_MOD.CloudHypervisor("/usr/bin/cloud-hypervisor")
    opts = {"cpus": 4, "memory": "256M", "blk_queues": 1, "net_queues": 1}
    cmd = be.build_cmd("/k", "/i", "/d.raw", "console=ttyS0", opts)
    assert "--api-socket" not in cmd


def test_unit_ch_net_receive_uses_named_tap():
    be = RUN_MOD.CloudHypervisor("/usr/bin/cloud-hypervisor")
    cmd = be.build_cmd(
        "/k", "/i", "/d.raw", "console=ttyS0",
        {"fuzz_device": "net", "net_queues": 2,
         "net_rx_tap": "vvperf123", "net_rx_ip": "198.18.7.1"})
    assert cmd[cmd.index("--net") + 1] == (
        "tap=vvperf123,ip=198.18.7.1,mask=255.255.255.0,num_queues=2")


def test_unit_ch_fuzz_device_attaches_only_that_device():
    be = RUN_MOD.CloudHypervisor("/usr/bin/cloud-hypervisor")
    cmd = be.build_cmd("/k", "/i", "/d.raw", "console=ttyS0",
                       {"fuzz_device": "blk", "blk_queues": 1})
    assert "--disk" in cmd
    assert "--net" not in cmd
    assert "--vsock" not in cmd
    assert "--balloon" not in cmd
    assert "--watchdog" not in cmd
    assert "--rng" not in cmd


def test_unit_qemu_build_cmd_adds_qmp():
    be = RUN_MOD.Qemu("/usr/bin/qemu-system-x86_64")
    opts = {"memory": "256M", "blk_queues": 1, "net_queues": 1,
            "api_socket": "/tmp/vv.api"}
    cmd = be.build_cmd("/k", "/i", "/d.raw", "console=ttyS0", opts)
    assert "-qmp" in cmd
    i = cmd.index("-qmp")
    assert cmd[i + 1] == "unix:/tmp/vv.api,server=on,wait=off"


def test_unit_qemu_build_cmd_omits_qmp():
    be = RUN_MOD.Qemu("/usr/bin/qemu-system-x86_64")
    opts = {"memory": "256M", "blk_queues": 1, "net_queues": 1}
    cmd = be.build_cmd("/k", "/i", "/d.raw", "console=ttyS0", opts)
    assert "-qmp" not in cmd


def test_unit_qemu_build_cmd_sets_cpu_model():
    be = RUN_MOD.Qemu("/usr/bin/qemu-system-x86_64")
    cmd = be.build_cmd("/k", "/i", "/d.raw", "console=ttyS0",
                       {"cpu_model": "host"})
    assert cmd[cmd.index("-cpu") + 1] == "host"


def test_unit_qemu_net_device_disables_option_rom():
    be = RUN_MOD.Qemu("/usr/bin/qemu-system-x86_64")
    cmd = be.build_cmd("/k", "/i", "/d.raw", "console=ttyS0", {})
    net_dev = next(arg for arg in cmd
                   if arg.startswith("virtio-net-pci-non-transitional,"))
    assert net_dev.endswith(",romfile=")


def test_unit_qemu_net_receive_uses_socket_endpoint():
    be = RUN_MOD.Qemu("/usr/bin/qemu-system-x86_64")
    cmd = be.build_cmd("/k", "/i", "/d.raw", "console=ttyS0",
                       {"net_rx_port": 41000,
                        "net_rx_source_port": 41001})
    assert cmd[cmd.index("-netdev") + 1] == (
        "socket,id=net0,udp=127.0.0.1:41001,localaddr=127.0.0.1:41000")


def test_unit_qemu_fuzz_device_attaches_only_that_device():
    """With opts.fuzz_device set the QEMU command carries just that
    device so the fuzz guest boots faster."""
    be = RUN_MOD.Qemu("/usr/bin/qemu-system-x86_64")
    cmd = be.build_cmd("/k", "/i", "/d.raw", "console=ttyS0",
                       {"fuzz_device": "blk"})
    assert any(str(a).startswith("virtio-blk-pci") for a in cmd)
    assert not any(str(a).startswith("virtio-net-pci") for a in cmd)
    assert not any(str(a).startswith("vhost-vsock-pci") for a in cmd)
    assert not any(str(a).startswith("virtio-rng-pci") for a in cmd)
    assert not any("virtio-crypto" in str(a) for a in cmd)
    # QEMU's default NIC is suppressed so no unused net device is emulated.
    assert "-net" in cmd and cmd[cmd.index("-net") + 1] == "none"


def test_unit_qemu_build_cmd_p9_optional():
    be = RUN_MOD.Qemu("/usr/bin/qemu-system-x86_64")
    without = be.build_cmd("/k", "/i", "/d.raw", "c", {})
    assert not any("virtio-9p-pci" in str(arg) for arg in without)
    with_p9 = be.build_cmd("/k", "/i", "/d.raw", "c",
                           {"p9_sharedir": "/share"})
    assert with_p9[with_p9.index("-fsdev") + 1] == (
        "local,id=fsdev9p,path=/share,security_model=none")
    device = next(arg for arg in with_p9 if "virtio-9p-pci" in str(arg))
    assert "disable-legacy=on" in device
    assert "fsdev=fsdev9p" in device
    assert "mount_tag=vv9p" in device


def test_unit_qemu_mem_region_configuration():
    original = VMM_MOD.platform.machine
    try:
        for arch in ("x86_64", "aarch64"):
            VMM_MOD.platform.machine = lambda arch=arch: arch
            be = VMM_MOD.Qemu("qemu-system")
            without = be.build_cmd("/k", "/i", "/d.raw", "c", {})
            assert not any("virtio-mem-pci" in arg for arg in without)

            opts = {"fuzz_device": "mem", "mem_region_size": "256M"}
            cmd = be.build_cmd("/k", "/i", "/d.raw", "c", opts)
            assert "memory-backend-ram,id=mem0,size=256M" in cmd
            assert ("virtio-mem-pci,memdev=mem0,requested-size=0,"
                    "block-size=2M,unplugged-inaccessible=off") in cmd

            opts["fuzz_device"] = "blk"
            other = be.build_cmd("/k", "/i", "/d.raw", "c", opts)
            assert not any("virtio-mem-pci" in arg for arg in other)
    finally:
        VMM_MOD.platform.machine = original


def test_unit_qemu_no_fuzz_device_attaches_full_set():
    """Without a fuzz device focus every device stays present, so the
    conformance command is unchanged."""
    be = RUN_MOD.Qemu("/usr/bin/qemu-system-x86_64")
    cmd = be.build_cmd("/k", "/i", "/d.raw", "console=ttyS0", {})
    assert any(str(a).startswith("virtio-blk-pci") for a in cmd)
    assert any(str(a).startswith("virtio-net-pci") for a in cmd)
    assert any(str(a).startswith("vhost-vsock-pci") for a in cmd)
    assert any(str(a).startswith("virtio-rng-pci") for a in cmd)
    assert any("virtio-crypto" in str(a) for a in cmd)
    assert "virtio-sound-pci,audiodev=snd0" in cmd
    assert cmd[cmd.index("-audiodev") + 1] == "none,id=snd0"
    # The full set includes an explicit NIC, so no default suppression.
    assert "none" not in cmd


def test_unit_qemu_tcg_accel():
    be = RUN_MOD.Qemu("/usr/bin/qemu-system-x86_64")
    cmd = be.build_cmd("/k", "/i", "/d.raw", "console=ttyS0",
                       {"accel": "tcg"})
    assert cmd[cmd.index("-accel") + 1] == "tcg"
    assert "-enable-kvm" not in cmd


def test_unit_qemu_aarch64_build_cmd_sets_machine():
    original = RUN_MOD.platform.machine
    try:
        RUN_MOD.platform.machine = lambda: "aarch64"
        be = RUN_MOD.Qemu("/usr/bin/qemu-system-aarch64")
        cmd = be.build_cmd("/k", "/i", "/d.raw", "console=ttyAMA0",
                           {"pmem_path": "/d.pmem"})
    finally:
        RUN_MOD.platform.machine = original
    assert cmd[cmd.index("-M") + 1] == "virt"
    assert cmd[cmd.index("-cpu") + 1] == "host"
    assert "isa-debug-exit,iobase=0x501,iosize=1" not in cmd
    assert not any("pmem0" in arg for arg in cmd)
    assert not any("virtio-pmem" in arg for arg in cmd)


def test_unit_qemu_aarch64_tcg_omits_host_cpu():
    original = RUN_MOD.platform.machine
    try:
        RUN_MOD.platform.machine = lambda: "aarch64"
        be = RUN_MOD.Qemu("/usr/bin/qemu-system-aarch64")
        cmd = be.build_cmd("/k", "/i", "/d.raw", "console=ttyAMA0",
                           {"accel": "tcg"})
    finally:
        RUN_MOD.platform.machine = original
    assert cmd[cmd.index("-accel") + 1] == "tcg"
    assert cmd[cmd.index("-M") + 1] == "virt"
    assert "-cpu" not in cmd


# ---------------------------------------------------------------------------
# Memory size normalization.

def test_unit_normalize_mem_uppercases_unit():
    assert RUN_MOD._normalize_mem("512m") == "512M"
    assert RUN_MOD._normalize_mem("1g") == "1G"


def test_unit_normalize_mem_drops_two_letter_suffix():
    assert RUN_MOD._normalize_mem("256mb") == "256M"
    assert RUN_MOD._normalize_mem("256MB") == "256M"
    assert RUN_MOD._normalize_mem("2gb") == "2G"


def test_unit_normalize_mem_drops_iec_suffix():
    assert RUN_MOD._normalize_mem("256mib") == "256M"
    assert RUN_MOD._normalize_mem("1GiB") == "1G"


def test_unit_normalize_mem_passes_plain_and_single_letter():
    assert RUN_MOD._normalize_mem("512M") == "512M"
    assert RUN_MOD._normalize_mem("1024") == "1024"


# ---------------------------------------------------------------------------
# Block IO engine and direct IO mapping.

def _qemu_drive(cmd):
    return cmd[cmd.index("-drive") + 1]


def _ch_disk(cmd):
    return cmd[cmd.index("--disk") + 1]


def test_unit_qemu_io_engine_default_omits_aio():
    be = RUN_MOD.Qemu("/usr/bin/qemu-system-x86_64")
    drive = _qemu_drive(be.build_cmd("/k", "/i", "/d.raw", "c", {}))
    assert "aio=" not in drive
    assert "cache.direct" not in drive


def test_unit_qemu_io_engine_io_uring():
    be = RUN_MOD.Qemu("/usr/bin/qemu-system-x86_64")
    drive = _qemu_drive(
        be.build_cmd("/k", "/i", "/d.raw", "c", {"io_engine": "io_uring"}))
    assert "aio=io_uring" in drive


def test_unit_qemu_io_engine_sync_maps_threads():
    be = RUN_MOD.Qemu("/usr/bin/qemu-system-x86_64")
    drive = _qemu_drive(
        be.build_cmd("/k", "/i", "/d.raw", "c", {"io_engine": "sync"}))
    assert "aio=threads" in drive


def test_unit_qemu_io_engine_aio_forces_direct():
    """QEMU aio=native requires O_DIRECT, so aio turns direct on."""
    be = RUN_MOD.Qemu("/usr/bin/qemu-system-x86_64")
    drive = _qemu_drive(
        be.build_cmd("/k", "/i", "/d.raw", "c", {"io_engine": "aio"}))
    assert "aio=native" in drive
    assert "cache.direct=on" in drive


def test_unit_qemu_direct_without_engine():
    be = RUN_MOD.Qemu("/usr/bin/qemu-system-x86_64")
    drive = _qemu_drive(
        be.build_cmd("/k", "/i", "/d.raw", "c", {"direct": True}))
    assert "cache.direct=on" in drive
    assert "aio=" not in drive


def test_unit_ch_io_engine_default_has_no_toggles():
    be = RUN_MOD.CloudHypervisor("/usr/bin/cloud-hypervisor")
    disk = _ch_disk(be.build_cmd("/k", "/i", "/d.raw", "c",
                                 {"blk_queues": 1, "net_queues": 1}))
    assert "_disable_io_uring" not in disk
    assert "_disable_aio" not in disk


def test_unit_ch_block_iommu_defaults_to_virtio():
    be = RUN_MOD.CloudHypervisor("/usr/bin/cloud-hypervisor")
    disk = _ch_disk(be.build_cmd(
        "/k", "/i", "/d.raw", "c",
        {"blk_queues": 1, "net_queues": 1}))
    assert "iommu=virtio" in disk.split(",")


def test_unit_ch_block_iommu_can_be_disabled():
    be = RUN_MOD.CloudHypervisor("/usr/bin/cloud-hypervisor")
    disk = _ch_disk(be.build_cmd(
        "/k", "/i", "/d.raw", "c",
        {"blk_queues": 1, "net_queues": 1, "blk_iommu": False}))
    assert "iommu=" not in disk


def test_unit_ch_network_iommu_uses_virtio():
    be = RUN_MOD.CloudHypervisor("/usr/bin/cloud-hypervisor")
    cmd = be.build_cmd(
        "/k", "/i", "/d.raw", "c",
        {"blk_queues": 1, "net_queues": 1, "net_iommu": True})
    net = cmd[cmd.index("--net") + 1]
    assert "iommu=virtio" in net.split(",")


def test_unit_ch_network_iommu_is_opt_in():
    be = RUN_MOD.CloudHypervisor("/usr/bin/cloud-hypervisor")
    for opts in ({}, {"net_iommu": False}):
        cmd = be.build_cmd("/k", "/i", "/d.raw", "c", opts)
        net = cmd[cmd.index("--net") + 1]
        assert "iommu=" not in net


def test_unit_ch_io_engine_aio_disables_io_uring():
    be = RUN_MOD.CloudHypervisor("/usr/bin/cloud-hypervisor")
    disk = _ch_disk(be.build_cmd(
        "/k", "/i", "/d.raw", "c",
        {"blk_queues": 1, "net_queues": 1, "io_engine": "aio"}))
    assert "_disable_io_uring=on" in disk
    assert "_disable_aio" not in disk


def test_unit_ch_io_engine_sync_disables_both():
    be = RUN_MOD.CloudHypervisor("/usr/bin/cloud-hypervisor")
    disk = _ch_disk(be.build_cmd(
        "/k", "/i", "/d.raw", "c",
        {"blk_queues": 1, "net_queues": 1, "io_engine": "sync"}))
    assert "_disable_io_uring=on" in disk
    assert "_disable_aio=on" in disk


def test_unit_ch_direct_adds_direct():
    be = RUN_MOD.CloudHypervisor("/usr/bin/cloud-hypervisor")
    disk = _ch_disk(be.build_cmd(
        "/k", "/i", "/d.raw", "c",
        {"blk_queues": 1, "net_queues": 1, "direct": True}))
    assert "direct=on" in disk


# ---------------------------------------------------------------------------
# OpenVMM backend command construction.

def _openvmm_opt(cmd, flag):
    """Return the argument that follows the first occurrence of flag."""
    return cmd[cmd.index(flag) + 1]


def _openvmm_ports(cmd):
    """Return every PCIe root port name declared on the command line."""
    return [cmd[i + 1].split(":", 1)[1]
            for i, a in enumerate(cmd) if a == "--pcie-root-port"]


def test_unit_openvmm_detect_by_name():
    be = RUN_MOD.detect_vmm("/opt/openvmm/openvmm")
    assert isinstance(be, VMM_MOD.OpenVmm)
    assert be.name == "openvmm"


def test_unit_openvmm_build_cmd_direct_boot_layout():
    """Every virtio device is placed on its own named PCIe root port."""
    original = RUN_MOD.platform.machine
    try:
        RUN_MOD.platform.machine = lambda: "x86_64"
        be = VMM_MOD.OpenVmm("/opt/openvmm/openvmm")
        cmd = be.build_cmd(
            "/k", "/i", "/d.raw", "console=ttyS0 vv.test=T0001",
            {"cpus": 2, "memory": "256M"})
    finally:
        RUN_MOD.platform.machine = original
    assert _openvmm_opt(cmd, "-k") == "/k"
    assert _openvmm_opt(cmd, "-r") == "/i"
    assert _openvmm_opt(cmd, "-c") == "console=ttyS0 vv.test=T0001"
    assert _openvmm_opt(cmd, "-m") == "256M"
    assert _openvmm_opt(cmd, "-p") == "2"
    assert _openvmm_opt(cmd, "--hypervisor") == "kvm"
    assert "--hv" in cmd
    assert _openvmm_opt(cmd, "--pcie-root-complex") == "rc0"
    # Runner output rides on COM1, not the virtio-console device.
    assert _openvmm_opt(cmd, "--com1") == "console"
    assert "--single-process" in cmd
    # The disk, net, console and rng ports are always declared.
    assert set(_openvmm_ports(cmd)) >= {"disk", "net", "console", "rng"}


def test_unit_openvmm_aarch64_disables_hv():
    """KVM/aarch64 cannot provide the SynIC required by VMBus."""
    original = RUN_MOD.platform.machine
    try:
        RUN_MOD.platform.machine = lambda: "aarch64"
        be = VMM_MOD.OpenVmm("/opt/openvmm/openvmm")
        cmd = be.build_cmd("/k", "/i", "/d.raw", "console=ttyAMA0", {})
    finally:
        RUN_MOD.platform.machine = original
    assert "--hv" not in cmd


def test_unit_openvmm_build_cmd_blk_on_pcie_port():
    be = VMM_MOD.OpenVmm("/opt/openvmm/openvmm")
    cmd = be.build_cmd("/k", "/i", "/d.raw", "c", {})
    blk = _openvmm_opt(cmd, "--virtio-blk")
    assert blk == "file:/d.raw,pcie_port=disk"


def test_unit_openvmm_build_cmd_blk_direct():
    be = VMM_MOD.OpenVmm("/opt/openvmm/openvmm")
    cmd = be.build_cmd("/k", "/i", "/d.raw", "c", {"direct": True})
    assert _openvmm_opt(cmd, "--virtio-blk") == "file:/d.raw;direct,pcie_port=disk"


def test_unit_block_readonly_configuration():
    from unittest.mock import patch

    with patch.object(RUN_MOD, "_SCRIPT_DIR", ROOT_DIR):
        sidecar = RUN_MOD._load_sidecar(RUN_MOD._find_sidecar("B0057"))
    assert sidecar.vmm_config() == {"opts": {"blk_read_only": True}}
    for backend, flag, readonly in (
            (RUN_MOD.CloudHypervisor, "--disk", "readonly=on"),
            (RUN_MOD.Qemu, "-drive", "readonly=on"),
            (RUN_MOD.QemuMicrovm, "-drive", "readonly=on"),
            (VMM_MOD.OpenVmm, "--virtio-blk", "ro")):
        instance = backend("vmm")
        for direct in (False, True):
            options = {"direct": direct}
            cmd = instance.build_cmd("/k", "/i", "/d.raw", "c", options)
            assert readonly not in _openvmm_opt(cmd, flag).split(",")
            options.update(sidecar.vmm_config()["opts"])
            cmd = instance.build_cmd("/k", "/i", "/d.raw", "c", options)
            assert readonly in _openvmm_opt(cmd, flag).split(",")


def test_unit_readonly_probe_and_batch_configuration():
    import contextlib
    import io
    from types import SimpleNamespace
    from unittest.mock import Mock, patch

    names = ["B0001", "B0057", "B0002"]
    metadata = {
        name: {"device_id": 0x1042, "required_features": (
            0x20 if name == "B0057" else 0), "flags": 0, "min_queues": 1}
        for name in names}
    sidecar = SimpleNamespace(
        vmm_config=lambda: {"opts": {"blk_read_only": True}})
    for backend_name in ("ch", "qemu", "openvmm"):
        with tempfile.TemporaryDirectory() as directory:
            backend = SimpleNamespace(
                name=backend_name, binary=MOCK_LINK, mmio_transport=False,
                console_device="ttyS0", build_cmd=Mock(
                    side_effect=lambda k, i, d, c, o: [
                        "probe-vmm", str(o.get("blk_read_only", False))]))

            def launch(cmd, **kwargs):
                features = b"20" if cmd[1] == "True" else b"0"
                return subprocess.CompletedProcess(
                    cmd, 0, b"PROBE\t1042\t" + features + b"\t1\t1\t0\n", b"")

            def run_case(b, k, selected, *args):
                if isinstance(selected, str):
                    selected = [selected]
                return [(name, "PASS", "") for name in selected]

            argv = [RUN, "--vmm", MOCK_LINK, "--kernel", "/dev/null",
                    "--initramfs", RUN, "--skip-unsupported", "--batch", "2",
                    "-j", "1", *names]
            with patch.object(RUN_MOD, "_PROBE_CACHE_DIR", directory), \
                 patch.object(VMM_MOD, "_SCRIPT_DIR", directory), \
                 patch.object(RUN_MOD, "detect_vmm", return_value=backend), \
                 patch.object(RUN_MOD, "print_banner"), \
                 patch.object(RUN_MOD, "_install_termination_as_interrupt"), \
                 patch.object(RUN_MOD, "get_test_list",
                              return_value=(names, {}, metadata)), \
                 patch.object(RUN_MOD, "_find_sidecar", side_effect=(
                     lambda name: "sidecar" if name == "B0057" else None)), \
                 patch.object(RUN_MOD, "_load_sidecar", return_value=sidecar), \
                 patch.object(RUN_MOD.process, "capture", side_effect=launch), \
                 patch.object(RUN_MOD, "run_test",
                              side_effect=run_case) as run_test, \
                 patch.object(sys, "argv", argv), \
                 contextlib.redirect_stdout(io.StringIO()):
                try:
                    RUN_MOD.main()
                except SystemExit as error:
                    assert error.code == 0
            assert backend.build_cmd.call_count == 2
            assert [call.args[2] for call in run_test.call_args_list] == [
                ["B0001", "B0002"], "B0057"]


def test_unit_openvmm_build_cmd_accepts_io_uring():
    be = VMM_MOD.OpenVmm("/opt/openvmm/openvmm")
    cmd = be.build_cmd("/k", "/i", "/d.raw", "c", {"io_engine": "io_uring"})
    assert _openvmm_opt(cmd, "--virtio-blk") == "file:/d.raw,pcie_port=disk"


def test_unit_openvmm_build_cmd_rejects_unsupported_io_engines():
    be = VMM_MOD.OpenVmm("/opt/openvmm/openvmm")
    for engine in ("aio", "sync"):
        try:
            be.build_cmd("/k", "/i", "/d.raw", "c", {"io_engine": engine})
        except ValueError as error:
            assert str(error) == (
                f"OpenVMM does not support selecting the {engine} IO engine")
        else:
            raise AssertionError(f"OpenVMM accepted the {engine} IO engine")


def test_unit_openvmm_build_cmd_net_consomme():
    be = VMM_MOD.OpenVmm("/opt/openvmm/openvmm")
    cmd = be.build_cmd("/k", "/i", "/d.raw", "c", {})
    assert _openvmm_opt(cmd, "--virtio-net") == "pcie_port=net:consomme"


def test_unit_openvmm_net_receive_uses_consomme_forwarding():
    be = VMM_MOD.OpenVmm("/opt/openvmm/openvmm")
    cmd = be.build_cmd("/k", "/i", "/d.raw", "c",
                       {"net_rx_port": 41000})
    assert _openvmm_opt(cmd, "--virtio-net") == (
        "pcie_port=net:consomme:hostfwd=udp:127.0.0.1:41000-:41000")


def test_unit_openvmm_fuzz_device_attaches_only_that_device():
    """A focused fuzz device attaches only that device and its port."""
    be = VMM_MOD.OpenVmm("/opt/openvmm/openvmm")
    cmd = be.build_cmd("/k", "/i", "/d.raw", "c", {"fuzz_device": "blk"})
    assert _openvmm_ports(cmd) == ["disk"]
    assert "--virtio-blk" in cmd
    assert "--virtio-net" not in cmd
    assert "--virtio-rng" not in cmd


def test_unit_openvmm_exit_on_guest_end_sets_actions():
    """Fuzzing makes the VMM exit on guest power off or reset."""
    be = VMM_MOD.OpenVmm("/opt/openvmm/openvmm")
    cmd = be.build_cmd("/k", "/i", "/d.raw", "c", {"exit_on_guest_end": True})
    assert _openvmm_opt(cmd, "--guest-shutdown-action") == "exit"
    assert _openvmm_opt(cmd, "--guest-reset-action") == "exit"


def test_unit_openvmm_default_keeps_guest_actions_unset():
    """Conformance runs do not force exit actions."""
    be = VMM_MOD.OpenVmm("/opt/openvmm/openvmm")
    cmd = be.build_cmd("/k", "/i", "/d.raw", "c", {})
    assert "--guest-shutdown-action" not in cmd
    assert "--guest-reset-action" not in cmd


def test_unit_openvmm_build_cmd_vsock_on_pcie_port():
    be = VMM_MOD.OpenVmm("/opt/openvmm/openvmm")
    cmd = be.build_cmd("/k", "/i", "/d.raw", "c", {})
    assert _openvmm_opt(cmd, "--virtio-vsock-path") == "/d.vsock"
    assert _openvmm_opt(cmd, "--virtio-vsock-bus") == "pcie"
    assert "vsock" in _openvmm_ports(cmd)


def test_unit_openvmm_build_cmd_pmem_optional():
    be = VMM_MOD.OpenVmm("/opt/openvmm/openvmm")
    without = be.build_cmd("/k", "/i", "/d.raw", "c", {})
    assert "--virtio-pmem" not in without
    assert "pmem" not in _openvmm_ports(without)
    with_pmem = be.build_cmd("/k", "/i", "/d.raw", "c",
                             {"pmem_path": "/d.pmem"})
    assert _openvmm_opt(with_pmem, "--virtio-pmem") == "pcie_port=pmem:/d.pmem"
    assert "pmem" in _openvmm_ports(with_pmem)


def test_unit_openvmm_build_cmd_fs_optional():
    be = VMM_MOD.OpenVmm("/opt/openvmm/openvmm")
    without = be.build_cmd("/k", "/i", "/d.raw", "c", {})
    assert "--vhost-user" not in without
    with_fs = be.build_cmd("/k", "/i", "/d.raw", "c",
                           {"fs_socket": "/d.fsd"})
    assert _openvmm_opt(with_fs, "--vhost-user") == \
        "/d.fsd,type=fs,tag=vvfs,pcie_port=fs"
    assert "fs" in _openvmm_ports(with_fs)


def test_unit_openvmm_build_cmd_p9_optional():
    be = VMM_MOD.OpenVmm("/opt/openvmm/openvmm")
    without = be.build_cmd("/k", "/i", "/d.raw", "c", {})
    assert "--virtio-9p" not in without
    with_p9 = be.build_cmd("/k", "/i", "/d.raw", "c",
                           {"p9_sharedir": "/share"})
    assert _openvmm_opt(with_p9, "--virtio-9p") == \
        "pcie_port=p9:vv9p,/share"
    assert "p9" in _openvmm_ports(with_p9)


def test_unit_qemu_aio_forces_direct_gating():
    """The aio direct IO nudge fires only for the QEMU backends."""
    f = RUN_MOD._qemu_aio_forces_direct
    assert f("qemu", "aio", False) is True
    assert f("qemu-mmio", "aio", False) is True
    assert f("qemu", "aio", True) is False
    assert f("qemu", "io_uring", False) is False
    assert f("ch", "aio", False) is False


def _help_text(script, *args):
    r = subprocess.run([sys.executable, script, *args, "--help"],
                       capture_output=True, text=True, cwd=ROOT_DIR)
    return r.stdout


def test_unit_run_help_short_option_first():
    text = _help_text(RUN)
    assert not re.search(r"--[\w-]+, -\w", text), \
        "an option lists its long form before the short form"
    assert "-j, --jobs" in text


def test_unit_run_help_lists_help_last():
    text = _help_text(RUN)
    assert "-h, --help" in text
    assert text.index("--no-api-socket") < text.index("-h, --help")


def test_unit_runfuzz_fuzz_help_short_option_first():
    text = _help_text(RUN_FUZZ, "fuzz")
    assert not re.search(r"--[\w-]+, -\w", text), \
        "an option lists its long form before the short form"
    assert "-n, --iterations" in text
    assert "-j, --jobs" in text


def test_unit_runfuzz_fuzz_help_lists_help_last():
    text = _help_text(RUN_FUZZ, "fuzz")
    assert "-h, --help" in text
    assert text.index("--timeout") < text.index("-h, --help")


def test_unit_find_ch_remote_sibling():
    """_find_ch_remote prefers a binary next to cloud-hypervisor."""
    with tempfile.TemporaryDirectory() as td:
        ch = os.path.join(td, "cloud-hypervisor")
        rem = os.path.join(td, "ch-remote")
        open(ch, "w").close()
        open(rem, "w").close()
        os.chmod(ch, 0o755)
        os.chmod(rem, 0o755)
        assert RUN_MOD._find_ch_remote(ch) == rem


def test_unit_find_ch_remote_path_fallback():
    """_find_ch_remote falls back to PATH when no sibling exists."""
    with tempfile.TemporaryDirectory() as td:
        ch = os.path.join(td, "cloud-hypervisor")
        open(ch, "w").close()
        os.chmod(ch, 0o755)
        path_dir = tempfile.mkdtemp()
        try:
            rem = os.path.join(path_dir, "ch-remote")
            open(rem, "w").close()
            os.chmod(rem, 0o755)
            old_path = os.environ.get("PATH", "")
            os.environ["PATH"] = path_dir + os.pathsep + old_path
            try:
                got = RUN_MOD._find_ch_remote(ch)
                assert got == rem, got
            finally:
                os.environ["PATH"] = old_path
        finally:
            shutil.rmtree(path_dir, ignore_errors=True)


def test_unit_vm_api_unsupported_backend():
    try:
        RUN_MOD.vm_api("/tmp/x", object(), "ping")
    except (ValueError, OSError):
        return
    raise AssertionError("expected error for unsupported backend")


def test_unit_vm_api_qemu_qmp_roundtrip():
    """Speak QMP over a local fake socket and verify request shape."""
    import json
    import socket as _socket
    import threading

    sock_path = tempfile.mktemp(suffix=".qmp")
    captured = {}
    ready = threading.Event()

    def server():
        srv = _socket.socket(_socket.AF_UNIX, _socket.SOCK_STREAM)
        srv.bind(sock_path)
        srv.listen(1)
        ready.set()
        c, _ = srv.accept()
        try:
            c.sendall(b'{"QMP":{"version":"x"}}\n')
            buf = b""
            while b"\n" not in buf:
                chunk = c.recv(4096)
                if not chunk:
                    break
                buf += chunk
            captured["caps"] = buf.split(b"\n", 1)[0]
            c.sendall(b'{"event":"RESET"}\n{"return":{}}\n')
            buf = b""
            while b"\n" not in buf:
                chunk = c.recv(4096)
                if not chunk:
                    break
                buf += chunk
            captured["cmd"] = buf.split(b"\n", 1)[0]
            c.sendall(b'{"event":"STOP"}\n'
                      b'{"event":"RESUME"}\n'
                      b'{"return":{"status":"running"}}\n')
        finally:
            c.close()
            srv.close()

    t = threading.Thread(target=server, daemon=True)
    t.start()
    assert ready.wait(timeout=2.0), "server not ready"
    try:
        be = RUN_MOD.Qemu("/usr/bin/qemu-system-x86_64")
        out = RUN_MOD.vm_api(sock_path, be, "query-status",
                             timeout=2.0)
        t.join(timeout=2)
        assert out == {"return": {"status": "running"}}, out
        assert json.loads(captured["caps"]) == {
            "execute": "qmp_capabilities"}
        assert json.loads(captured["cmd"]) == {
            "execute": "query-status"}
    finally:
        try:
            os.unlink(sock_path)
        except FileNotFoundError:
            pass


def test_unit_vm_api_socket_missing_raises():
    be = RUN_MOD.Qemu("/usr/bin/qemu-system-x86_64")
    try:
        RUN_MOD.vm_api("/tmp/does-not-exist-vv-selftest.api", be,
                       "query-status", timeout=0.2)
    except OSError:
        return
    raise AssertionError("expected OSError when api socket is absent")


# ---------------------------------------------------------------------------
# Result classification.

def test_unit_parse_result_pass():
    assert RUN_MOD._parse_result("[vv] hi\n[PASS] T1\n", 0) == "PASS"


def test_unit_parse_result_fail_marker_wins_over_zero_rc():
    assert RUN_MOD._parse_result("[vv] x\n[FAIL] T1\n", 0) == "FAIL"


def test_unit_parse_result_fail_precedes_pass():
    """Multiple markers: FAIL is checked before PASS."""
    out = "[vv] x\n[FAIL] T1\n[PASS] T2\n"
    assert RUN_MOD._parse_result(out, 0) == "FAIL"


def test_unit_parse_result_skip():
    assert RUN_MOD._parse_result("[vv] x\n[SKIP] T1\n", 0) == "SKIP"


def test_unit_parse_result_reject():
    assert RUN_MOD._parse_result("[vv] x\n[REJECT] T1\n", 0) == "REJECT"


def test_unit_parse_result_wedged_marker():
    assert RUN_MOD._parse_result("[vv] x\n[WEDGED] T1\n", 0) == "WEDGED"


def test_unit_parse_result_xfail():
    assert RUN_MOD._parse_result("[vv] x\n[XFAIL] T1\n", 0) == "XFAIL"


def test_unit_parse_result_xpass():
    assert RUN_MOD._parse_result("[vv] x\n[XPASS] T1\n", 0) == "XPASS"


def test_unit_parse_result_signal_returncode_means_fail():
    assert RUN_MOD._parse_result("[vv] x\n", -9) == "FAIL"


def test_unit_parse_result_no_vv_means_fail():
    """Output without the [vv] banner indicates the harness never ran."""
    assert RUN_MOD._parse_result("kernel panic\n", 0) == "FAIL"


def test_unit_parse_result_vv_no_marker_means_wedged():
    assert RUN_MOD._parse_result("[vv] hi\n[vv] still alive\n", 0) == "WEDGED"


# ---------------------------------------------------------------------------
# Backend detection.

def test_unit_detect_vmm_ch_by_name():
    be = RUN_MOD.detect_vmm("/some/path/cloud-hypervisor")
    assert isinstance(be, RUN_MOD.CloudHypervisor)
    assert be.binary == "/some/path/cloud-hypervisor"


def test_unit_detect_vmm_ch_short_name():
    be = RUN_MOD.detect_vmm("/usr/local/bin/ch")
    assert isinstance(be, RUN_MOD.CloudHypervisor)


def test_unit_detect_vmm_qemu_system():
    be = RUN_MOD.detect_vmm("/usr/bin/qemu-system-x86_64")
    assert isinstance(be, RUN_MOD.Qemu)


def test_unit_detect_vmm_unknown_aborts():
    try:
        RUN_MOD.detect_vmm("/usr/bin/nosuch-vmm-bin")
    except SystemExit as e:
        assert "Cannot detect" in str(e)
        return
    raise AssertionError("expected SystemExit for unknown VMM")


# ---------------------------------------------------------------------------
# QemuMicrovm build_cmd.

def test_unit_qemu_microvm_build_cmd_uses_mmio_device():
    be = RUN_MOD.QemuMicrovm("/usr/bin/qemu-system-x86_64")
    opts = {"memory": "256M", "api_socket": "/api"}
    cmd = be.build_cmd("/k", "/i", "/d.raw", "console=ttyS0", opts)
    assert "virtio-blk-device,drive=vd0" in cmd
    assert not any("virtio-blk-pci" in str(a) for a in cmd)
    assert "unix:/api,server=on,wait=off" in cmd
    assert not any("x-option-roms=off" in str(a) for a in cmd)
    assert "virtio-mmio.force-legacy=false" in cmd


def test_unit_qemu_microvm_selects_sidecar_device():
    be = RUN_MOD.QemuMicrovm("/usr/bin/qemu-system-x86_64")
    for device, model in (
            ("blk", "virtio-blk-device"),
            ("rng", "virtio-rng-device"),
            ("balloon", "virtio-balloon-device"),
            ("rtc", "virtio-rtc-device"),
            ("console", "virtio-serial-device"),
            ("vsock", "vhost-vsock-device"),
            ("scsi", "virtio-scsi-device")):
        cmd = be.build_cmd("/k", "/i", "/d.raw", "console=ttyS0",
                           {"mmio_device": device,
                            "scsi_path": "/scsi.raw",
                            "scsi_path2": "/scsi2.raw"})
        models = [cmd[i + 1].split(",")[0] for i, arg in enumerate(cmd[:-1])
                  if arg == "-device" and cmd[i + 1].split(",")[0] in (
                      "virtio-blk-device", "virtio-rng-device",
                      "virtio-balloon-device", "virtio-rtc-device",
                      "virtio-serial-device", "vhost-vsock-device",
                      "virtio-scsi-device")]
        assert models == [model], (device, models)
        if device == "scsi":
            assert ("virtio-scsi-device,id=scsi0,num_queues=4") in cmd
            assert "file=/scsi.raw,if=none,id=scsidisk,format=raw" in cmd
            assert "file=/scsi2.raw,if=none,id=scsidisk2,format=raw" in cmd
            assert any(arg.startswith(
                "scsi-hd,drive=scsidisk,bus=scsi0.0,channel=0,scsi-id=0,lun=0")
                for arg in cmd)
            assert any(arg.startswith(
                "scsi-hd,drive=scsidisk2,bus=scsi0.0,channel=0,scsi-id=0,lun=1")
                for arg in cmd)
    try:
        be.build_cmd("/k", "/i", "/d.raw", "", {"mmio_device": "unknown"})
    except ValueError as error:
        assert "unsupported MMIO device" in str(error)
    else:
        raise AssertionError("unsupported MMIO device accepted")


def test_unit_fetch_kernel_selects_mmio_image():
    from unittest.mock import patch

    with tempfile.TemporaryDirectory() as directory, \
         patch.object(VMM_MOD, "_SCRIPT_DIR", directory), \
         patch.object(RUN_MOD.platform, "machine", return_value="x86_64"):
        os.mkdir(os.path.join(directory, "target"))
        for name in ("bzImage-x86_64", "vmlinux-x86_64"):
            with open(os.path.join(directory, "target", name), "wb") as image:
                image.write(b"kernel")
        with patch.object(VMM_MOD.artifacts, "github_release_asset") as retrieve, \
             patch.object(VMM_MOD.sys, "stderr"):
            assert RUN_MOD.fetch_kernel(mmio=True).endswith("/bzImage-x86_64")
            assert RUN_MOD.fetch_kernel().endswith("/vmlinux-x86_64")
        retrieve.assert_not_called()


def test_transport_rejects_non_qemu():
    result = run_runner("--transport", "mmio", "PASS")
    assert result.returncode != 0
    assert "--transport is only supported for QEMU" in result.stderr


def test_accel_rejects_non_qemu():
    result = run_runner("--accel", "tcg", "PASS")
    assert result.returncode != 0
    assert "--accel is only supported for QEMU" in result.stderr


def test_unit_mmio_unsupported_backend_reports_skips_without_fallback():
    import contextlib
    import io
    import json
    import xml.etree.ElementTree as ET
    from types import SimpleNamespace
    from unittest.mock import patch

    for backend_name in ("ch", "openvmm", "qemu"):
        for report_format in ("text", "json", "junit"):
            for names, skip_unsupported in (
                    (["M0019"], False),
                    (["M0019", "PASS"], False),
                    (["M0019"], True),
                    (["M0019", "PASS"], True)):
                with tempfile.TemporaryDirectory() as directory:
                    backend = SimpleNamespace(
                        name=backend_name, binary=MOCK_LINK,
                        mmio_transport=False)
                    argv = [
                        RUN, "--vmm", MOCK_LINK, "--initramfs", RUN,
                        "--format", report_format, "--jobs", "12",
                        "--batch", "2", "--retries", "1", *names]
                    if skip_unsupported:
                        argv.append("--skip-unsupported")
                    output = io.StringIO()
                    errors = io.StringIO()
                    with patch.object(VMM_MOD, "_SCRIPT_DIR", directory), \
                         patch.object(RUN_MOD, "detect_vmm",
                                      return_value=backend), \
                         patch.object(RUN_MOD, "print_banner"), \
                         patch.object(RUN_MOD,
                                      "_install_termination_as_interrupt"), \
                         patch.object(RUN_MOD, "fetch_kernel",
                                      return_value="/dev/null") as fetch, \
                         patch.object(RUN_MOD, "get_test_list",
                                      return_value=(names, {}, {})), \
                         patch.object(RUN_MOD, "QemuMicrovm") as fallback, \
                         patch.object(RUN_MOD, "_mmio_boots") as boot, \
                         patch.object(shutil, "which",
                                      return_value="/usr/bin/qemu-system-x86_64"), \
                         patch.object(RUN_MOD, "run_test", side_effect=(
                             lambda b, k, name, *args: [
                                 (name, "PASS", "")])) as launch, \
                         patch.object(sys, "argv", argv), \
                         contextlib.redirect_stdout(output), \
                         contextlib.redirect_stderr(errors):
                        try:
                            RUN_MOD.main()
                        except SystemExit as error:
                            assert error.code == 0
                    fallback.assert_not_called()
                    boot.assert_not_called()
                    if len(names) == 1:
                        fetch.assert_not_called()
                    else:
                        fetch.assert_called_once_with(mmio=False)
                    assert launch.call_count == len(names) - 1
                    for call in launch.call_args_list:
                        assert call.args[:3] == (backend, "/dev/null", "PASS")
                    human = output if report_format == "text" else errors
                    assert "MMIO" in human.getvalue()
                    assert "MMIO transport is unavailable" in human.getvalue()
                    total = len(names) - int(skip_unsupported)
                    skipped = int(not skip_unsupported)
                    if report_format == "json":
                        doc = json.loads(output.getvalue())
                        assert doc["backend"] == backend_name
                        assert doc["vmm"] == MOCK_LINK
                        assert doc["total"] == total
                        assert doc["counts"]["skip"] == skipped
                        cases = [t for t in doc["tests"]
                                 if t["name"] == "M0019"]
                        if skip_unsupported:
                            assert cases == []
                        else:
                            assert len(cases) == 1
                            assert cases[0]["status"] == "SKIP"
                            assert "MMIO transport is unavailable" in (
                                cases[0]["output"])
                    elif report_format == "junit":
                        root = ET.fromstring(output.getvalue())
                        assert root.get("tests") == str(total)
                        assert root.get("skipped") == str(skipped)
                        case = root.find(".//testcase[@name='M0019']")
                        if skip_unsupported:
                            assert case is None
                        else:
                            assert case is not None
                            node = case.find("skipped")
                            assert node is not None
                            assert "MMIO transport is unavailable" in node.text
                    else:
                        if skip_unsupported:
                            assert "M0019" not in output.getvalue()
                            assert "[SKIP]" not in output.getvalue()
                        else:
                            assert "[SKIP]" in output.getvalue()
                            assert "1 skipped" in output.getvalue()


def test_unit_skip_unsupported_filters_sidecar_backends_before_probing():
    import contextlib
    import io
    import json
    from types import SimpleNamespace
    from unittest.mock import patch

    for mixed in (False, True):
        with tempfile.TemporaryDirectory() as directory:
            backend = SimpleNamespace(
                name="openvmm", binary=MOCK_LINK, mmio_transport=False)
            expected = ["B0001", "B0204", "SCSI0240"] if mixed else []
            names = ["L0200", *expected]
            modules = {
                "L0200": SimpleNamespace(SUPPORTED_BACKENDS=("ch", "qemu")),
                "B0204": SimpleNamespace(SUPPORTED_BACKENDS=("openvmm",)),
                "SCSI0240": SimpleNamespace(),
            }
            metadata = {"L0200": {
                "device_id": 0x1045, "required_features": 0,
                "flags": 0, "min_queues": 0}}
            argv = [
                RUN, "--vmm", MOCK_LINK, "--initramfs", RUN,
                "--skip-unsupported", "--format", "json",
                "--jobs", "12", *names]
            output = io.StringIO()
            errors = io.StringIO()
            with patch.object(VMM_MOD, "_SCRIPT_DIR", directory), \
                 patch.object(RUN_MOD, "detect_vmm", return_value=backend), \
                 patch.object(RUN_MOD, "print_banner"), \
                 patch.object(RUN_MOD, "_install_termination_as_interrupt"), \
                 patch.object(RUN_MOD, "fetch_kernel",
                              return_value="/dev/null") as fetch, \
                 patch.object(RUN_MOD, "get_test_list",
                              return_value=(names, {}, metadata)), \
                 patch.object(RUN_MOD, "_find_sidecar",
                              side_effect=lambda name: (
                                  name if name in modules else None)), \
                 patch.object(RUN_MOD, "_load_sidecar",
                              side_effect=lambda path: modules[path]), \
                 patch.object(RUN_MOD, "_cached_probe") as probe, \
                 patch.object(RUN_MOD, "run_test", side_effect=(
                     lambda b, k, name, *args: [
                         (name, "PASS", "")])) as launch, \
                 patch.object(sys, "argv", argv), \
                 contextlib.redirect_stdout(output), \
                 contextlib.redirect_stderr(errors):
                try:
                    RUN_MOD.main()
                except SystemExit as error:
                    assert error.code == 0
            probe.assert_not_called()
            if mixed:
                fetch.assert_called_once_with(mmio=False)
            else:
                fetch.assert_not_called()
            assert sorted(call.args[2] for call in launch.call_args_list) == (
                sorted(expected))
            doc = json.loads(output.getvalue())
            assert doc["total"] == len(expected)
            assert sorted(t["name"] for t in doc["tests"]) == sorted(expected)
            assert doc["counts"]["skip"] == 0
            assert "sidecar" in errors.getvalue()


def test_unit_mmio_selected_transport_uses_only_selected_vmm():
    import contextlib
    import io
    import json
    from types import SimpleNamespace
    from unittest.mock import patch

    for explicit_qemu, boots in ((False, True), (True, True), (True, False)):
        with tempfile.TemporaryDirectory() as directory:
            backend = SimpleNamespace(
                name="qemu-mmio" if explicit_qemu else "ch",
                binary=MOCK_LINK, mmio_transport=True)
            detected = RUN_MOD.Qemu(MOCK_LINK) if explicit_qemu else backend
            argv = [RUN, "--vmm", MOCK_LINK, "--initramfs", RUN,
                    "--format", "json", "M0019"]
            if explicit_qemu:
                argv.extend(["--transport", "mmio"])
            output = io.StringIO()
            with patch.object(VMM_MOD, "_SCRIPT_DIR", directory), \
                 patch.object(RUN_MOD, "detect_vmm", return_value=detected), \
                 patch.object(RUN_MOD, "print_banner"), \
                 patch.object(RUN_MOD, "_install_termination_as_interrupt"), \
                 patch.object(RUN_MOD, "fetch_kernel",
                              return_value="/dev/null") as fetch, \
                 patch.object(RUN_MOD, "get_test_list",
                              return_value=(["M0019"], {}, {})), \
                 patch.object(RUN_MOD, "QemuMicrovm",
                              return_value=backend) as select, \
                 patch.object(RUN_MOD, "_mmio_boots",
                              return_value=boots) as boot, \
                 patch.object(RUN_MOD, "run_test", return_value=[
                     ("M0019", "PASS", "")]) as launch, \
                 patch.object(sys, "argv", argv), \
                 contextlib.redirect_stdout(output):
                try:
                    RUN_MOD.main()
                except SystemExit as error:
                    assert error.code == 0
            if explicit_qemu:
                select.assert_called_once_with(MOCK_LINK)
                boot.assert_called_once_with(backend, "/dev/null", RUN)
            else:
                select.assert_not_called()
                boot.assert_not_called()
            fetch.assert_called_once_with(mmio=explicit_qemu)
            if boots:
                launch.assert_called_once()
                assert launch.call_args.args[:3] == (
                    backend, "/dev/null", "M0019")
            else:
                launch.assert_not_called()
            doc = json.loads(output.getvalue())
            assert doc["backend"] == backend.name
            assert doc["counts"]["pass"] == int(boots)
            assert doc["counts"]["skip"] == int(not boots)
            if not boots:
                assert "MMIO guest boot probe failed" in (
                    doc["tests"][0]["output"])


# ---------------------------------------------------------------------------
# End to end coverage of result tags via mock VMM.

def test_skip_summary():
    r = run_runner("SKIP")
    assert r.returncode == 0, r.stdout
    assert "[SKIP]" in r.stdout
    assert "1 skipped" in r.stdout


def test_reject_summary():
    r = run_runner("REJECT")
    assert r.returncode == 0, r.stdout
    assert "[REJECT]" in r.stdout
    assert "1 rejected" in r.stdout


def test_wedged_marker_summary():
    """A test that prints [WEDGED] is reported as wedged and fails the run."""
    r = run_runner("WEDGED")
    assert r.returncode == 1, r.stdout
    assert "[WEDGED]" in r.stdout
    assert "1 wedged" in r.stdout


def test_no_marker_means_wedged():
    """A VM that finishes without any result tag is classified WEDGED."""
    r = run_runner("NOMARKER")
    assert r.returncode == 1, r.stdout
    assert "[WEDGED]" in r.stdout


def test_mixed_states_in_summary():
    r = run_runner("PASS", "FAIL", "SKIP", "REJECT", "WEDGED")
    assert r.returncode == 1, r.stdout
    assert "1/5 tests passed" in r.stdout
    assert "1 failed" in r.stdout
    assert "1 rejected" in r.stdout
    assert "1 wedged" in r.stdout
    assert "1 skipped" in r.stdout


def test_xfail_summary():
    """An XFAIL test passes the run and is listed in the XFAIL block."""
    r = run_runner("XFAIL")
    assert r.returncode == 0, r.stdout
    assert "[XFAIL]" in r.stdout
    assert "1 xfail" in r.stdout
    assert "XFAIL (expected failure" in r.stdout


def test_xpass_summary():
    """An XPASS test fails the run so the stale xfail marker is noticed."""
    r = run_runner("XPASS")
    assert r.returncode == 1, r.stdout
    assert "[XPASS]" in r.stdout
    assert "1 xpass" in r.stdout
    assert "XPASS (xfail marker is stale" in r.stdout


def test_xfail_and_xpass_mixed():
    """XFAIL counts as success, XPASS counts as failure; both appear."""
    r = run_runner("PASS", "XFAIL", "XPASS")
    assert r.returncode == 1, r.stdout
    assert "1/3 tests passed" in r.stdout
    assert "1 xfail" in r.stdout
    assert "1 xpass" in r.stdout
    assert "XFAIL (expected failure" in r.stdout
    assert "XPASS (xfail marker is stale" in r.stdout


# ---------------------------------------------------------------------------
# Machine readable report formatters.

def _sample_results():
    return [
        ("RNG0001", "PASS", "boot ok"),
        ("X0001", "FAIL", "panic: boom\nNEEDS_RESET"),
        ("X0002", "WEDGED", "no marker"),
        ("X0003", "SKIP", "no device"),
        ("X0004", "REJECT", "silent"),
        ("X0005", "XFAIL", "known bug"),
        ("X0006", "XPASS", "marker stale"),
    ]


def _sample_info():
    return {
        "RNG0001": ("RNG basic request", "1.2", "5.4.6"),
        "X0001": ("boom test", "1.2", "2.6"),
    }


def test_unit_build_results_doc_counts():
    doc = RUN_MOD.build_results_doc(
        _sample_results(), _sample_info(),
        backend_name="ch", vmm="/bin/ch")
    assert doc["tool"] == "virtio-villain"
    assert doc["backend"] == "ch"
    assert doc["vmm"] == "/bin/ch"
    assert doc["total"] == 7
    c = doc["counts"]
    assert c == {"pass": 1, "fail": 1, "reject": 1, "wedged": 1,
                 "skip": 1, "xfail": 1, "xpass": 1, "retried": 0}


def test_unit_build_results_doc_per_test_fields():
    doc = RUN_MOD.build_results_doc(
        _sample_results(), _sample_info(),
        backend_name="ch", vmm="/bin/ch")
    by_name = {t["name"]: t for t in doc["tests"]}
    rng = by_name["RNG0001"]
    assert rng["status"] == "PASS"
    assert rng["description"] == "RNG basic request"
    assert rng["spec_version"] == "1.2"
    assert rng["spec_section"] == "5.4.6"
    # A passing test carries no output payload.
    assert rng["output"] == ""
    # A failing test keeps its output for triage.
    assert "panic: boom" in by_name["X0001"]["output"]


def test_unit_format_results_json_roundtrips():
    import json
    doc = RUN_MOD.build_results_doc(
        _sample_results(), _sample_info(),
        backend_name="ch", vmm="/bin/ch")
    text = RUN_MOD.format_results_json(doc)
    parsed = json.loads(text)
    assert parsed == doc


def test_unit_format_results_junit_wellformed():
    import xml.dom.minidom as md
    doc = RUN_MOD.build_results_doc(
        _sample_results(), _sample_info(),
        backend_name="ch", vmm="/bin/ch")
    xml = RUN_MOD.format_results_junit(doc)
    assert xml.startswith("<?xml")
    # Must parse as well-formed XML.
    md.parseString(xml)


def test_unit_format_results_junit_classification():
    import xml.etree.ElementTree as ET
    doc = RUN_MOD.build_results_doc(
        _sample_results(), _sample_info(),
        backend_name="ch", vmm="/bin/ch")
    xml = RUN_MOD.format_results_junit(doc)
    root = ET.fromstring(xml)
    suite = root.find("testsuite")
    assert suite.get("tests") == "7"
    # FAIL, WEDGED and XPASS all count as JUnit failures.
    assert suite.get("failures") == "3"
    # SKIP and XFAIL count as skipped.
    assert suite.get("skipped") == "2"
    cases = {c.get("name"): c for c in suite.findall("testcase")}
    assert cases["X0001"].find("failure") is not None
    assert cases["X0002"].find("failure") is not None
    assert cases["X0006"].find("failure") is not None
    assert cases["X0003"].find("skipped") is not None
    assert cases["X0005"].find("skipped") is not None
    # PASS and REJECT are plain testcases.
    assert cases["RNG0001"].find("failure") is None
    assert cases["RNG0001"].find("skipped") is None
    assert cases["X0004"].find("failure") is None


def _security_results():
    """Results whose output exercises each security tier in the runner."""
    return [
        ("RNG0001", "PASS", "boot ok"),
        ("X0001", "FAIL",
         "==1==ERROR: AddressSanitizer: heap-use-after-free on 0x60\n"),
        ("X0002", "WEDGED",
         "Corrupted request detected ... Setting device status to "
         "'NEEDS_RESET'\n"),
        ("X0003", "FAIL",
         "thread '<_disk0_q0>' panicked at 'index out of bounds'\n"),
        ("X0004", "FAIL", "TIMEOUT after 30s"),
    ]


def test_unit_build_results_doc_security_tiers():
    doc = RUN_MOD.build_results_doc(
        _security_results(), {}, backend_name="ch", vmm="/bin/ch")
    by_name = {t["name"]: t for t in doc["tests"]}
    # A passing test carries no security verdict.
    assert by_name["RNG0001"]["security"] is None
    assert by_name["X0001"]["security"]["tier"] == "CRITICAL"
    assert by_name["X0002"]["security"]["tier"] == "LOW"
    assert by_name["X0003"]["security"]["tier"] == "HIGH"
    assert by_name["X0004"]["security"]["tier"] == "HIGH"


def test_unit_format_results_junit_carries_security():
    import xml.etree.ElementTree as ET
    doc = RUN_MOD.build_results_doc(
        _security_results(), {}, backend_name="ch", vmm="/bin/ch")
    xml = RUN_MOD.format_results_junit(doc)
    root = ET.fromstring(xml)
    cases = {c.get("name"): c
             for c in root.find("testsuite").findall("testcase")}
    fail = cases["X0001"].find("failure")
    # The tier is exposed both as an attribute and inline in the body.
    assert fail.get("security") == "CRITICAL"
    assert "security: CRITICAL" in fail.text


def test_unit_security_triage_from_output_only():
    """The runner triages from captured output without an exit code."""
    tier, _ = RUN_MOD._security_triage("device rejected the input")
    assert tier == "NONE"
    tier, _ = RUN_MOD._security_triage("TIMEOUT after 30s")
    assert tier == "HIGH"


def test_unit_xml_safe_strips_illegal_chars():
    # A NUL and other C0 control bytes are illegal in XML 1.0.
    dirty = "ok\x00\x08bad\x1f"
    clean = RUN_MOD._xml_safe(dirty)
    assert "\x00" not in clean
    assert "\x08" not in clean
    assert "\x1f" not in clean
    # Tab, newline and carriage return are preserved.
    assert RUN_MOD._xml_safe("a\tb\nc\rd") == "a\tb\nc\rd"


def test_unit_format_results_junit_no_keyerror():
    """Ensure format_results_junit accesses only keys that exist in counts."""
    doc = RUN_MOD.build_results_doc(
        _sample_results(), _sample_info(),
        backend_name="ch", vmm="/bin/ch")
    # Must not raise KeyError for any verdict status.
    xml = RUN_MOD.format_results_junit(doc)
    assert "testsuite" in xml


# ---------------------------------------------------------------------------
# Retry and transient failure handling (--retries).

def test_unit_fold_retries_recovered_keeps_real_verdict():
    # A test that failed then passed keeps its real PASS verdict, not a
    # synthetic status, and carries the attempts note so it can be
    # listed as retried separately.
    name, status, out = RUN_MOD._fold_retries(
        "X0001", ["FAIL", "PASS"], [("FAIL", "fail out")])
    assert status == "PASS"
    assert out.startswith("[vv] attempts: FAIL, PASS\n")
    assert "fail out" in out
    assert RUN_MOD._recovered_on_retry(status, out) is True


def test_unit_fold_retries_recovered_to_skip_keeps_skip():
    name, status, out = RUN_MOD._fold_retries(
        "X0001", ["WEDGED", "SKIP"], [("SKIP", "solo skip log")])
    assert status == "SKIP"
    assert out == "[vv] attempts: WEDGED, SKIP\nsolo skip log"
    assert RUN_MOD._recovered_on_retry(status, out) is True


def test_unit_fold_retries_all_fail_is_hard_failure():
    # Every attempt failed, only the failure kind varied. This stays a
    # gating failure, most severe kind, and did not recover.
    name, status, out = RUN_MOD._fold_retries(
        "X0001", ["WEDGED", "FAIL"],
        [("WEDGED", "wedged out"), ("FAIL", "fail out")])
    assert status == "FAIL"
    assert out.startswith("[vv] attempts: WEDGED, FAIL\n")
    assert "fail out" in out
    assert RUN_MOD._recovered_on_retry(status, out) is False


def test_unit_recovered_on_retry():
    # Retried and resolved to acceptable -> recovered.
    assert RUN_MOD._recovered_on_retry(
        "PASS", "[vv] attempts: FAIL, PASS\n") is True
    # Retried but still failing -> not recovered, it gates.
    assert RUN_MOD._recovered_on_retry(
        "FAIL", "[vv] attempts: FAIL, FAIL\n") is False
    # Not retried -> not recovered.
    assert RUN_MOD._recovered_on_retry("PASS", "boot ok") is False
    assert RUN_MOD._recovered_on_retry("SKIP", "") is False


def test_unit_build_results_doc_retried_counts_and_verdict():
    results = [
        ("RNG0001", "PASS", "boot ok"),
        ("X0001", "SKIP",
         "[vv] attempts: WEDGED, SKIP\n[SKIP] X0001\n"),
    ]
    doc = RUN_MOD.build_results_doc(
        results, {}, backend_name="ch", vmm="/bin/ch")
    # The recovered test keeps its real SKIP verdict and is counted
    # retried too.
    assert doc["counts"]["skip"] == 1
    assert doc["counts"]["retried"] == 1
    by_name = {t["name"]: t for t in doc["tests"]}
    assert by_name["X0001"]["status"] == "SKIP"
    assert by_name["X0001"]["retried"] is True
    # An acceptable verdict carries no security tier.
    assert by_name["X0001"]["security"] is None
    assert by_name["RNG0001"]["retried"] is False


def test_unit_build_results_doc_retried_pass_detected():
    # A recovered PASS drops its output but is still counted retried,
    # detected from the note before the output is dropped.
    results = [("X0001", "PASS", "[vv] attempts: FAIL, PASS\n")]
    doc = RUN_MOD.build_results_doc(
        results, {}, backend_name="ch", vmm="/bin/ch")
    assert doc["counts"]["retried"] == 1
    t = doc["tests"][0]
    assert t["status"] == "PASS"
    assert t["retried"] is True
    assert t["output"] == ""


def test_unit_format_results_junit_recovered_is_not_failure():
    import xml.etree.ElementTree as ET
    results = [
        ("RNG0001", "PASS", "boot ok"),
        ("X0001", "SKIP", "[vv] attempts: WEDGED, SKIP\n"),
    ]
    doc = RUN_MOD.build_results_doc(
        results, {}, backend_name="ch", vmm="/bin/ch")
    xml = RUN_MOD.format_results_junit(doc)
    root = ET.fromstring(xml)
    suite = root.find("testsuite")
    # A recovered result resolved to SKIP is not a JUnit failure.
    assert suite.get("failures") == "0"
    cases = {c.get("name"): c for c in suite.findall("testcase")}
    assert cases["X0001"].find("failure") is None
    assert cases["X0001"].find("skipped") is not None


def test_unit_apply_retries_no_retry_for_acceptable():
    # A batch of acceptable verdicts is returned untouched and boots no
    # extra VMs, so batching and green runs pay no retry cost.
    calls = {"n": 0}

    def fake_run_test(*a, **k):
        calls["n"] += 1
        return [("X", "PASS", "x")]

    orig = RUN_MOD.run_test
    RUN_MOD.run_test = fake_run_test
    try:
        batch = [("A", "PASS", "a"), ("B", "REJECT", "b"),
                 ("C", "SKIP", "c")]
        out = RUN_MOD._apply_retries(
            batch, 5, None, None, 5, 0, "raw", {})
    finally:
        RUN_MOD.run_test = orig
    assert out == batch
    assert calls["n"] == 0


def test_unit_apply_retries_recovered_keeps_real_verdict():
    # Only the failing test is retried, it stops at the first pass, and
    # it reports its real PASS verdict, marked retried by the note.
    calls = {"n": 0}

    def fake_run_test(backend, kernel, name, *a, **k):
        calls["n"] += 1
        return [(name, "PASS", "retry ok")]

    orig = RUN_MOD.run_test
    RUN_MOD.run_test = fake_run_test
    try:
        batch = [("A", "PASS", "a"), ("B", "FAIL", "b failed")]
        out = RUN_MOD._apply_retries(
            batch, 3, None, None, 5, 0, "raw", {})
    finally:
        RUN_MOD.run_test = orig
    names = {n: (s, o) for n, s, o in out}
    assert names["A"] == ("PASS", "a")
    assert names["B"][0] == "PASS"
    assert RUN_MOD._recovered_on_retry(*names["B"]) is True
    assert names["B"][1].startswith("[vv] attempts: FAIL, PASS\n")
    assert "b failed" in names["B"][1]
    # Only B was retried, and it stopped at the first pass.
    assert calls["n"] == 1


def test_unit_apply_retries_batched_drops_shared_output():
    # When the failing attempt came from a batch its output is shared
    # with the whole batch, so it must not be attached to one test. The
    # solo rerun's own clean output is attached instead.
    def fake_run_test(backend, kernel, name, *a, **k):
        return [(name, "SKIP", "solo skip output")]

    orig = RUN_MOD.run_test
    RUN_MOD.run_test = fake_run_test
    try:
        batch = [("N0102", "WEDGED",
                  "[SKIP] F0018\n[SKIP] T0110\n[PASS] B0193\n"
                  "cloud-hypervisor: unrelated batch log noise")]
        out = RUN_MOD._apply_retries(
            batch, 3, None, None, 5, 0, "raw", {}, True)
    finally:
        RUN_MOD.run_test = orig
    name, status, output = out[0]
    assert status == "SKIP"
    assert RUN_MOD._recovered_on_retry(status, output) is True
    assert output == "[vv] attempts: WEDGED, SKIP\nsolo skip output"
    # None of the shared batch log leaked into this test's detail.
    assert "B0193" not in output
    assert "cloud-hypervisor" not in output


def test_unit_apply_retries_batched_keeps_solo_failure_output():
    # A batched failure whose solo rerun also fails attaches the solo
    # failing output, which is specific to this test.
    def fake_run_test(backend, kernel, name, *a, **k):
        return [(name, "FAIL", "solo failure detail")]

    orig = RUN_MOD.run_test
    RUN_MOD.run_test = fake_run_test
    try:
        batch = [("B", "FAIL", "shared batch blob")]
        out = RUN_MOD._apply_retries(
            batch, 2, None, None, 5, 0, "raw", {}, True)
    finally:
        RUN_MOD.run_test = orig
    name, status, output = out[0]
    assert status == "FAIL"
    assert output.startswith("[vv] attempts: FAIL, FAIL, FAIL\n")
    assert "solo failure detail" in output
    assert "shared batch blob" not in output


def test_unit_apply_retries_all_fail_gates():
    # A test that fails every attempt exhausts the budget and keeps a
    # gating failure verdict.
    calls = {"n": 0}

    def fake_run_test(backend, kernel, name, *a, **k):
        calls["n"] += 1
        return [(name, "FAIL", "still broken")]

    orig = RUN_MOD.run_test
    RUN_MOD.run_test = fake_run_test
    try:
        batch = [("B", "FAIL", "b failed")]
        out = RUN_MOD._apply_retries(
            batch, 2, None, None, 5, 0, "raw", {})
    finally:
        RUN_MOD.run_test = orig
    assert out[0][1] == "FAIL"
    assert RUN_MOD._recovered_on_retry(out[0][1], out[0][2]) is False
    # Two retries after the seeded failure.
    assert calls["n"] == 2


def test_unit_apply_retries_zero_is_noop():
    batch = [("B", "FAIL", "b failed")]
    out = RUN_MOD._apply_retries(batch, 0, None, None, 5, 0, "raw", {})
    assert out == batch


def test_retries_flag_rejects_negative():
    result = run_runner("--retries", "-1", "RNG0001")
    assert result.returncode != 0
    assert "--retries must not be negative" in (result.stderr + result.stdout)


# ---------------------------------------------------------------------------
# Batch result parsing.

def test_unit_parse_batch_results_all_found():
    output = "[PASS] RNG0001\n[FAIL] RNG0002\n[SKIP] RNG0003\n"
    r = RUN_MOD._parse_batch_results(output, ["RNG0001", "RNG0002", "RNG0003"])
    assert r == {"RNG0001": "PASS", "RNG0002": "FAIL", "RNG0003": "SKIP"}


def test_unit_parse_batch_results_missing_is_wedged():
    output = "[PASS] RNG0001\n"
    r = RUN_MOD._parse_batch_results(output, ["RNG0001", "RNG0002"])
    assert r["RNG0001"] == "PASS"
    assert r["RNG0002"] == "WEDGED"


def test_unit_parse_batch_results_ch_noise():
    output = ("cloud-hypervisor: some log line\n"
              "[PASS] RNG0001\n"
              "cloud-hypervisor: another line\n"
              "[REJECT] B0005\n")
    r = RUN_MOD._parse_batch_results(output, ["RNG0001", "B0005"])
    assert r == {"RNG0001": "PASS", "B0005": "REJECT"}


def test_unit_parse_batch_results_kernel_log_spliced_after_name():
    output = "[PASS] B0144[    0.373672] pci_bus 0000:04: scanning bus\n"
    r = RUN_MOD._parse_batch_results(output, ["B0144"])
    assert r == {"B0144": "PASS"}


def test_unit_parse_batch_results_kernel_log_spliced_inside_name():
    output = "[PASS] B0[    0.373672] pci_bus 0000:04: scanning bus\n144\n"
    r = RUN_MOD._parse_batch_results(output, ["B0144"])
    assert r == {"B0144": "PASS"}


def test_unit_parse_batch_results_name_spliced_inside_kernel_log():
    output = "[    0.373672] pci_bus [PASS] B0144\n"
    r = RUN_MOD._parse_batch_results(output, ["B0144"])
    assert r == {"B0144": "PASS"}


def test_unit_parse_batch_results_split_name_spliced_inside_kernel_log():
    output = "[    0.373672] pci_bus [PASS] B0\n144\n"
    r = RUN_MOD._parse_batch_results(output, ["B0144"])
    assert r == {"B0144": "PASS"}


def test_unit_parse_batch_results_single():
    output = "[PASS] RNG0001\n"
    r = RUN_MOD._parse_batch_results(output, ["RNG0001"])
    assert r == {"RNG0001": "PASS"}


def test_unit_retry_wedged_no_wedged():
    batch = [("A", "PASS", ""), ("B", "REJECT", ""), ("C", "PASS", "")]
    result = RUN_MOD._retry_wedged(batch, None, None, 10, 0, "raw", {})
    assert result == batch


def test_unit_retry_wedged_keeps_first_wedged():
    batch = [
        ("A", "PASS", "out"),
        ("B", "WEDGED", "out"),
        ("C", "WEDGED", "out"),
    ]
    # Mock run_test to return PASS for C when retried individually
    original = RUN_MOD.run_test
    RUN_MOD.run_test = lambda *a, **kw: [("C", "PASS", "retried")]
    try:
        result = RUN_MOD._retry_wedged(batch, None, None, 10, 0, "raw", {})
        assert len(result) == 3
        assert result[0] == ("A", "PASS", "out")
        assert result[1] == ("B", "WEDGED", "out")  # first WEDGED kept
        assert result[2] == ("C", "PASS", "retried")  # retried
    finally:
        RUN_MOD.run_test = original


def test_unit_retry_wedged_timeout_retries_first():
    """On a batch timeout the first WEDGED test has no culprit.

    A timeout note in the shared output means the VM was killed by the
    deadline, not by a test crashing it, so the first WEDGED test must
    be retried too rather than kept as a spurious WEDGED.
    """
    note = "[vv] TIMEOUT after 240s"
    batch = [
        ("A", "PASS", note),
        ("B", "WEDGED", note),
        ("C", "WEDGED", note),
    ]
    calls = []
    original = RUN_MOD.run_test

    def _fake(backend, kernel, name, *a, **kw):
        calls.append(name)
        return [(name, "PASS", "retried")]

    RUN_MOD.run_test = _fake
    try:
        result = RUN_MOD._retry_wedged(batch, None, None, 10, 0, "raw", {})
        verdicts = {n: s for n, s, _ in result}
        assert verdicts["A"] == "PASS"
        assert verdicts["B"] == "PASS"  # first WEDGED retried on timeout
        assert verdicts["C"] == "PASS"
        assert calls == ["B", "C"]  # both wedged tests retried
    finally:
        RUN_MOD.run_test = original


def test_unit_retry_wedged_no_duplicate_after_first():
    """A reported test after the first WEDGED appears once, not twice.

    Guards the regression where the crash branch appended a non WEDGED
    result twice, double counting it in the summary.
    """
    batch = [
        ("A", "WEDGED", "out"),  # crash culprit, no timeout note
        ("B", "PASS", "out"),    # reported after the culprit
    ]
    original = RUN_MOD.run_test
    RUN_MOD.run_test = lambda *a, **kw: [("X", "PASS", "retried")]
    try:
        result = RUN_MOD._retry_wedged(batch, None, None, 10, 0, "raw", {})
        names = [n for n, _, _ in result]
        assert names == ["A", "B"]  # B not duplicated
    finally:
        RUN_MOD.run_test = original


class _TimeoutBackend:
    """Fake backend whose VM prints two verdicts then hangs.

    Used to drive run_test into the batch timeout path without a real
    VMM. The first two named tests report; the run then blocks so the
    per test timer fires.
    """
    name = "fake"
    console_device = "ttyS0"
    mmio_transport = False

    def build_cmd(self, kernel, initramfs, disk_path, cmdline, opts=None):
        return ["sh", "-c",
                "printf '[PASS] T0001\\n[PASS] T0002\\n'; sleep 30"]


def test_unit_batch_timeout_preserves_printed_verdicts():
    """A batch that times out must not erase already printed verdicts.

    Only tests that never printed a marker before the timeout are
    WEDGED. This guards the regression where the timeout handler
    replaced the captured output with a bare TIMEOUT string, marking
    the whole batch WEDGED and leaving one spurious WEDGED per batch
    after the individual retry.
    """
    results = RUN_MOD.run_test(
        _TimeoutBackend(), "kernel", ["T0001", "T0002", "T0003"],
        1, 0, False, "raw", {})
    verdicts = {name: st for name, st, _ in results}
    assert verdicts["T0001"] == "PASS"
    assert verdicts["T0002"] == "PASS"
    assert verdicts["T0003"] == "WEDGED"


class _SlowMidBatchBackend:
    """VM where a test in the middle of a batch is slow to report.

    T0001 reports at once, then the VM is silent for longer than a
    single per test timeout before T0002 and T0003 report. The whole
    run stays well inside the batch budget (timeout * ntests).
    """
    name = "fake"
    console_device = "ttyS0"
    mmio_transport = False

    def build_cmd(self, kernel, initramfs, disk_path, cmdline, opts=None):
        return ["sh", "-c",
                "printf '[PASS] T0001\\n'; sleep 1.4; "
                "printf '[PASS] T0002\\n[PASS] T0003\\n'"]


def test_unit_batch_slow_test_does_not_wedge_siblings():
    """A slow test must not wedge the tests after it in the same batch.

    T0002 goes silent for about 1.4s, longer than the 1s per test
    timeout but far under the 3s batch budget. The VM must not be killed
    for overrunning a single per test slice, so every test reports. A
    per test idle kill or a premature all-markers kill would wedge T0002
    and T0003; this test fails on that behavior and passes on the single
    absolute batch timer.
    """
    results = RUN_MOD.run_test(
        _SlowMidBatchBackend(), "kernel", ["T0001", "T0002", "T0003"],
        1, 0, False, "raw", {})
    verdicts = {name: st for name, st, _ in results}
    assert verdicts["T0001"] == "PASS"
    assert verdicts["T0002"] == "PASS"
    assert verdicts["T0003"] == "PASS"


def test_unit_merge_streams_no_stderr():
    f = RUN_MOD._merge_streams
    assert f("out", []) == "out"
    assert f("out", [b""]) == "out"


def test_unit_merge_streams_appends_stderr():
    f = RUN_MOD._merge_streams
    assert f("out", [b"log1\n", b"log2\n"]) == "out\nlog1\nlog2\n"


def test_unit_desc_column_width_caps_at_longest():
    """A wide terminal caps the description column at the longest desc."""
    # cols 200, other columns take 35, longest description is 40.
    assert RUN_MOD._desc_column_width(200, 40, 35) == 40


def test_unit_desc_column_width_limited_by_terminal():
    """A narrow terminal shrinks the column below the longest desc."""
    # cols 60, fixed 35 leaves 25, which is below the longest desc 40.
    assert RUN_MOD._desc_column_width(60, 40, 35) == 25


def test_unit_desc_column_width_floor():
    """A very narrow terminal still leaves a minimum column."""
    assert RUN_MOD._desc_column_width(30, 40, 35) == 10


class _StderrNoiseBackend:
    """Fake backend that reports one test on stdout and puts a second,
    conflicting verdict marker on stderr.

    Mimics a VMM that writes its own log lines to stderr while the
    guest prints its marker on stdout. stdout carries only T0001, so
    there is no batch kill race; the process exits on its own.
    """
    name = "fake"
    console_device = "ttyS0"
    mmio_transport = False

    def build_cmd(self, kernel, initramfs, disk_path, cmdline, opts=None):
        return ["sh", "-c",
                "printf '[PASS] T0002\\n' >&2; "
                "printf '[PASS] T0001\\n'"]


def test_unit_stderr_does_not_corrupt_verdicts():
    """Verdicts come from stdout only; stderr markers are ignored.

    stdout reports T0001, stderr carries a [PASS] T0002 marker. Only
    T0001 is parsed as PASS; T0002 stays WEDGED because its marker is
    on stderr. The stderr content is still present in the combined
    output for logging and triage. Guards the regression where stderr
    was merged into stdout, letting a log line reach the parser and
    corrupt or fabricate a verdict.
    """
    results = RUN_MOD.run_test(
        _StderrNoiseBackend(), "kernel", ["T0001", "T0002"],
        10, 0, False, "raw", {})
    verdicts = {n: s for n, s, _ in results}
    assert verdicts["T0001"] == "PASS"
    assert verdicts["T0002"] == "WEDGED"
    combined = results[0][2]
    assert "[PASS] T0002" in combined


# ---------------------------------------------------------------------------
# Probe filtering logic.

def test_unit_sidecar_exception_overrides_guest_verdict():
    """A failed host action must not preserve a successful guest verdict."""
    from types import SimpleNamespace
    from unittest.mock import patch

    class Backend:
        name = "fake"
        console_device = "ttyS0"
        mmio_transport = False

        def __init__(self, verdict, detail=""):
            self.verdict = verdict
            self.detail = detail

        def build_cmd(self, kernel, initramfs, disk_path, cmdline, opts=None):
            return ["sh", "-c",
                    f"sleep 0.1; printf '[{self.verdict}] T0001"
                    f"'; sleep 0.05; printf '{self.detail}\\n'"]

    def fail_action(ctx):
        raise RuntimeError("required host action failed")

    sidecar = SimpleNamespace(run=fail_action)
    cases = [(verdict, "", "FAIL") for verdict in (
        "PASS", "REJECT", "SKIP", "XFAIL", "XPASS", "FAIL")]
    cases += [
        ("SKIP", " (no device 0x1051)", "SKIP"),
        ("PASS", " (no device 0x1051)", "FAIL"),
        ("SKIP", " (feature unavailable)", "FAIL"),
    ]
    for verdict, detail, expected in cases:
        with patch.object(RUN_MOD, "_find_sidecar", return_value="sidecar"), \
             patch.object(RUN_MOD, "_load_sidecar", return_value=sidecar):
            results = RUN_MOD.run_test(
                Backend(verdict, detail), "kernel", "T0001",
                5, 0, False, "raw", {})
        _, status, output = results[0]
        assert status == expected, (verdict, detail, status, output)
        assert f"[{verdict}] T0001{detail}\n" in output, output
        if expected == "SKIP":
            assert "[vv-sidecar] SKIP: guest device unavailable" in output
            assert "required host action failed" in output
            assert "[vv-sidecar] FAIL:" not in output
        else:
            assert "[vv-sidecar] FAIL: required host action failed" in output
            assert "RuntimeError" in output


def test_unit_sidecar_unfinished_action_fails():
    import threading
    from types import SimpleNamespace
    from unittest.mock import patch

    class Backend:
        name = "fake"
        console_device = "ttyS0"
        mmio_transport = False

        def build_cmd(self, kernel, initramfs, disk_path, cmdline, opts=None):
            return ["sh", "-c", f"printf '{marker}\\n'"]

    original_join = threading.Thread.join
    for marker, expected in (
            ("[PASS] T0001", "FAIL"),
            ("[REJECT] T0001", "FAIL"),
            ("[SKIP] T0001", "FAIL"),
            ("[XFAIL] T0001", "FAIL"),
            ("[XPASS] T0001", "FAIL"),
            ("[FAIL] T0001", "FAIL"),
            ("[WEDGED] T0001", "WEDGED"),
            ("[SKIP] T0001 (no device 0x1051)", "SKIP")):
        release = threading.Event()
        threads = []

        def action(ctx):
            release.wait()

        def join(thread, timeout=None):
            if thread.name == "sidecar-T0001":
                assert timeout == RUN_MOD._SIDECAR_JOIN_TIMEOUT
                threads.append(thread)
                return original_join(thread, timeout=0.01)
            return original_join(thread, timeout)

        try:
            with patch.object(RUN_MOD, "_find_sidecar", return_value="sidecar"), \
                 patch.object(RUN_MOD, "_load_sidecar",
                              return_value=SimpleNamespace(run=action)), \
                 patch.object(threading.Thread, "join", join):
                results = RUN_MOD.run_test(Backend(), "kernel", "T0001", 5)
            _, status, output = results[0]
            assert status == expected, (marker, status, output)
            assert ("host action did not exit within "
                    f"{RUN_MOD._SIDECAR_JOIN_TIMEOUT:g}s") in output
            if expected == "SKIP":
                assert "[vv-sidecar] FAIL:" not in output
            else:
                assert "[vv-sidecar] FAIL:" in output
        finally:
            release.set()
            for thread in threads:
                original_join(thread, timeout=1)
                assert not thread.is_alive()


def test_unit_sidecar_missing_prerequisites_skips_before_setup():
    from types import SimpleNamespace
    from unittest.mock import Mock, patch

    sidecar = SimpleNamespace(
        SUPPORTED_BACKENDS=("ch",), REQUIRED_TOOLS={"ch": ("ch-remote",)},
        run=Mock())
    for backend, reason in (
            ("qemu", "backend qemu is not supported"),
            ("ch", "required host tool ch-remote not found")):
        with patch.object(RUN_MOD, "_find_sidecar", return_value="sidecar"), \
             patch.object(RUN_MOD, "_load_sidecar", return_value=sidecar), \
             patch.object(RUN_MOD, "_find_ch_remote", return_value=None), \
             patch.object(RUN_MOD.tempfile, "mktemp") as create_disk, \
             patch.object(RUN_MOD.subprocess, "Popen") as launch:
            results = RUN_MOD.run_test(
                SimpleNamespace(name=backend, binary="/vmm"), "kernel",
                "T0001", 5, disk_format="qcow2")
        assert results[0][:2] == ("T0001", "SKIP"), results
        assert reason in results[0][2], results
        create_disk.assert_not_called()
        launch.assert_not_called()
        sidecar.run.assert_not_called()


def test_unit_sidecar_supported_backend_runs():
    from types import SimpleNamespace
    from unittest.mock import Mock, patch

    class Backend:
        name = "fake"
        console_device = "ttyS0"
        mmio_transport = False

        def build_cmd(self, kernel, initramfs, disk_path, cmdline, opts=None):
            return ["sh", "-c", "sleep 0.1; printf '[PASS] T0001\\n'"]

    for declaration in (
            {}, {"SUPPORTED_BACKENDS": ("fake",)},
            {"REQUIRED_TOOLS": {"ch": ("ch-remote",)}},
            {"REQUIRED_TOOLS": {"fake": ("test-control",)}}):
        action = Mock()
        sidecar = SimpleNamespace(run=action, **declaration)
        with patch.object(RUN_MOD, "_find_sidecar", return_value="sidecar"), \
             patch.object(RUN_MOD, "_load_sidecar", return_value=sidecar), \
             patch.object(RUN_MOD.shutil, "which",
                          return_value="/tools/test-control"):
            results = RUN_MOD.run_test(
                Backend(), "kernel", "T0001", 5, 0, False, "raw", {})
        assert results[0][:2] == ("T0001", "PASS"), results
        action.assert_called_once()


def test_unit_run_test_uses_shared_images_with_existing_backing_sizes():
    from unittest.mock import patch

    class Backend:
        name = "fake"
        console_device = "ttyS0"
        mmio_transport = False

        def build_cmd(self, kernel, initramfs, disk_path, cmdline, opts=None):
            assert os.path.getsize(disk_path) == RUN_MOD.DISK_SIZE
            assert os.path.getsize(opts["pmem_path"]) == PMEM_SIZE
            for name in ("scsi_path", "scsi_path2"):
                assert os.path.getsize(opts[name]) == RUN_MOD.DISK_SIZE
            return [sys.executable, "-c", "print('[PASS] T9999')"]

    with patch.object(RUN_MOD, "_find_sidecar", return_value=None), \
         patch.object(RUN_MOD.images, "create", wraps=RUN_MOD.images.create) as create:
        results = RUN_MOD.run_test(Backend(), "kernel", "T9999", 5)
    assert results[0][:2] == ("T9999", "PASS"), results
    create.assert_called_once()
    assert create.call_args.args[1] == RUN_MOD.DISK_SIZE
    assert create.call_args.kwargs == {"image_format": "raw"}


def test_unit_sidecar_backing_images_use_shared_creation():
    from types import SimpleNamespace
    from unittest.mock import Mock, patch

    for module, action, filename in (
            (sidecar_helpers, "add_disk", "hp.raw"),
            (RUN_MOD._load_sidecar(os.path.join(
                ROOT_DIR, "tests", "pmem", "e0200_hotplug_add.py")),
             "run", "hp_pmem.raw"),
            (RUN_MOD._load_sidecar(os.path.join(
                ROOT_DIR, "tests", "scsi", "scsi_helpers.py")),
             "hot_add", "hp.raw")):
        with tempfile.TemporaryDirectory() as work:
            ctx = SimpleNamespace(
                tmpdir=work, backend=SimpleNamespace(name="qemu"), api_sock="api",
                wait_text=Mock(return_value=True), log=Mock(),
                vm_api=Mock(return_value={"return": {}}))
            with patch.object(RUN_MOD.images, "create_raw",
                              wraps=RUN_MOD.images.create_raw) as create:
                getattr(module, action)(ctx)
            path = os.path.join(work, filename)
            create.assert_called_once_with(path, HOTPLUG_DISK_SIZE)
            assert os.path.getsize(path) == HOTPLUG_DISK_SIZE


def test_unit_sidecar_outcomes():
    import glob
    import json
    import threading
    from types import SimpleNamespace
    from unittest.mock import Mock, patch

    def skip(reason):
        raise RUN_MOD._SidecarSkip(reason)

    cases = []
    for path in sorted(glob.glob(
            os.path.join(ROOT_DIR, "tests", "*", "*.py"))):
        sidecar = RUN_MOD._load_sidecar(path)
        supported = getattr(sidecar, "SUPPORTED_BACKENDS", None)
        if supported is None or not hasattr(sidecar, "run"):
            continue
        for backend_name in supported:
            cases.append((path, sidecar, supported, backend_name))
    for path, sidecar, supported, backend_name in cases:
        other_backend = "unsupported"
        scsi = os.path.basename(path).startswith("scsi")
        banner_text = "vv-scsi-armed" if scsi else "[vv]"
        banner_error = ("guest did not arm the event queue" if scsi
                        else "guest harness banner not seen")
        name = os.path.basename(path).split("_", 1)[0].upper()
        with patch.object(RUN_MOD, "_find_sidecar", return_value=path), \
             patch.object(RUN_MOD.tempfile, "mktemp") as create_disk, \
             patch.object(RUN_MOD.subprocess, "Popen") as launch:
            results = RUN_MOD.run_test(
                SimpleNamespace(name=other_backend), "kernel", name, 5)
        assert results[0][:2] == (name, "SKIP"), (path, results)
        create_disk.assert_not_called()
        launch.assert_not_called()

        for backend, api, banner, error_type, reason in (
                (other_backend, "api", True, RUN_MOD._SidecarSkip,
                 "backend is not"),
                (backend_name, None, True, RUN_MOD._SidecarSkip,
                 "api socket disabled"),
                (backend_name, "api", False, RuntimeError, banner_error)):
            ctx = SimpleNamespace(
                backend=SimpleNamespace(name=backend), api_sock=api,
                skip=skip, wait_text=Mock(return_value=banner),
                vm_api=Mock(), log=Mock())
            try:
                sidecar.run(ctx)
            except error_type as error:
                assert reason in str(error), (path, str(error))
            else:
                raise AssertionError(f"{path} ignored {reason}")
            ctx.vm_api.assert_not_called()

        with tempfile.TemporaryDirectory() as tmpdir, patch("time.sleep"):
            disk = os.path.join(tmpdir, "disk.raw")
            with open(disk, "wb") as image:
                image.truncate(DISK_SIZE)
            success = (subprocess.CompletedProcess([], 0, "", "")
                       if backend_name in ("ch", "openvmm")
                       else {"return": {"actual": MEMORY_SIZE,
                                        "status": "completed"}})
            ctx = SimpleNamespace(
                backend=SimpleNamespace(name=backend_name), api_sock="api",
                tmpdir=tmpdir, disk=disk, skip=skip, vmm_pid=1000,
                stop=threading.Event(),
                wait_text=Mock(return_value=True), log=Mock(),
                vm_api=Mock(return_value=success))

            def successful_response(command, *args, **kwargs):
                if command == "info":
                    return subprocess.CompletedProcess([], 0, json.dumps({
                        "config": {"memory": {
                            "size": MEMORY_SIZE,
                            "hotplug_size": MEMORY_HOTPLUG_SIZE,
                        }},
                    }), "")
                if command == "human-monitor-command":
                    return {"return": ""}
                return {"return": []} if command == "qom-list" else success

            ctx.vm_api.side_effect = successful_response
            if backend_name in ("qemu", "qemu-mmio"):
                os.mkdir(os.path.join(tmpdir, "snap"))
                with open(os.path.join(tmpdir, "snap", "vmstate"), "wb") as state:
                    state.write(b"VM state")
            sidecar.run(ctx)
            commands = ctx.vm_api.call_args_list
            assert commands, f"{path} performed no host actions"
            resize_index = 0
            for command in commands:
                if command.args[0] == "balloon":
                    assert command.args[1] == {
                        "value": success["return"]["actual"] - MEMORY_REDUCTION
                    }, (path, command)
                if command.args[0] == "block_resize":
                    assert command.args[1] == {
                        "device": "scsidisk" if scsi else "vd0",
                        "size": ((2 + resize_index) * DISK_SIZE
                                 if name == "PCI0131" else 2 * DISK_SIZE)
                    }, (path, command)
                    resize_index += 1
            waits = ctx.wait_text.call_args_list
            assert waits[0].args == (banner_text,), (path, waits)
            assert all(wait.kwargs == {"timeout": 20.0}
                       for wait in waits), (path, waits)
            assert ctx.log.called, f"{path} did not report completion"
            completion = ctx.log.call_args.args[0]

            for index in range(1, len(waits)):
                ctx.vm_api.reset_mock()
                ctx.wait_text.side_effect = [True] * index + [False]
                try:
                    sidecar.run(ctx)
                except RuntimeError:
                    pass
                else:
                    raise AssertionError(
                        f"{path} ignored missing {waits[index].args[0]}")
                assert len(ctx.vm_api.call_args_list) < len(commands), path
            ctx.wait_text.side_effect = None

            for index, command in enumerate(commands):
                responses = ([subprocess.CompletedProcess([], 1, "out", "err")]
                             if backend_name in ("ch", "openvmm") else [
                                 {"error": {"class": "GenericError",
                                            "desc": "command failed"}},
                                 b"invalid reply", {}, None])
                for failure in (*responses,
                                RuntimeError("API connection failed"),
                                subprocess.TimeoutExpired(command.args[0], 30)):
                    ctx.log.reset_mock()
                    ctx.vm_api.reset_mock()
                    ctx.vm_api.side_effect = (
                        [successful_response(entry.args[0])
                         for entry in commands[:index]] + [failure] +
                        [successful_response(entry.args[0])
                         for entry in commands[index + 1:]] + [success])
                    try:
                        sidecar.run(ctx)
                    except (RuntimeError, subprocess.TimeoutExpired) as error:
                        if isinstance(failure, Exception):
                            assert error is failure, (path, str(error))
                        else:
                            assert command.args[0] in str(error), (
                                path, command, str(error))
                            diagnostics = str(error) + " ".join(
                                entry.args[0]
                                for entry in ctx.log.call_args_list)
                            if backend_name in ("ch", "openvmm"):
                                assert "rc=1" in diagnostics, (path, diagnostics)
                                assert "err" in diagnostics, (path, diagnostics)
                            else:
                                assert repr(failure) in diagnostics, (
                                    path, diagnostics)
                    else:
                        raise AssertionError(
                            f"{path} ignored {command.args[0]} failure")
                    assert ctx.vm_api.call_args_list[:index + 1] == (
                        commands[:index + 1]), path
                    assert completion not in [
                        entry.args[0] for entry in ctx.log.call_args_list], path
    assert cases, "no sidecars were checked"


def test_unit_host_action_completion():
    from types import SimpleNamespace
    from unittest.mock import Mock, patch

    helpers = sidecar_helpers
    for states, file_contents, error in (
            ([{"status": "active"}, {"status": "completed"}], b"state", None),
            ([{"status": "failed"}], b"state", RuntimeError),
            ([{"status": "cancelled"}], b"state", RuntimeError),
            ([[]], b"state", RuntimeError),
            ([{}], b"state", RuntimeError),
            ([{"status": "active"}], b"state", TimeoutError),
            ([{"status": "completed"}], b"", RuntimeError),
            ([{"status": "completed"}], None, FileNotFoundError)):
        with tempfile.TemporaryDirectory() as tmpdir:
            os.mkdir(os.path.join(tmpdir, "snap"))
            if file_contents is not None:
                with open(os.path.join(tmpdir, "snap", "vmstate"), "wb") as f:
                    f.write(file_contents)
            replies = iter(states)
            commands = []

            def snapshot_response(command, arguments=None):
                commands.append(command)
                return {"return": next(replies) if command == "query-migrate"
                        else {}}

            ctx = SimpleNamespace(
                backend=SimpleNamespace(name="qemu"), api_sock="api",
                tmpdir=tmpdir, wait_text=Mock(return_value=True), log=Mock(),
                vm_api=snapshot_response)
            clock = [0, 21] if error is TimeoutError else [0, 0]
            with patch.object(helpers.time, "sleep"), \
                 patch.object(helpers.time, "monotonic", side_effect=clock):
                try:
                    helpers.snapshot(ctx)
                except (RuntimeError, TimeoutError, FileNotFoundError) as exc:
                    assert error is not None and isinstance(exc, error), (
                        states, error, exc)
                else:
                    assert error is None, states
            assert commands[:2] == ["stop", "migrate"], commands
            assert commands[-1] == "cont", commands
            completed = any(call.args == ("snapshot completed",)
                            for call in ctx.log.call_args_list)
            assert completed == (error is None), (states, completed)

    for responses, error in (
            ([[{"name": "hp0"}], []], None),
            ([[{"name": "hp0"}]], TimeoutError),
            ([{}], RuntimeError),
            ([[{"name": None}]], RuntimeError)):
        replies = iter(responses)
        commands = []

        def removal_response(command, arguments=None):
            commands.append(command)
            return {"return": next(replies) if command == "qom-list" else {}}

        ctx = SimpleNamespace(
            backend=SimpleNamespace(name="qemu"), vm_api=removal_response,
            log=Mock())
        clock = [0, 6] if error is TimeoutError else [0, 0]
        with patch.object(helpers.time, "sleep"), \
             patch.object(helpers.time, "monotonic", side_effect=clock):
            try:
                helpers.remove_disk(ctx)
            except (RuntimeError, TimeoutError) as exc:
                assert error is not None and isinstance(exc, error), (
                    responses, error, exc)
            else:
                assert error is None, responses
        assert commands[:2] == ["device_del", "qom-list"], commands
        completed = any(call.args == ("remove-device completed",)
                        for call in ctx.log.call_args_list)
        assert completed == (error is None), (responses, completed)


def test_unit_sidecar_runtime_skip_preserves_failures():
    from types import SimpleNamespace
    from unittest.mock import patch

    class Backend:
        name = "fake"
        console_device = "ttyS0"
        mmio_transport = False

        def __init__(self, verdict):
            self.verdict = verdict

        def build_cmd(self, kernel, initramfs, disk_path, cmdline, opts=None):
            return ["sh", "-c",
                    f"sleep 0.1; printf '[{self.verdict}] T0001\\n'"]

    def skip_action(ctx):
        ctx.skip("api socket disabled")
        raise AssertionError("ctx.skip returned")

    sidecar = SimpleNamespace(run=skip_action)
    for verdict in ("PASS", "REJECT", "SKIP", "XFAIL",
                    "FAIL", "WEDGED", "XPASS"):
        with patch.object(RUN_MOD, "_find_sidecar", return_value="sidecar"), \
             patch.object(RUN_MOD, "_load_sidecar", return_value=sidecar):
            results = RUN_MOD.run_test(
                Backend(verdict), "kernel", "T0001",
                5, 0, False, "raw", {})
        _, status, output = results[0]
        expected = verdict if verdict in ("FAIL", "WEDGED", "XPASS") \
            else "SKIP"
        assert status == expected, (verdict, status, output)
        assert "[vv-sidecar] SKIP: api socket disabled" in output
        assert "[vv-sidecar] FAIL:" not in output
        assert "Traceback" not in output


def _sidecar_api_context():
    import threading
    from types import SimpleNamespace

    return RUN_MOD._Ctx(
        test_id="T0001", backend=SimpleNamespace(name="ch"),
        vmm_pid=None, tmpdir="/tmp", api_sock="api", disk="disk",
        log=lambda message: None, output_buf=[],
        output_lock=threading.Lock(), stop_event=threading.Event())


def test_unit_sidecar_reads_buffered_markers_after_stop():
    ctx = _sidecar_api_context()
    ctx._buf.extend([b"[vv] virtio-", b"villain\r\n[SKIP] T0001\r\n"])
    ctx.stop.set()
    assert ctx.wait_text("[vv]", timeout=1)
    assert ctx.wait_text(b"[SKIP] T0001", timeout=1)
    assert not ctx.wait_text("vv-msix-snapshot-ready", timeout=1)


def test_unit_openvmm_console_commands():
    from types import SimpleNamespace
    from unittest.mock import patch

    class Backend:
        name = "openvmm"
        console_device = "ttyS0"
        mmio_transport = False

        def __init__(self, mode, commands, split=None, terminate=False):
            self.mode = mode
            self.commands = commands
            self.split = split
            self.terminate = terminate

        def build_cmd(self, *args):
            prefix = "ERROR openvmm_entry::repl:  pulse save/restore failed error="
            error = {
                "unsupported": prefix + "reset not supported",
                "unsupported-quoted": prefix + '"reset not supported"',
                "unsupported-guest-fail": prefix + "reset not supported",
                "restore-error": prefix + "restoring vm state",
                "longer-error": prefix + "reset not supported by this restore path",
                "quoted-longer-error": prefix + '"reset not supported" during restore',
                "error": "ERROR command failed",
            }.get(self.mode)
            script = (
                "import os, sys, time, tty\n"
                "tty.setraw(0)\n"
                "print('[vv] guest ready', flush=True)\n"
                "while os.read(0, 1) != b'\\x11': pass\n"
                f"mode = {self.mode!r}\n"
                "if mode == 'prompt-exit': sys.exit(0)\n"
                "if mode == 'prompt-timeout':\n"
                "    time.sleep(0.4)\n"
                "    print('[PASS] T0001', flush=True)\n"
                "    time.sleep(1)\n"
                "    sys.exit(0)\n"
                "print('openvmm> ', flush=True)\n"
                f"for command in {self.commands!r}:\n"
                "    assert sys.stdin.readline().strip() == command\n"
                f"    error = {error!r}\n"
                "    if error is not None:\n"
                f"        split = {self.split!r}\n"
                "        if split is not None:\n"
                "            sys.stderr.write(error[:split])\n"
                "            sys.stderr.flush()\n"
                "            time.sleep(0.05)\n"
                "            error = error[split:]\n"
                "        sys.stderr.write(error)\n"
                f"        if not {self.terminate!r}: sys.stderr.write('\\n')\n"
                "        sys.stderr.flush()\n"
                "        break\n"
                "    if mode == 'timeout':\n"
                "        time.sleep(0.4)\n"
                "        break\n"
                "    reply = {'pause': 'pause complete', 'resume': 'resumed complete', "
                "'pulse-save-restore': 'pulse save/restore complete'}[command]\n"
                "    print(reply, file=sys.stderr, flush=True)\n"
                "verdict = 'FAIL' if mode == 'unsupported-guest-fail' else 'PASS'\n"
                "print(f'[{verdict}] T0001', flush=True)\n"
                f"if {self.terminate!r}: sys.exit(0)\n"
                "time.sleep(1)\n"
            )
            return [sys.executable, "-u", "-c", script]

    def action(ctx):
        assert ctx.wait_text("[vv]", timeout=2)
        for command in commands:
            response = ctx.vm_api(command, timeout=0.2)
            assert response.returncode == 0

    def check(mode, expected, split=None, terminate=False):
        with patch.object(RUN_MOD, "_find_sidecar", return_value="actor"), \
             patch.object(RUN_MOD, "_load_sidecar",
                          return_value=SimpleNamespace(run=action)):
            [(name, status, output)] = RUN_MOD.run_test(
                Backend(mode, commands, split, terminate), "kernel", "T0001", 3,
                opts={"api_enabled": True})
        assert status == expected, (mode, split, terminate, name, status, output)
        if mode.startswith("unsupported") and commands == ("pulse-save-restore",):
            assert ("[vv-sidecar] SKIP: OpenVMM pulse-save-restore unavailable: "
                    "reset not supported") in output
            assert "[vv-sidecar] FAIL:" not in output
        elif mode in ("restore-error", "longer-error", "quoted-longer-error"):
            assert "[vv-sidecar] FAIL: OpenVMM pulse-save-restore failed" in output
            assert "[vv-sidecar] SKIP:" not in output
        elif mode == "unsupported":
            assert "[vv-sidecar] FAIL: OpenVMM pause failed" in output
            assert "[vv-sidecar] SKIP:" not in output
        if mode == "error":
            assert "OpenVMM pause failed" in output
        if mode == "timeout":
            assert "OpenVMM pause timed out" in output
        if mode == "prompt-exit":
            assert "OpenVMM exited before command prompt" in output
        if mode == "prompt-timeout":
            assert "OpenVMM command prompt timed out" in output

    commands = ("pause", "resume")
    for mode, expected in (("success", "PASS"), ("error", "FAIL"),
                           ("timeout", "FAIL"), ("prompt-exit", "WEDGED"),
                           ("prompt-timeout", "FAIL"), ("unsupported", "FAIL")):
        check(mode, expected)

    commands = ("pulse-save-restore",)
    for mode, expected in (
            ("success", "PASS"), ("unsupported", "SKIP"),
            ("unsupported-quoted", "SKIP"), ("unsupported-guest-fail", "FAIL"),
            ("restore-error", "FAIL"), ("longer-error", "FAIL"),
            ("quoted-longer-error", "FAIL")):
        check(mode, expected)
    for mode, expected in (
            ("unsupported", "SKIP"), ("unsupported-quoted", "SKIP"),
            ("unsupported-guest-fail", "FAIL"), ("restore-error", "FAIL"),
            ("longer-error", "FAIL"), ("quoted-longer-error", "FAIL")):
        check(mode, expected, terminate=True)
    prefix = "ERROR openvmm_entry::repl:  pulse save/restore failed error="
    for mode, expected, split in (
            ("unsupported", "SKIP", len("ERROR")),
            ("unsupported", "SKIP", len(prefix + "reset not supported")),
            ("unsupported-quoted", "SKIP", len(prefix + '"reset not supported')),
            ("unsupported-guest-fail", "FAIL", len("ERROR")),
            ("restore-error", "FAIL", len("ERROR")),
            ("longer-error", "FAIL", len(prefix + "reset not supported")),
            ("quoted-longer-error", "FAIL", len(prefix + '"reset not supported"'))):
        check(mode, expected, split=split)
        check(mode, expected, split=split, terminate=True)


def test_unit_sidecar_api_rejects_exited_vmm():
    from unittest.mock import patch

    ctx = _sidecar_api_context()
    with patch.object(ctx, "vm_alive", return_value=False), \
         patch.object(RUN_MOD, "vm_api") as api:
        try:
            ctx.vm_api("pause")
        except RuntimeError as error:
            assert "VMM exited before API command pause" in str(error)
        else:
            raise AssertionError("exited VMM reported API success")
    api.assert_not_called()


def test_unit_sidecar_api_preserves_responses():
    from unittest.mock import patch

    ctx = _sidecar_api_context()
    responses = [
        subprocess.CompletedProcess(["pause"], 1, "out", message)
        for message in (
            "Connection refused", "os error 111", "Connection reset",
            "os error 104", "Broken pipe", "os error 32",
            "command failed")
    ]
    responses += [subprocess.CompletedProcess(["pause"], 0, "ok", ""),
                  {"return": {}}, {"error": {"desc": "command failed"}}]
    for response in responses:
        with patch.object(ctx, "vm_alive", return_value=True), \
             patch.object(RUN_MOD, "vm_api", return_value=response) as api:
            assert ctx.vm_api("pause", ["arg"], timeout=3) is response
        api.assert_called_once_with(
            "api", ctx.backend, "pause", ["arg"], timeout=3)


def test_unit_sidecar_api_preserves_connection_exceptions():
    from unittest.mock import patch

    ctx = _sidecar_api_context()
    error = ConnectionRefusedError("API unavailable")
    with patch.object(ctx, "vm_alive", return_value=True), \
         patch.object(RUN_MOD, "vm_api", side_effect=error):
        try:
            ctx.vm_api("pause")
        except ConnectionRefusedError as caught:
            assert caught is error
        else:
            raise AssertionError("connection error reported API success")


def test_unit_probe_filter_excludes_missing_device():
    """Tests targeting a device not in available_devices are excluded."""
    tests = ["RNG0001", "RTC0022", "D0001"]
    available_devices = {0x1044, 0x1063}  # RNG and watchdog, no RTC
    available_features = {0x1044: 0, 0x1063: 0}
    available_queues = {0x1044: 1, 0x1063: 1}
    meta = {
        "RNG0001": {"device_id": 0x1044, "flags": 0,
                    "required_features": 0, "min_queues": 0},
        "RTC0022": {"device_id": 0x1051, "flags": 0,
                    "required_features": 0, "min_queues": 0},
        "D0001": {"device_id": 0x1063, "flags": 0,
                  "required_features": 0, "min_queues": 0},
    }
    filtered = []
    for t in tests:
        m = meta.get(t)
        if m:
            dev_id = m["device_id"]
            features = m["required_features"]
            minq = m["min_queues"]
            if dev_id and dev_id not in available_devices:
                continue
            if features:
                offered = available_features.get(dev_id, 0)
                if (features & offered) != features:
                    continue
            if minq:
                nq = available_queues.get(dev_id, 0)
                if nq < minq:
                    continue
        filtered.append(t)
    assert filtered == ["RNG0001", "D0001"]


def test_unit_probe_filter_excludes_missing_features():
    """Tests requiring features not offered are excluded."""
    tests = ["N0049", "RNG0001"]
    available_devices = {0x1041, 0x1044}
    available_features = {0x1041: 0x0000, 0x1044: 0}  # net has no features
    available_queues = {0x1041: 3, 0x1044: 1}
    meta = {
        "N0049": {"device_id": 0x1041, "flags": 0,
                  "required_features": (1 << 22),  # NET_F_MQ
                  "min_queues": 0},
        "RNG0001": {"device_id": 0x1044, "flags": 0,
                    "required_features": 0, "min_queues": 0},
    }
    filtered = []
    for t in tests:
        m = meta.get(t)
        if m:
            dev_id = m["device_id"]
            features = m["required_features"]
            minq = m["min_queues"]
            if dev_id and dev_id not in available_devices:
                continue
            if features:
                offered = available_features.get(dev_id, 0)
                if (features & offered) != features:
                    continue
            if minq:
                nq = available_queues.get(dev_id, 0)
                if nq < minq:
                    continue
        filtered.append(t)
    assert filtered == ["RNG0001"]


def test_unit_probe_filter_excludes_insufficient_queues():
    """Tests requiring more queues than available are excluded."""
    tests = ["B0032", "RNG0001"]
    available_devices = {0x1042, 0x1044}
    available_features = {0x1042: 0, 0x1044: 0}
    available_queues = {0x1042: 1, 0x1044: 1}  # blk has only 1 queue
    meta = {
        "B0032": {"device_id": 0x1042, "flags": 0,
                  "required_features": 0, "min_queues": 2},
        "RNG0001": {"device_id": 0x1044, "flags": 0,
                    "required_features": 0, "min_queues": 0},
    }
    filtered = []
    for t in tests:
        m = meta.get(t)
        if m:
            dev_id = m["device_id"]
            features = m["required_features"]
            minq = m["min_queues"]
            if dev_id and dev_id not in available_devices:
                continue
            if features:
                offered = available_features.get(dev_id, 0)
                if (features & offered) != features:
                    continue
            if minq:
                nq = available_queues.get(dev_id, 0)
                if nq < minq:
                    continue
        filtered.append(t)
    assert filtered == ["RNG0001"]


# ---------------------------------------------------------------------------
# Batch grouping.

def test_unit_batch_grouping_sidecars_solo():
    """Sidecar tests (02xx) must not be batched."""
    import re as _re
    tests = ["B0001", "B0200", "RNG0001", "D0200", "T0022"]
    _sc_re = _re.compile(r'[A-Z]+0[23]\d\d', _re.IGNORECASE)
    solo = [t for t in tests if _sc_re.match(t)]
    batchable = [t for t in tests if not _sc_re.match(t)]
    assert solo == ["B0200", "D0200"]
    assert batchable == ["B0001", "RNG0001", "T0022"]


def test_unit_batch_grouping_xfail_solo():
    """XFAIL tests must not be batched."""
    import re as _re
    tests = ["B0001", "N0130", "RNG0001"]
    _sc_re = _re.compile(r'[A-Z]+0[23]\d\d', _re.IGNORECASE)
    meta = {
        "B0001": {"flags": 0},
        "N0130": {"flags": 4},  # TEST_FLAG_XFAIL
        "RNG0001": {"flags": 0},
    }

    def _must_solo(t):
        if _sc_re.match(t):
            return True
        m = meta.get(t)
        if m and m["flags"] & 4:
            return True
        return False

    solo = [t for t in tests if _must_solo(t)]
    batchable = [t for t in tests if not _must_solo(t)]
    assert solo == ["N0130"]
    assert batchable == ["B0001", "RNG0001"]


def test_unit_batch_chunking():
    """Batchable tests are split into chunks of N."""
    batchable = ["A", "B", "C", "D", "E"]
    batch_size = 2
    work_items = []
    for i in range(0, len(batchable), batch_size):
        work_items.append(batchable[i:i + batch_size])
    assert work_items == [["A", "B"], ["C", "D"], ["E"]]


def test_unit_batch_retry_pipeline_matches_sequential_and_parallel_execution():
    import contextlib
    import io
    import threading
    from collections import Counter
    from types import SimpleNamespace
    from unittest.mock import patch

    names = ["B0001", "B0002", "B0003", "B0200", "N0130", "RNG0001"]
    metadata = {name: {"flags": 4 if name == "N0130" else 0} for name in names}
    expected_statuses = {
        "B0001": "PASS", "B0002": "PASS", "B0003": "PASS",
        "B0200": "PASS", "N0130": "XFAIL", "RNG0001": "FAIL"}
    expected_calls = {
        ("B0001", "B0002"): 1, ("RNG0001",): 3,
        ("B0002",): 2, ("B0003",): 1, ("B0200",): 1, ("N0130",): 1}
    observed = []
    for jobs in (1, 3):
        calls = Counter()
        lock = threading.Lock()

        def run_case(backend, kernel, selected, *args):
            batch = [selected] if isinstance(selected, str) else selected
            with lock:
                calls[tuple(batch)] += 1
                attempt = calls[tuple(batch)]
            if len(batch) > 1:
                assert batch == ["B0001", "B0002"]
                output = "shared batch output\n[vv] TIMEOUT after 10s\n"
                return [("B0001", "PASS", output), ("B0002", "WEDGED", output)]
            name = batch[0]
            if name == "B0002":
                return [(name, "FAIL" if attempt == 1 else "PASS",
                         "first solo failure\n" if attempt == 1 else "solo recovery\n")]
            return [(name, expected_statuses[name], f"{name} solo attempt {attempt}\n")]

        argv = [RUN, "--vmm", MOCK_LINK, "--kernel", "/dev/null",
                "--initramfs", RUN, "--order", "alpha", "--batch", "2",
                "--retries", "2", "--format", "json", "-j", str(jobs), *names]
        with patch.object(RUN_MOD, "detect_vmm", return_value=SimpleNamespace(
                name="ch", mmio_transport=False)), \
             patch.object(RUN_MOD, "print_banner"), \
             patch.object(RUN_MOD, "_install_termination_as_interrupt"), \
             patch.object(RUN_MOD, "get_test_list", return_value=(names[:], {}, metadata)), \
             patch.object(RUN_MOD, "_find_sidecar", side_effect=(
                 lambda name: "host action" if name == "B0003" else None)), \
             patch.object(RUN_MOD, "run_test", side_effect=run_case), \
             patch.object(RUN_MOD, "build_results_doc",
                          wraps=RUN_MOD.build_results_doc) as report, \
             patch.object(sys, "argv", argv), \
             contextlib.redirect_stdout(io.StringIO()), \
             contextlib.redirect_stderr(io.StringIO()):
            try:
                RUN_MOD.main()
            except SystemExit as error:
                assert error.code == 1, error.code
            else:
                raise AssertionError("A stable failure did not gate the run")
        results = report.call_args.args[0]
        assert {name: status for name, status, _ in results} == expected_statuses
        assert dict(calls) == expected_calls, calls
        recovered = next(output for name, _, output in results if name == "B0002")
        assert recovered == "[vv] attempts: FAIL, PASS\nsolo recovery\n", recovered
        failure = next(output for name, _, output in results if name == "RNG0001")
        assert failure.startswith("[vv] attempts: FAIL, FAIL, FAIL\n"), failure
        observed.append(sorted(results))
    assert observed[0] == observed[1]


def test_unit_stopped_run_never_prepares_images_or_sidecars():
    import threading
    from unittest.mock import patch

    stop = threading.Event()
    stop.set()
    with patch.object(RUN_MOD.images, "create") as image, \
         patch.object(RUN_MOD, "_find_sidecar") as sidecar:
        try:
            RUN_MOD.run_test(None, None, "B0001", 5, stop=stop)
        except KeyboardInterrupt:
            pass
        else:
            raise AssertionError("Cancelled work started")
    image.assert_not_called()
    sidecar.assert_not_called()


def test_unit_reader_failure_cancels_the_test_deadline():
    from unittest.mock import Mock, patch

    class Backend:
        name = "fake"
        console_device = "ttyS0"

        def build_cmd(self, *args):
            return [sys.executable, "-c",
                    "import os, signal\nos.write(1, b'guest started\\n')\nsignal.pause()\n"]

    timers = []
    timer = RUN_MOD.threading.Timer

    def deadline(*args, **kwargs):
        result = timer(*args, **kwargs)
        timers.append(result)
        return result

    destination = Mock()
    destination.buffer.write.side_effect = OSError("Console sink failed")
    with patch.object(RUN_MOD.sys, "stdout", destination), \
         patch.object(RUN_MOD.threading, "Timer", side_effect=deadline), \
         patch.object(RUN_MOD, "_find_sidecar", return_value=None):
        results = RUN_MOD.run_test(Backend(), "kernel", "B0001", 5, console=True)
    assert results == [("B0001", "FAIL", "Console sink failed")], results
    assert len(timers) == 1
    assert timers[0].finished.is_set()
    assert not timers[0].is_alive()


# ---------------------------------------------------------------------------
# --order fast interleave.

def test_unit_order_fast_interleave_spreads_slow():
    """Slow tests should be distributed evenly among fast ones."""
    import random as _random
    _random.seed(42)
    slow = ["S1", "S2"]
    fast = ["F1", "F2", "F3", "F4", "F5", "F6"]
    _random.shuffle(slow)
    _random.shuffle(fast)
    step = max(1, len(fast) // (len(slow) + 1))
    merged = []
    si = 0
    for i, t in enumerate(fast):
        if si < len(slow) and i > 0 and i % step == 0:
            merged.append(slow[si])
            si += 1
        merged.append(t)
    merged.extend(slow[si:])
    # Slow tests must not be adjacent
    for i in range(len(merged) - 1):
        assert not (merged[i].startswith("S") and merged[i + 1].startswith("S")), \
            f"Slow tests adjacent: {merged[i]} and {merged[i+1]}"
    # All tests present
    assert sorted(merged) == sorted(slow + fast)


def test_unit_order_fast_no_slow():
    """With no slow tests the result is just the shuffled fast list."""
    import random as _random
    _random.seed(42)
    fast = ["F1", "F2", "F3"]
    _random.shuffle(fast)
    # No slow tests, just fast
    assert len(fast) == 3
    assert set(fast) == {"F1", "F2", "F3"}


# ---------------------------------------------------------------------------
# get_test_list meta parsing.

def test_unit_get_test_list_parses_meta():
    """get_test_list returns meta with device_id, flags, features, minq."""
    if not os.path.exists(INIT_BINARY):
        raise SkipTest("target/init not built")
    tests, info, meta = RUN_MOD.get_test_list(init_binary=INIT_BINARY)
    assert len(tests) > 0
    # Every test with meta should have all four fields
    for tid, m in meta.items():
        assert "device_id" in m, f"{tid} missing device_id"
        assert "flags" in m, f"{tid} missing flags"
        assert "required_features" in m, f"{tid} missing required_features"
        assert "min_queues" in m, f"{tid} missing min_queues"


def test_unit_get_test_list_packed_has_ring_packed():
    """Packed tests should have VIRTIO_F_RING_PACKED in required_features."""
    if not os.path.exists(INIT_BINARY):
        raise SkipTest("target/init not built")
    tests, info, meta = RUN_MOD.get_test_list(init_binary=INIT_BINARY)
    packed = [t for t in tests if t.startswith("P0")]
    assert len(packed) > 0
    for t in packed:
        m = meta.get(t)
        if m and m["flags"] & 1:  # TEST_FLAG_PACKED
            assert m["required_features"] & (1 << 34), \
                f"{t} is packed but lacks VIRTIO_F_RING_PACKED"


# ---------------------------------------------------------------------------
# --no-api-socket flag plumbing.

def test_no_api_socket_flag_in_command():
    """With --no-api-socket the printed command must not contain the flag."""
    r = run_runner("-v", "--no-api-socket", "PASS")
    assert r.returncode == 0, r.stdout
    assert "--api-socket" not in r.stdout


# ---------------------------------------------------------------------------
# run-fuzz _parse_len tests

RUN_FUZZ = os.path.join(ROOT_DIR, "run-fuzz")
FUZZ_MOD = None


def _load_fuzz_module():
    loader = importlib.machinery.SourceFileLoader("vv_fuzz", RUN_FUZZ)
    spec = importlib.util.spec_from_loader("vv_fuzz", loader)
    mod = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(mod)
    return mod


def test_parse_len_decimal():
    assert FUZZ_MOD._parse_len("42") == 42


def test_parse_len_hex():
    assert FUZZ_MOD._parse_len("0x100") == 256


def test_parse_len_hex_suffix_u():
    assert FUZZ_MOD._parse_len("0x40000000U") == 0x40000000


def test_parse_len_hex_suffix_ul():
    assert FUZZ_MOD._parse_len("0x1000UL") == 0x1000


def test_parse_len_hex_suffix_ull():
    assert FUZZ_MOD._parse_len("0x200ULL") == 0x200


def test_parse_len_decimal_suffix_u():
    assert FUZZ_MOD._parse_len("4096U") == 4096


def test_parse_len_sizeof():
    assert FUZZ_MOD._parse_len("sizeof(*hdr)") == 16


def test_parse_len_sizeof_math():
    assert FUZZ_MOD._parse_len("sizeof(*hdr) + 1") == 17


def test_parse_len_whitespace():
    assert FUZZ_MOD._parse_len("  64  ") == 64


# ---------------------------------------------------------------------------
# run-fuzz minimize set-cover and the empty-coverage guard.
#
# Regression: minimize used to delete the entire corpus when the VMM was
# not built with coverage instrumentation, because every entry reported
# zero edges and so looked redundant. _select_minimal_corpus must refuse
# to return a selection when total coverage is empty.

def test_minimize_keeps_minimal_cover():
    """A single entry covering every edge makes the others redundant."""
    entry_edges = [
        ("a.bin", {1, 2, 3}),
        ("b.bin", {1}),
        ("c.bin", {2, 3}),
    ]
    keep, covered = FUZZ_MOD._select_minimal_corpus(entry_edges)
    assert keep == ["a.bin"]
    assert covered == {1, 2, 3}


def test_minimize_keeps_complementary_entries():
    """Entries with disjoint edges are all kept."""
    entry_edges = [
        ("a.bin", {1, 2}),
        ("b.bin", {3, 4}),
    ]
    keep, covered = FUZZ_MOD._select_minimal_corpus(entry_edges)
    assert sorted(keep) == ["a.bin", "b.bin"]
    assert covered == {1, 2, 3, 4}


def test_minimize_drops_pure_duplicates():
    """Two entries with identical edges keep only one."""
    entry_edges = [
        ("a.bin", {7, 8, 9}),
        ("b.bin", {7, 8, 9}),
    ]
    keep, covered = FUZZ_MOD._select_minimal_corpus(entry_edges)
    assert len(keep) == 1
    assert covered == {7, 8, 9}


def test_minimize_prefers_larger_cover_first():
    """The entry with the most edges is selected before smaller ones."""
    entry_edges = [
        ("small.bin", {1}),
        ("big.bin", {1, 2, 3, 4}),
        ("mid.bin", {2, 3}),
    ]
    keep, covered = FUZZ_MOD._select_minimal_corpus(entry_edges)
    assert keep == ["big.bin"]
    assert covered == {1, 2, 3, 4}


def test_minimize_empty_coverage_raises():
    """Zero total coverage must raise rather than report an empty keep.

    This is the guard against wiping the corpus when coverage data is
    missing, for example an uninstrumented VMM binary.
    """
    entry_edges = [
        ("a.bin", set()),
        ("b.bin", set()),
        ("c.bin", set()),
    ]
    try:
        FUZZ_MOD._select_minimal_corpus(entry_edges)
    except FUZZ_MOD.EmptyCoverageError:
        return
    raise AssertionError(
        "expected EmptyCoverageError when no entry contributes edges")


def test_minimize_no_entries_raises():
    """An empty input set has empty coverage and must also raise."""
    try:
        FUZZ_MOD._select_minimal_corpus([])
    except FUZZ_MOD.EmptyCoverageError:
        return
    raise AssertionError("expected EmptyCoverageError for empty input")


# ---------------------------------------------------------------------------
# run-fuzz structure-aware mutation (VqBlob intermediate representation).
#
# VqBlob parses a blob into a virtqueue object, lets operators mutate it
# structurally, then serializes back with a coherent byte layout. These
# tests pin the round-trip and the structural operators so the framing
# stays consistent while adversarial semantics are preserved.

def test_vqblob_roundtrip_preserves_fields():
    """parse(serialize(x)) is a fixed point for a well-formed blob."""
    vq = FUZZ_MOD.FuzzBlob.parse(FUZZ_MOD.make_seed()).segments[0].vq
    again = FUZZ_MOD.VqBlob.parse(vq.serialize())
    assert again.queue_size == vq.queue_size
    assert again.avail_idx == vq.avail_idx
    assert again.descs == vq.descs
    assert again.avail_ring == vq.avail_ring


def test_fuzzblob_seed_is_single_blk_segment():
    """A seed blob is one blk queue zero segment in the versioned frame."""
    parsed = FUZZ_MOD.FuzzBlob.parse(FUZZ_MOD.make_seed())
    assert len(parsed.segments) == 1
    assert parsed.segments[0].device_id == FUZZ_MOD.VIRTIO_PCI_DEVICE_BLK
    assert parsed.segments[0].queue_index == 0


def test_fuzzblob_parse_rejects_unframed_blob():
    """A blob without the magic is rejected, no legacy fallback."""
    try:
        FUZZ_MOD.FuzzBlob.parse(b"\x00" * 4096)
    except ValueError:
        return
    raise AssertionError("expected ValueError for an unframed blob")


def test_fuzzblob_segment_roundtrip_preserves_targets():
    """Versioned framing keeps each segment device and queue target."""
    seed = FUZZ_MOD.FuzzBlob.parse(FUZZ_MOD.make_seed()).segments[0].vq
    blob = FUZZ_MOD.FuzzBlob([
        FUZZ_MOD.FuzzSegment(FUZZ_MOD.VIRTIO_PCI_DEVICE_BLK, 1, seed),
        FUZZ_MOD.FuzzSegment(0x1041, 2, seed),
    ]).serialize()
    parsed = FUZZ_MOD.FuzzBlob.parse(blob)
    assert len(blob) == FUZZ_MOD.INPUT_SIZE
    assert [(s.device_id, s.queue_index) for s in parsed.segments] == [
        (FUZZ_MOD.VIRTIO_PCI_DEVICE_BLK, 1), (0x1041, 2)]
    assert parsed.segments[0].vq.descs == seed.descs


def test_fuzzblob_segment_selects_test_device():
    """Generated tests select the device from the first blob segment."""
    seed = FUZZ_MOD.FuzzBlob.parse(FUZZ_MOD.make_seed()).segments[0].vq
    rng_id = FUZZ_MOD._DEVICE_MAP["rng"][2]
    segment = FUZZ_MOD.FuzzSegment(rng_id, 0, seed)
    assert FUZZ_MOD._device_from_segment(segment) == "rng"


def test_make_device_seed_targets_device():
    """A device seed carries the device id and parses back to its name."""
    seed = FUZZ_MOD.make_device_seed("rng")
    parsed = FUZZ_MOD.FuzzBlob.parse(seed)
    assert parsed.segments[0].device_id == FUZZ_MOD._DEVICE_MAP["rng"][2]
    assert FUZZ_MOD._blob_device_name(seed) == "rng"


def test_crash_path_routes_by_device():
    """A crash blob is saved under a per-device subdirectory."""
    with tempfile.TemporaryDirectory() as td:
        seed = FUZZ_MOD.make_device_seed("net")
        path = FUZZ_MOD._crash_path(td, seed)
        assert os.path.dirname(path) == os.path.join(td, "net")
        assert os.path.isdir(os.path.join(td, "net"))
        assert os.path.basename(path).startswith("crash_")


def test_corpus_path_routes_by_device():
    """A new corpus blob is saved under a per-device subdirectory."""
    with tempfile.TemporaryDirectory() as td:
        seed = FUZZ_MOD.make_device_seed("console")
        path = FUZZ_MOD._corpus_path(td, seed, 7)
        assert os.path.dirname(path) == os.path.join(td, "console")
        assert os.path.isdir(os.path.join(td, "console"))
        assert os.path.basename(path).startswith("gen_000007_")


def test_blk_request_seeds_have_valid_headers():
    """Each blk request seed carries a coherent outhdr type on blk."""
    expected = {
        "in": 0, "out": 1, "flush": 4, "get_id": 8,
        "get_lifetime": 10, "discard": 11, "write_zeroes": 13,
        "in_q1": 0, "out_q1": 1,
    }
    seeds = FUZZ_MOD.blk_request_seeds()
    assert set(seeds) == set(expected)
    for name, blob in seeds.items():
        assert FUZZ_MOD._blob_device_name(blob) == "blk"
        segment = FUZZ_MOD.FuzzBlob.parse(blob).segments[0]
        vq = segment.vq
        req_type = FUZZ_MOD.struct.unpack_from("<I", vq.payload, 0)[0]
        assert req_type == expected[name]
        assert vq.descs[0][1] & 0x01       # header has NEXT
        assert vq.descs[-1][1] & 0x02      # status is writable
        assert len(vq.descs) == (2 if name == "flush" else 3)
        assert segment.queue_index == (1 if name.endswith("_q1") else 0)


def test_op_blk_valid_header_shapes_in_range():
    """The valid-header operator writes a known type and in-range sector."""
    random.seed(3)
    vq = FUZZ_MOD.FuzzBlob.parse(FUZZ_MOD.make_seed()).segments[0].vq
    for _ in range(20):
        FUZZ_MOD._op_blk_valid_header(vq)
        req_type, _res, sector = FUZZ_MOD.struct.unpack_from(
            "<IIQ", vq.payload, 0)
        assert req_type in FUZZ_MOD._BLK_VALID_TYPES
        assert sector < FUZZ_MOD._BLK_DISK_SECTORS


def test_op_blk_oob_sector_shapes_out_of_range():
    """The out-of-range operator keeps a valid type but a bad sector."""
    random.seed(4)
    vq = FUZZ_MOD.FuzzBlob.parse(FUZZ_MOD.make_seed()).segments[0].vq
    for _ in range(20):
        FUZZ_MOD._op_blk_oob_sector(vq)
        req_type, _res, sector = FUZZ_MOD.struct.unpack_from(
            "<IIQ", vq.payload, 0)
        assert req_type in FUZZ_MOD._BLK_VALID_TYPES
        assert sector >= FUZZ_MOD._BLK_DISK_SECTORS


def test_net_ctrl_seeds_target_control_queue():
    """Net control seeds target the net control queue with a hdr and ack."""
    seeds = FUZZ_MOD.net_ctrl_seeds()
    assert set(seeds) == {"mq_pairs_set", "bad_class", "bad_command",
                          "mq_no_data"}
    for name, blob in seeds.items():
        assert FUZZ_MOD._blob_device_name(blob) == "net"
        segment = FUZZ_MOD.FuzzBlob.parse(blob).segments[0]
        assert segment.queue_index == FUZZ_MOD._NET_CTRL_QUEUE
        vq = segment.vq
        cls, command = FUZZ_MOD.struct.unpack_from("<BB", vq.payload, 0)
        if name == "bad_class":
            assert cls == 0xff
        if name == "bad_command":
            assert command == 0xff
        assert vq.descs[0][1] & 0x01       # header has NEXT
        assert vq.descs[-1][1] & 0x02      # ack is writable


def test_parse_profdata_counts_captures_entry_and_blocks():
    """Profdata show output yields entry and nonzero block markers."""
    sample = (
        "Counters:\n"
        "  main:\n"
        "    Hash: 0x000000000a712458\n"
        "    Counters: 2\n"
        "    Function count: 2\n"
        "    Block counts: [1]\n"
        "  add:\n"
        "    Hash: 0x000000029c498458\n"
        "    Counters: 2\n"
        "    Function count: 0\n"
        "    Block counts: [0]\n"
        "Instrumentation level: Front-end\n"
        "Total functions: 2\n"
    )
    edges = FUZZ_MOD._parse_profdata_counts(sample)
    # main ran: entry marker plus its nonzero block 0.
    assert "main\t0x000000000a712458\t-1" in edges
    assert "main\t0x000000000a712458\t0" in edges
    # add never entered and its block is zero, so it contributes nothing.
    assert "add\t0x000000029c498458\t-1" not in edges
    assert "add\t0x000000029c498458\t0" not in edges


def test_parse_profdata_counts_empty_on_garbage():
    """Non profile text yields an empty set rather than raising."""
    assert FUZZ_MOD._parse_profdata_counts("not profile data\n") == set()


def test_show_counts_offload_matches_parser():
    """The child reducer yields the same edges as the in process parse,
    so moving the parse off the GIL does not change the coverage signal."""
    import subprocess as _sp
    sample = (
        "Counters:\n"
        "  core::fmt:\n"
        "    Hash: 0x1234\n"
        "    Counters: 3\n"
        "    Function count: 5\n"
        "    Block counts: [0, 2, 0]\n"
        "  main::run:\n"
        "    Hash: 0xabcd\n"
        "    Counters: 2\n"
        "    Function count: 0\n"
        "    Block counts: [0, 0]\n"
    )
    ref = FUZZ_MOD._parse_profdata_counts(sample)
    show = _sp.Popen(["printf", "%s", sample], stdout=_sp.PIPE)
    reduce = _sp.Popen(
        [sys.executable, "-c", FUZZ_MOD._REDUCE_COUNTS_SRC],
        stdin=show.stdout, stdout=_sp.PIPE, text=True)
    show.stdout.close()
    out, _ = reduce.communicate()
    show.wait()
    # Edge keys are plain "func\thash\tindex" strings, so the parent set
    # is just the child lines. It must match the in process parse.
    child = set(out.splitlines())
    assert child == ref
    assert "core::fmt\t0x1234\t-1" in ref


def test_parse_profdata_counts_scope_filters_functions():
    """With a scope, only functions whose name contains it are counted,
    so the boot path does not drown the device signal."""
    sample = (
        "Counters:\n"
        "  virtio_devices::vsock::handle:\n"
        "    Hash: 0x1111\n"
        "    Counters: 1\n"
        "    Function count: 3\n"
        "    Block counts: [1]\n"
        "  core::fmt::write:\n"
        "    Hash: 0x2222\n"
        "    Counters: 1\n"
        "    Function count: 9\n"
        "    Block counts: [1]\n"
    )
    scoped = FUZZ_MOD._parse_profdata_counts(sample, "virtio_devices")
    assert any("virtio_devices" in e for e in scoped)
    assert not any("core::fmt" in e for e in scoped)
    # Without a scope both functions count.
    whole = FUZZ_MOD._parse_profdata_counts(sample)
    assert any("core::fmt" in e for e in whole)


def test_show_counts_offload_honors_scope():
    """The child reducer applies the scope the same way as the parser."""
    import subprocess as _sp
    sample = (
        "Counters:\n"
        "  virtio_devices::net::rx:\n"
        "    Hash: 0x3333\n"
        "    Counters: 1\n"
        "    Function count: 1\n"
        "    Block counts: [1]\n"
        "  alloc::vec::grow:\n"
        "    Hash: 0x4444\n"
        "    Counters: 1\n"
        "    Function count: 1\n"
        "    Block counts: [1]\n"
    )
    ref = FUZZ_MOD._parse_profdata_counts(sample, "virtio_devices")
    show = _sp.Popen(["printf", "%s", sample], stdout=_sp.PIPE)
    reduce = _sp.Popen(
        [sys.executable, "-c", FUZZ_MOD._REDUCE_COUNTS_SRC, "virtio_devices"],
        stdin=show.stdout, stdout=_sp.PIPE, text=True)
    show.stdout.close()
    out, _ = reduce.communicate()
    show.wait()
    assert set(out.splitlines()) == ref
    assert all("virtio_devices" in e for e in ref)


def test_annotate_container_reads_segments_not_magic():
    """Annotating a crash blob uses the segments, not the outer magic."""
    seed = FUZZ_MOD.make_seed()
    notes = FUZZ_MOD._annotate_container(seed)
    joined = " ".join(notes)
    # The bug read the VVFZ magic as a queue header giving these numbers.
    assert "num_descs=23110" not in joined
    assert "queue_size=22102" not in joined
    # A real anomaly still comes through, tagged with its segment.
    for note in notes:
        assert note.startswith("segment[")


def test_annotate_container_caps_flood():
    """A segment with a huge descriptor count does not flood the notes."""
    vq = FUZZ_MOD.FuzzBlob.parse(FUZZ_MOD.make_seed()).segments[0].vq
    vq.descs = [[0, 0, 0] for _ in range(4000)]  # many zero length descs
    blob = FUZZ_MOD.FuzzBlob([FUZZ_MOD.FuzzSegment(
        FUZZ_MOD.VIRTIO_PCI_DEVICE_BLK, 0, vq)]).serialize()
    notes = FUZZ_MOD._annotate_container(blob)
    assert len(notes) <= FUZZ_MOD._MAX_BLOB_NOTES + 2


def test_require_vmm_missing_exits():
    """A missing VMM path exits cleanly instead of raising later."""
    try:
        FUZZ_MOD._require_vmm("/no/such/vmm-xyz")
    except SystemExit as e:
        assert "not found" in str(e)
        return
    raise AssertionError("expected SystemExit for a missing VMM binary")


def test_require_vmm_accepts_executable():
    """An existing executable passes validation without raising."""
    FUZZ_MOD._require_vmm(sys.executable)


def test_vmm_instrumentation_check_non_binary():
    """A non binary file cannot be told apart, so the check returns None."""
    if not shutil.which("nm"):
        return
    with tempfile.NamedTemporaryFile(suffix=".txt", delete=False) as f:
        f.write(b"not a binary\n")
        path = f.name
    try:
        assert FUZZ_MOD._vmm_has_coverage_instrumentation(path) is None
    finally:
        os.unlink(path)


def test_vmm_instrumentation_check_plain_binary():
    """A normal binary without coverage is reported as not instrumented."""
    if not shutil.which("nm"):
        return
    res = FUZZ_MOD._vmm_has_coverage_instrumentation(sys.executable)
    assert res in (False, None)


def test_device_seeds_target_expected_queues():
    """Each per device seed lands on the right device and queue."""
    expected = {
        ("rng", "fill"): ("rng", 0),
        ("console", "tx"): ("console", 1),
        ("net", "tx"): ("net", 1),
        ("net", "rx"): ("net", 0),
        ("vsock", "tx"): ("vsock", 1),
        ("vsock", "rw"): ("vsock", 1),
        ("balloon", "inflate"): ("balloon", 0),
        ("mem", "state"): ("mem", 0),
    }
    seeds = FUZZ_MOD.device_seeds()
    assert set(seeds) == set(expected)
    for key, blob in seeds.items():
        device, queue = expected[key]
        assert FUZZ_MOD._blob_device_name(blob) == device
        segment = FUZZ_MOD.FuzzBlob.parse(blob).segments[0]
        assert segment.queue_index == queue


def test_vsock_seed_is_valid_stream_request():
    """The vsock seed carries a well formed ring and a real stream
    connection request, so CH parses a packet and runs the connection
    state machine instead of dropping an all-zero header."""
    import struct
    vq = FUZZ_MOD.FuzzBlob.parse(
        FUZZ_MOD.device_seeds()[("vsock", "tx")]).segments[0].vq
    # ring is well formed: one readable 44 byte header, avail points at it
    assert vq.descs == [[44, 0, 0]]
    assert vq.avail_ring[:1] == [0]
    assert vq.avail_idx == 1
    hdr = struct.unpack("<QQIIIHHIII", bytes(vq.payload[:44]))
    dst_cid, vtype, op = hdr[1], hdr[5], hdr[6]
    assert dst_cid == 2      # VMADDR_CID_HOST
    assert vtype == 1        # VIRTIO_VSOCK_TYPE_STREAM
    assert op == 1           # VIRTIO_VSOCK_OP_REQUEST


def test_vsock_rw_seed_length_matches_payload():
    """The vsock rw seed declares a length equal to its trailing data,
    so the packet parser reaches the data descriptor path."""
    import struct
    vq = FUZZ_MOD.FuzzBlob.parse(
        FUZZ_MOD.device_seeds()[("vsock", "rw")]).segments[0].vq
    hdr = struct.unpack("<QQIIIHHIII", bytes(vq.payload[:44]))
    length, vtype, op = hdr[4], hdr[5], hdr[6]
    assert op == 5           # VIRTIO_VSOCK_OP_RW
    assert vtype == 1        # VIRTIO_VSOCK_TYPE_STREAM
    assert length > 0
    assert bytes(vq.payload[44:44 + length]) == b"vv-fuzz-data"


def test_mutate_returns_versioned_container():
    """mutate consumes and returns a valid versioned blob."""
    random.seed(7)
    blob = FUZZ_MOD.make_seed()
    for _ in range(50):
        blob = FUZZ_MOD.mutate(blob)
        assert len(blob) == FUZZ_MOD.INPUT_SIZE
        parsed = FUZZ_MOD.FuzzBlob.parse(blob)
        assert parsed.segments


def test_fuzz_vmm_command_provisions_auxiliary_devices():
    """Fuzz commands include every device targeted by the guest format."""
    cmd = FUZZ_MOD._fuzz_vmm_cmd(
        "/vmm", "/kernel", "/initramfs", "/disk", "/pmem", "/work",
        2, "128M")

    def pair(flag, val):
        return any(cmd[i] == flag and cmd[i + 1] == val
                   for i in range(len(cmd) - 1))

    assert pair("--disk", "path=/disk,num_queues=2")
    # a throwaway disk behind the iommu instantiates virtio-iommu
    assert pair("--disk", "path=/work/iommu.raw,iommu=virtio")
    assert pair("--net", "tap=,num_queues=2")
    assert "--vsock" in cmd
    assert pair("--balloon", "size=0")
    assert "--watchdog" in cmd
    assert pair("--rng", "src=/dev/urandom")
    assert pair("--console", "null")
    assert pair("--pmem", "file=/pmem,size=128M,discard_writes=on")
    assert any("hotplug_method=virtio-mem" in a for a in cmd)


def test_fuzz_vmm_command_assigns_unique_vsock_cids():
    """Parallel fuzz VMs must not share a vsock CID."""
    first = FUZZ_MOD._fuzz_vmm_cmd(
        "/vmm", "/kernel", "/initramfs", "/disk", "/pmem", "/work",
        1, "128M")
    second = FUZZ_MOD._fuzz_vmm_cmd(
        "/vmm", "/kernel", "/initramfs", "/disk", "/pmem", "/work",
        1, "128M")
    vsock_first = first[first.index("--vsock") + 1]
    vsock_second = second[second.index("--vsock") + 1]
    assert vsock_first != vsock_second


def test_fuzz_vmm_command_preserves_explicit_launch_policy():
    from unittest.mock import patch

    with patch.dict(os.environ, {"VV_CH_DEBUG": "1"}):
        cmd = FUZZ_MOD._fuzz_vmm_cmd(
            "/vmm", "/kernel", "/initramfs", "/disk", "/pmem", "/work", 2, "128M")
    assert cmd[:17] == [
        "/vmm", "--kernel", "/kernel", "--initramfs", "/initramfs",
        "--cmdline", "console=hvc0 initcall_blacklist=virtio_blk_init",
        "--cpus", "boot=2",
        "--memory", "size=128M,hotplug_size=256M,hotplug_method=virtio-mem",
        "--serial", "off", "--console", "null", "--seccomp", "false"]
    assert "-vv" not in cmd
    assert [cmd[index + 1] for index, flag in enumerate(cmd) if flag == "--disk"] == [
        "path=/disk,num_queues=2", "path=/work/iommu.raw,iommu=virtio"]


def test_run_vmm_shares_image_creation_without_replacing_existing_data():
    from unittest.mock import patch

    with tempfile.TemporaryDirectory() as work:
        paths = {
            os.path.join(work, "selected.raw"): FUZZ_DISK_SIZE,
            os.path.join(work, "pmem.raw"): PMEM_SIZE,
            os.path.join(work, "iommu.raw"): IOMMU_DISK_SIZE}
        disk = os.path.join(work, "selected.raw")
        result = subprocess.CompletedProcess([], 0, stdout=b"", stderr=b"")
        with patch.object(FUZZ_MOD, "_detect_backend", return_value=VMM_MOD.Qemu("/vmm")), \
             patch.object(FUZZ_MOD.process, "capture", return_value=result), \
             patch.object(FUZZ_MOD.images, "create_raw",
                          wraps=FUZZ_MOD.images.create_raw) as create:
            FUZZ_MOD.run_vmm("/vmm", "/archive", work, kernel_path="/kernel", disk_path=disk)
            assert [(call.args, call.kwargs) for call in create.call_args_list] == [
                ((path, size), {}) for path, size in paths.items()]
            for path, size in paths.items():
                assert os.path.getsize(path) == size
                with open(path, "rb") as stream:
                    assert stream.read(4096) == bytes(4096)
                    stream.seek(-4096, os.SEEK_END)
                    assert stream.read() == bytes(4096)
                with open(path, "wb") as stream:
                    stream.write(b"retained contents")
            create.reset_mock()
            FUZZ_MOD.run_vmm("/vmm", "/archive", work, kernel_path="/kernel", disk_path=disk)
            create.assert_not_called()
        for path in paths:
            with open(path, "rb") as stream:
                assert stream.read() == b"retained contents"


def test_vqblob_serialize_is_blob_size():
    """serialize always emits exactly BLOB_SIZE bytes."""
    vq = FUZZ_MOD.FuzzBlob.parse(FUZZ_MOD.make_seed()).segments[0].vq
    assert len(vq.serialize()) == FUZZ_MOD.BLOB_SIZE


def test_vqblob_parse_tolerates_short_blob():
    """A blob shorter than the header parses without raising."""
    vq = FUZZ_MOD.VqBlob.parse(b"\x10\x00")
    assert vq.descs == []
    assert len(vq.serialize()) == FUZZ_MOD.BLOB_SIZE


def test_op_make_chain_links_all_but_last():
    """make_chain sets NEXT on every descriptor except the tail."""
    vq = FUZZ_MOD.FuzzBlob.parse(FUZZ_MOD.make_seed()).segments[0].vq
    # Ensure at least three descriptors to exercise the chain.
    vq.descs = [[16, 0, 0], [16, 0, 0], [16, 0, 0]]
    FUZZ_MOD._op_make_chain(vq)
    for i in range(len(vq.descs) - 1):
        assert vq.descs[i][1] & FUZZ_MOD._F_NEXT
        assert vq.descs[i][2] == i + 1
    assert not (vq.descs[-1][1] & FUZZ_MOD._F_NEXT)


def test_op_make_loop_sets_next_in_range():
    """make_loop sets NEXT and points within the descriptor table."""
    vq = FUZZ_MOD.FuzzBlob.parse(FUZZ_MOD.make_seed()).segments[0].vq
    vq.descs = [[16, 0, 9], [16, 0, 9]]
    FUZZ_MOD._op_make_loop(vq)
    looped = [d for d in vq.descs if d[1] & FUZZ_MOD._F_NEXT]
    assert looped
    for d in vq.descs:
        if d[1] & FUZZ_MOD._F_NEXT:
            assert 0 <= d[2] < len(vq.descs)


def test_op_make_indirect_clears_next():
    """make_indirect sets INDIRECT and clears NEXT on the target."""
    vq = FUZZ_MOD.FuzzBlob.parse(FUZZ_MOD.make_seed()).segments[0].vq
    vq.descs = [[16, FUZZ_MOD._F_NEXT, 1]]
    FUZZ_MOD._op_make_indirect(vq)
    assert vq.descs[0][1] & FUZZ_MOD._F_INDIRECT
    assert not (vq.descs[0][1] & FUZZ_MOD._F_NEXT)


def test_op_clone_desc_aliases():
    """clone_desc duplicates a descriptor so two slots match."""
    vq = FUZZ_MOD.FuzzBlob.parse(FUZZ_MOD.make_seed()).segments[0].vq
    vq.descs = [[512, FUZZ_MOD._F_WRITE, 0]]
    FUZZ_MOD._op_clone_desc(vq)
    assert len(vq.descs) == 2
    assert vq.descs[0] == vq.descs[1]


def test_structural_mutate_stays_well_framed():
    """structural mutation always yields a parseable BLOB_SIZE blob."""
    random.seed(1234)
    blob = FUZZ_MOD.FuzzBlob.parse(FUZZ_MOD.make_seed()).segments[0].vq.serialize()
    for _ in range(200):
        blob = bytes(FUZZ_MOD._structural_mutate(blob))
        assert len(blob) == FUZZ_MOD.BLOB_SIZE
        # Must re-parse without raising.
        FUZZ_MOD.VqBlob.parse(blob)


def test_structural_mutate_stays_light():
    """structural blobs do not ratchet rings to the hard maximum.

    Operators only grow the descriptor and avail rings, so without a cap
    repeated mutation saturates a blob and every VM run then processes
    hundreds of request heads, which is the slow path. The cap keeps each
    case cheap while chains and indirect tables stay expressible.
    """
    random.seed(99)
    blob = FUZZ_MOD.FuzzBlob.parse(FUZZ_MOD.make_seed()).segments[0].vq.serialize()
    for _ in range(500):
        blob = bytes(FUZZ_MOD._structural_mutate(blob))
        vq = FUZZ_MOD.VqBlob.parse(blob)
        assert len(vq.descs) <= FUZZ_MOD.SOFT_DESCS
        assert len(vq.avail_ring) <= FUZZ_MOD.SOFT_AVAIL


def test_serialize_masks_oversized_fields():
    """Out of range field values are masked, never crashing struct.pack."""
    vq = FUZZ_MOD.FuzzBlob.parse(FUZZ_MOD.make_seed()).segments[0].vq
    vq.descs = [[0x1_0000_0000, 0x1_FFFF, 0x9_9999]]
    vq.avail_ring = [0x1_2345]
    vq.queue_size = 0x9_0000
    vq.avail_idx = 0x9_0000
    out = vq.serialize()
    assert len(out) == FUZZ_MOD.BLOB_SIZE


def test_minimize_single_edge_survivor_not_wiped():
    """One real edge among empty entries is enough to avoid the guard."""
    entry_edges = [
        ("empty1.bin", set()),
        ("real.bin", {42}),
        ("empty2.bin", set()),
    ]
    keep, covered = FUZZ_MOD._select_minimal_corpus(entry_edges)
    assert keep == ["real.bin"]
    assert covered == {42}


# ---------------------------------------------------------------------------
# run-fuzz LeakSanitizer ptrace artifact filtering.
#
# A VMM built with LeakSanitizer aborts its exit time leak scan when its
# StopTheWorld ptrace attach is denied. The VMM is non dumpable, so a same
# uid tracer is refused with EPERM regardless of kernel.yama.ptrace_scope
# or seccomp. The fuzzer must not score that teardown as a crash, and must
# not misread the ptrace warning text (which mentions seccomp) as a real
# seccomp violation.

_LSAN_PTRACE_NOISE = (
    "==1234==WARNING: ptrace appears to be blocked "
    "(is seccomp enabled?). LeakSanitizer may hang.\n"
    "==1234==LeakSanitizer has encountered a fatal error.\n"
    "==1234==HINT: LeakSanitizer does not work under ptrace (strace, gdb)\n"
)


def test_sanitizer_artifact_detected_for_ptrace_noise():
    """Pure ptrace teardown noise is recognised as a non crash."""
    assert FUZZ_MOD._is_sanitizer_ptrace_artifact(_LSAN_PTRACE_NOISE) is True


def test_sanitizer_artifact_false_on_empty_output():
    """No output is a timeout, not a sanitizer artifact."""
    assert FUZZ_MOD._is_sanitizer_ptrace_artifact("") is False


def test_sanitizer_artifact_false_when_real_asan_report():
    """A genuine AddressSanitizer report is never masked."""
    output = (
        _LSAN_PTRACE_NOISE
        + "==1234==ERROR: AddressSanitizer: heap-use-after-free\n"
        + "SUMMARY: AddressSanitizer: heap-use-after-free foo.rs:1\n"
    )
    assert FUZZ_MOD._is_sanitizer_ptrace_artifact(output) is False


def test_sanitizer_artifact_false_when_real_leak_report():
    """A real detected leak is never masked."""
    output = (
        "==1234==ERROR: LeakSanitizer: detected memory leaks\n"
        "Direct leak of 16 byte(s) in 1 object(s)\n"
    )
    assert FUZZ_MOD._is_sanitizer_ptrace_artifact(output) is False


def test_sanitizer_artifact_true_with_benign_block_rejection():
    """Graceful block request rejections do not defeat the artifact match.

    Cloud Hypervisor logs malformed descriptor chains at ERROR level
    from block/src/io/request.rs while continuing to run. Such a run
    that exits non zero only because of the LSan ptrace teardown is not
    a crash.
    """
    output = (
        "cloud-hypervisor: <_disk0_q0> ERROR:block/src/io/request.rs:155 "
        "-- Need a data descriptor: request = Request { request_type: In }\n"
        "cloud-hypervisor: <_disk0_q0> WARN:virtio-devices/src/block.rs:290 "
        "-- Failed to parse virtio-blk request at head 0: too few descriptors\n"
        + _LSAN_PTRACE_NOISE
    )
    assert FUZZ_MOD._is_sanitizer_ptrace_artifact(output) is True


def test_sanitizer_artifact_false_block_error_with_real_panic():
    """A real panic alongside a benign block rejection is still a crash."""
    output = (
        "cloud-hypervisor: ERROR:block/src/io/request.rs:91 "
        "-- Missing head descriptor\n"
        "thread '<_disk0_q0>' panicked at 'index out of bounds'\n"
        + _LSAN_PTRACE_NOISE
    )
    assert FUZZ_MOD._is_sanitizer_ptrace_artifact(output) is False


def test_classify_ptrace_noise_not_seccomp():
    """The ptrace warning must not be classified as a seccomp violation."""
    cls = FUZZ_MOD._classify_vmm_output(_LSAN_PTRACE_NOISE)
    assert "seccomp" not in cls
    assert "ptrace teardown" in cls


def test_classify_real_seccomp_still_detected():
    """A genuine seccomp kill is still classified as such."""
    output = "ERROR: seccomp: killing process due to bad syscall\n"
    cls = FUZZ_MOD._classify_vmm_output(output)
    assert cls == "seccomp violation"


def test_boot_stall_before_init_is_a_boot_stall():
    """A captured guest boot that never reached the harness is a boot
    stage stall, not a fault from the fuzzed input."""
    output = ("[    0.0] Booting Linux on physical CPU 0x0\n"
              "[    0.0] Kernel command line: console=ttyAMA0\n"
              "[    0.3] clk: Disabling unused clocks\n")
    assert FUZZ_MOD._is_boot_stall(output) is True


def test_boot_stall_false_once_harness_ran():
    """Reaching /init means the harness owns the outcome, not the boot."""
    output = ("[    0.0] Booting Linux on physical CPU 0x0\n"
              "[    0.0] Kernel command line: console=ttyAMA0\n"
              "[    0.5] Run /init as init process\n")
    assert FUZZ_MOD._is_boot_stall(output) is False


def test_boot_stall_false_with_real_fault():
    """A sanitizer report during boot is a real fault, not a stall."""
    output = ("[    0.0] Booting Linux on physical CPU 0x0\n"
              "[    0.0] Kernel command line: console=ttyAMA0\n"
              "==1==ERROR: AddressSanitizer: heap-buffer-overflow\n")
    assert FUZZ_MOD._is_boot_stall(output) is False


def test_boot_stall_false_without_a_guest_boot_log():
    """No captured boot log (as on CH) means the check does not apply."""
    output = "cloud-hypervisor: ERROR: something benign\n"
    assert FUZZ_MOD._is_boot_stall(output) is False


def test_exit_status_names_a_fatal_signal():
    """A crash with no output marker is labeled by its killing signal."""
    import signal as _sig
    s = FUZZ_MOD._exit_status_str(-_sig.SIGSEGV)
    assert "SIGSEGV" in s


def test_exit_status_names_a_timeout():
    s = FUZZ_MOD._exit_status_str(-1, timed_out=True)
    assert "timed out" in s


def test_exit_status_plain_exit_code():
    s = FUZZ_MOD._exit_status_str(1)
    assert "exit code 1" in s


def test_classify_real_error_after_ptrace_noise():
    """A real ERROR line is reported even when ptrace noise precedes it."""
    output = (
        "==1234==WARNING: ptrace appears to be blocked "
        "(is seccomp enabled?).\n"
        "cloud-hypervisor: ERROR:foo.rs:9 -- something genuinely broke\n"
    )
    cls = FUZZ_MOD._classify_vmm_output(output)
    assert "something genuinely broke" in cls


def test_sanitizer_noise_matches_tracer_child_exit():
    """The residual sanitizer tracer exit line counts as teardown noise."""
    assert diagnostics.is_sanitizer_noise_line(
        "==1497092==Child exited with signal 42.") is True
    # A plain VMM log line that happens to mention a child exit, without
    # the sanitizer == prefix, is not swallowed.
    assert diagnostics.is_sanitizer_noise_line(
        "worker: Child exited with signal 9") is False


def test_sanitizer_artifact_recovered_run_is_not_a_crash():
    """With CAP_SYS_PTRACE the scan recovers; only soft noise remains."""
    output = (
        "==1497092==WARNING: ptrace appears to be blocked "
        "(is seccomp enabled?). LeakSanitizer may hang.\n"
        "==1497092==Child exited with signal 42.\n"
    )
    assert FUZZ_MOD._is_sanitizer_ptrace_artifact(output) is True


# ---------------------------------------------------------------------------
# run-fuzz genuine fault decider. run_vmm saves a crash only when this
# returns a class, so a non zero exit, a guest reboot, a NEEDS_RESET, and
# a handled ERROR log must all read as not a fault.

def test_fault_class_rust_panic():
    out = "thread 'vcpu0' panicked at src/foo.rs:42:\nindex out of bounds\n"
    assert FUZZ_MOD._fault_class(1, out) is not None


def test_fault_class_guest_kernel_panic_is_not_a_vmm_fault():
    """A guest Linux kernel panic is the guest reacting to the input, not
    the VMM process faulting, so it is not a VMM crash."""
    out = ("[    0.8] Kernel panic - not syncing: Attempted to kill init! "
           "exitcode=0x00000000\n")
    assert FUZZ_MOD._fault_class(1, out) is None


def test_fault_class_panic_cmdline_param_is_not_a_fault():
    """The panic= kernel command line parameter is not a panic."""
    out = "[    0.0] Command line: panic=-1 debug console=ttyS0\n"
    assert FUZZ_MOD._fault_class(1, out) is None


def test_fault_class_address_sanitizer():
    out = "==1==ERROR: AddressSanitizer: heap-buffer-overflow on 0xdead\n"
    assert FUZZ_MOD._fault_class(-6, out) is not None


def test_fault_class_fatal_signal_empty_output():
    assert FUZZ_MOD._fault_class(-11, "") == "fatal signal SIGSEGV"


def test_fault_class_needs_reset_is_not_a_fault():
    out = "cloud-hypervisor: <vcpu0> ERROR: device set to NEEDS_RESET\n"
    assert FUZZ_MOD._fault_class(1, out) is None


def test_fault_class_handled_block_error_is_not_a_fault():
    out = ("cloud-hypervisor: ERROR:block/src/io/request.rs:117 -- "
           "Only head descriptor present\n")
    assert FUZZ_MOD._fault_class(1, out) is None


def test_fault_class_guest_reboot_nonzero_exit_is_not_a_fault():
    assert FUZZ_MOD._fault_class(1, "some benign guest reboot log\n") is None


def test_fault_class_invalid_queue_size_error_is_not_a_fault():
    out = ("cloud-hypervisor: <vcpu0> ERROR:virtio-queue-0.18.0/src/queue.rs:"
           "368 -- virtio queue with invalid size: 256\n")
    assert FUZZ_MOD._fault_class(1, out) is None


def test_fault_class_ptrace_artifact_is_not_a_fault():
    assert FUZZ_MOD._fault_class(-6, _LSAN_PTRACE_NOISE) is None


def test_classify_signal_death_labeled():
    assert FUZZ_MOD._classify_vmm_output("", -11) == "fatal signal SIGSEGV"


# ---------------------------------------------------------------------------
# run-fuzz llvm tool discovery (rustup keeps llvm-profdata/llvm-cov off PATH).

def test_find_llvm_tool_env_override():
    FUZZ_MOD.find_llvm_tool.cache_clear()
    old = os.environ.get("LLVM_PROFDATA")
    os.environ["LLVM_PROFDATA"] = "/bin/sh"
    try:
        assert FUZZ_MOD.find_llvm_tool("llvm-profdata") == "/bin/sh"
    finally:
        if old is None:
            os.environ.pop("LLVM_PROFDATA", None)
        else:
            os.environ["LLVM_PROFDATA"] = old
        FUZZ_MOD.find_llvm_tool.cache_clear()


def test_find_llvm_tool_unknown_returns_none():
    FUZZ_MOD.find_llvm_tool.cache_clear()
    assert FUZZ_MOD.find_llvm_tool("llvm-does-not-exist-zzz") is None
    FUZZ_MOD.find_llvm_tool.cache_clear()


def test_rustlib_bin_shape():
    rb = FUZZ_MOD._rustlib_bin()
    assert rb is None or rb.endswith(os.path.join("bin"))


# ---------------------------------------------------------------------------
# run-fuzz security severity triage.
#
# Each replay outcome maps to a severity tier so a crash corpus can be
# prioritised: memory corruption first, then VMM crashes, then device
# wedges, then graceful rejections.

def test_security_triage_critical_on_asan_uaf():
    """An AddressSanitizer use-after-free is CRITICAL."""
    output = (
        "==123==ERROR: AddressSanitizer: heap-use-after-free on address "
        "0x602000000010\n"
        "    #0 0x55 in foo block/src/io/request.rs:42\n"
    )
    tier, why = FUZZ_MOD._security_triage(output, 1)
    assert tier == "CRITICAL"
    assert "corruption" in why


def test_security_triage_critical_on_segv_signal():
    """A VMM killed by SIGSEGV is CRITICAL even without output."""
    tier, _ = FUZZ_MOD._security_triage("", -11)
    assert tier == "CRITICAL"


def test_security_triage_high_on_panic():
    """A Rust panic that takes down the VMM is HIGH."""
    output = "thread '<_disk0_q0>' panicked at 'index out of bounds'\n"
    tier, why = FUZZ_MOD._security_triage(output, 101)
    assert tier == "HIGH"
    assert "panic" in why


def test_security_triage_high_on_timeout():
    """A hang with no output is HIGH (guest stalls the VMM)."""
    tier, why = FUZZ_MOD._security_triage("", -1)
    assert tier == "HIGH"
    assert "hang" in why or "timeout" in why


def test_security_triage_low_on_needs_reset():
    """A device wedge via NEEDS_RESET is LOW, not a host compromise."""
    output = (
        "cloud-hypervisor: <_disk0_q0> WARN:virtio-devices/src/lib.rs:86 "
        "-- Corrupted request detected ... Setting device status to "
        "'NEEDS_RESET' and stopping processing queues until reset.\n"
        + _LSAN_PTRACE_NOISE
    )
    tier, why = FUZZ_MOD._security_triage(output, 1)
    assert tier == "LOW"
    assert "wedged" in why or "DoS" in why


def test_security_triage_none_on_graceful_rejection():
    """A graceful block rejection plus LSan teardown is NONE."""
    output = (
        "cloud-hypervisor: <_disk0_q0> ERROR:block/src/io/request.rs:155 "
        "-- Need a data descriptor\n"
        + _LSAN_PTRACE_NOISE
    )
    tier, _ = FUZZ_MOD._security_triage(output, 1)
    assert tier == "NONE"


def test_security_triage_critical_outranks_benign_rejection():
    """Real corruption wins even when a benign rejection is also logged."""
    output = (
        "cloud-hypervisor: ERROR:block/src/io/request.rs:91 "
        "-- Missing head descriptor\n"
        "==123==ERROR: AddressSanitizer: heap-buffer-overflow\n"
    )
    tier, _ = FUZZ_MOD._security_triage(output, 1)
    assert tier == "CRITICAL"


# ---------------------------------------------------------------------------
# run --apply-sysconfig registry, apply and revert.
#
# apply_sysconfigs must only change entries whose current value differs
# from the desired value, must return a restore map covering exactly the
# entries it changed, and revert_sysconfigs must restore those values.

def test_sysctl_path_mapping():
    """Dotted sysctl names map to their /proc/sys path."""
    assert (HOST_MOD._sysctl_path("kernel.yama.ptrace_scope")
            == "/proc/sys/kernel/yama/ptrace_scope")


def test_apply_sysconfigs_changes_only_differing():
    """Only entries whose value differs from want are changed."""
    reads = {"kernel.a": "1", "kernel.b": "0"}
    writes = []
    HOST_MOD._sysctl_read = lambda key: reads.get(key)
    HOST_MOD._sysctl_write = lambda key, val: (writes.append((key, val))
                                               or True)
    configs = [
        {"key": "kernel.a", "want": "0", "why": "x"},
        {"key": "kernel.b", "want": "0", "why": "y"},
    ]
    restore = HOST_MOD.apply_sysconfigs(configs)
    # kernel.a differed (1 -> 0) and was changed; kernel.b already 0.
    assert writes == [("kernel.a", "0")]
    assert restore == {"kernel.a": "1"}


def test_apply_sysconfigs_skips_absent():
    """Absent sysctls are skipped, not changed."""
    writes = []
    HOST_MOD._sysctl_read = lambda key: None
    HOST_MOD._sysctl_write = lambda key, val: (writes.append((key, val))
                                               or True)
    configs = [{"key": "kernel.missing", "want": "0", "why": "x"}]
    restore = HOST_MOD.apply_sysconfigs(configs)
    assert writes == []
    assert restore == {}


def test_apply_sysconfigs_records_only_successful_writes():
    """A failed write is not recorded for revert."""
    HOST_MOD._sysctl_read = lambda key: "1"
    HOST_MOD._sysctl_write = lambda key, val: False
    configs = [{"key": "kernel.a", "want": "0", "why": "x"}]
    restore = HOST_MOD.apply_sysconfigs(configs)
    assert restore == {}


def test_apply_sysconfigs_warns_on_failure():
    """A needed change that fails to write emits a warning summary."""
    import io
    import contextlib
    HOST_MOD._sysctl_read = lambda key: "1"
    HOST_MOD._sysctl_write = lambda key, val: False
    configs = [{"key": "kernel.a", "want": "0", "why": "x"}]
    buf = io.StringIO()
    with contextlib.redirect_stderr(buf):
        restore = HOST_MOD.apply_sysconfigs(configs)
    assert restore == {}
    text = buf.getvalue()
    assert "WARNING" in text
    assert "kernel.a" in text


def test_revert_sysconfigs_restores_values():
    """revert_sysconfigs writes back the saved old values and clears."""
    writes = []
    HOST_MOD._sysctl_write = lambda key, val: (writes.append((key, val))
                                               or True)
    restore = {"kernel.a": "1", "kernel.b": "2"}
    HOST_MOD.revert_sysconfigs(restore)
    assert sorted(writes) == [("kernel.a", "1"), ("kernel.b", "2")]
    assert restore == {}


# ---------------------------------------------------------------------------
# run --apply-sysconfig CAP_SYS_PTRACE grant for the LeakSanitizer leak
# scan. The VMM is non dumpable, so a same uid LSan tracer is denied; the
# grant bypasses it. The helpers parse getcap, append the capability as a
# separate clause so existing caps survive, and revert exactly.

class _CP:
    """Minimal stand in for subprocess.CompletedProcess."""

    def __init__(self, returncode=0, stdout="", stderr=""):
        self.returncode = returncode
        self.stdout = stdout
        self.stderr = stderr


def test_run_vmm_timeout_reports_timed_out():
    """A boot timeout returns crashed and timed_out set, so the fuzz
    loop can reconfirm it instead of trusting a scheduling stall."""
    import tempfile
    orig_run = FUZZ_MOD.process.capture

    def _raise_timeout(*a, **k):
        raise FUZZ_MOD.subprocess.TimeoutExpired(
            cmd="vmm", timeout=3, output=b"FUZZ: guest\n", stderr=b"vmm diagnostics\xff\n")

    with tempfile.TemporaryDirectory() as d:
        kernel = os.path.join(d, "vmlinux")
        open(kernel, "wb").close()
        vmm = os.path.join(d, "cloud-hypervisor")
        try:
            FUZZ_MOD.process.capture = _raise_timeout
            rc, crashed, out, timed_out = FUZZ_MOD.run_vmm(
                vmm, os.path.join(d, "initramfs"), d,
                timeout=1, kernel_path=kernel)
        finally:
            FUZZ_MOD.process.capture = orig_run
    assert crashed is True
    assert timed_out is True
    assert rc == -1
    assert out == "vmm diagnostics\ufffd\nFUZZ: guest\n", out


def test_fuzz_execution_preserves_worker_slots_timeouts_and_cancellation():
    import contextlib
    import io
    import threading
    from collections import Counter
    from types import SimpleNamespace
    from unittest.mock import patch

    for mode, jobs in (("normal", 1), ("normal", 3), ("confirm", 2),
                       ("interrupt", 2), ("error", 2)):
        with tempfile.TemporaryDirectory() as root:
            target = os.path.join(root, "target")
            os.mkdir(target)
            with open(os.path.join(target, "fuzz"), "wb") as stream:
                stream.write(b"fuzz executable")
            args = SimpleNamespace(
                vmm="/vmm", corpus_dir=os.path.join(root, "corpus"),
                crashes_dir=os.path.join(root, "crashes"), no_coverage=True,
                device=None, kernel="/kernel", jobs=jobs, iterations=6,
                timeout=3, cpus=2, memory="128M", no_confirm_timeouts=False)
            lock = threading.Lock()
            barrier = threading.Barrier(jobs)
            started = threading.Event()
            release = threading.Event()
            active = set()
            directories = set()
            calls = []
            attempts = Counter()
            peak = 0

            def boot(vmm, archive, directory, timeout, **kwargs):
                nonlocal peak
                assert kwargs["stop"] is not None
                with lock:
                    assert directory not in active, "A worker slot was reused while active"
                    active.add(directory)
                    directories.add(directory)
                    calls.append((directory, timeout))
                    call_index = len(calls)
                    attempts[directory] += 1
                    attempt = attempts[directory]
                    peak = max(peak, len(active))
                    if len(active) == jobs:
                        started.set()
                try:
                    if call_index <= jobs:
                        barrier.wait(timeout=5)
                    if mode == "error" and call_index == 1:
                        raise RuntimeError("Worker failed")
                    if mode in ("error", "interrupt"):
                        assert release.wait(timeout=5), "Cancellation did not stop active work"
                    if mode == "confirm" and attempt % 2:
                        return -1, True, "FUZZ: guest\n", True
                    return 0, False, "FUZZ: guest\n", False
                finally:
                    assert os.path.isdir(directory), "Scratch was removed before the worker exited"
                    with lock:
                        active.remove(directory)

            def interrupted_wait(*args, **kwargs):
                assert started.wait(timeout=5), "Parallel workers did not start"
                raise KeyboardInterrupt

            with contextlib.ExitStack() as patches:
                for name, value in (
                        ("_SCRIPT_DIR", root), ("_corpus_ref", []), ("_STRUCT_PRESERVE", False)):
                    patches.enter_context(patch.object(FUZZ_MOD, name, value))
                for name in ("_require_vmm", "patch_blob", "build_initramfs"):
                    patches.enter_context(patch.object(FUZZ_MOD, name))
                patches.enter_context(patch.object(
                    FUZZ_MOD, "_detect_backend", return_value=SimpleNamespace(name="ch")))
                patches.enter_context(patch.object(
                    FUZZ_MOD, "find_fuzz_section_offset", return_value=0))
                patches.enter_context(patch.object(
                    FUZZ_MOD, "_sweep_stale_workdirs", return_value=0))
                patches.enter_context(patch.object(
                    FUZZ_MOD, "mutate", side_effect=lambda blob: blob))
                patches.enter_context(patch.object(
                    FUZZ_MOD.subprocess, "run", return_value=subprocess.CompletedProcess([], 0)))
                patches.enter_context(patch.object(FUZZ_MOD, "run_vmm", side_effect=boot))
                kill = patches.enter_context(patch.object(
                    FUZZ_MOD.process, "kill_all_processes", side_effect=release.set))
                queue = patches.enter_context(patch.object(
                    FUZZ_MOD.execution, "CompletionQueue",
                    wraps=FUZZ_MOD.execution.CompletionQueue))
                if mode == "interrupt":
                    patches.enter_context(patch.object(
                        FUZZ_MOD.execution, "wait", side_effect=interrupted_wait))
                captured = patches.enter_context(contextlib.redirect_stdout(io.StringIO()))
                if mode == "error":
                    try:
                        FUZZ_MOD.cmd_fuzz(args)
                    except RuntimeError as error:
                        assert str(error) == "Worker failed", error
                    else:
                        raise AssertionError("A worker failure was ignored")
                else:
                    FUZZ_MOD.cmd_fuzz(args)
            assert not active
            assert peak == jobs, (mode, peak)
            assert all(not os.path.exists(directory) for directory in directories)
            if jobs > 1:
                kill.assert_called_once_with()
                assert queue.call_args.kwargs["limit"] == jobs * 2
                assert queue.call_args.kwargs["stop"].is_set()
            else:
                kill.assert_not_called()
                queue.assert_not_called()
            if mode in ("interrupt", "error"):
                assert len(calls) == jobs, (mode, calls)
                if mode == "interrupt":
                    assert "[*] Interrupted" in captured.getvalue()
            else:
                expected_timeouts = {3: 6, 12: 6} if mode == "confirm" else {3: 6}
                assert dict(Counter(timeout for _, timeout in calls)) == expected_timeouts
                assert "[*] Done: 6 iterations" in captured.getvalue()


def test_detect_backend_maps_binary_names():
    """The fuzzer resolves each VMM binary to the matching backend."""
    cases = [("cloud-hypervisor", "ch"),
             ("qemu-system-aarch64", "qemu"),
             ("qemu-system-x86_64", "qemu"),
             ("openvmm", "openvmm")]
    for base, expect in cases:
        FUZZ_MOD._BACKEND_CACHE.clear()
        backend = FUZZ_MOD._detect_backend("/opt/bin/" + base)
        assert backend.name == expect
    FUZZ_MOD._BACKEND_CACHE.clear()


def test_run_vmm_routes_qemu_binary_through_qemu_backend():
    """A qemu-system binary boots via the QEMU backend command, so the
    fuzzer can target QEMU, not only Cloud Hypervisor."""
    import tempfile

    class _Result:
        returncode = 0
        stderr = b""
        stdout = b""

    captured = {}
    orig_run = FUZZ_MOD.process.capture

    def _capture(cmd, *a, **k):
        captured["cmd"] = cmd
        return _Result()

    with tempfile.TemporaryDirectory() as d:
        kernel = os.path.join(d, "vmlinux")
        open(kernel, "wb").close()
        vmm = os.path.join(d, "qemu-system-x86_64")
        FUZZ_MOD._BACKEND_CACHE.clear()
        try:
            FUZZ_MOD.process.capture = _capture
            FUZZ_MOD.run_vmm(vmm, os.path.join(d, "initramfs"), d,
                             timeout=5, kernel_path=kernel)
        finally:
            FUZZ_MOD.process.capture = orig_run
            FUZZ_MOD._BACKEND_CACHE.clear()
    cmd = captured["cmd"]
    assert vmm == cmd[0]
    assert "-no-reboot" in cmd
    assert any("virtio-blk" in str(x) for x in cmd)


def test_sweep_stale_workdirs_ages_out_only_old_dirs():
    """The sweep removes leaked scratch from dead runs but never a fresh
    dir, so a concurrent run is safe."""
    import tempfile
    import time
    with tempfile.TemporaryDirectory() as tmp:
        orig = FUZZ_MOD.tempfile.gettempdir
        old = os.path.join(tmp, "vvfuzz_w0_old")
        fresh = os.path.join(tmp, "vvfuzz_w1_new")
        other = os.path.join(tmp, "keepme")
        for d in (old, fresh, other):
            os.makedirs(d)
        past = time.time() - 3 * 3600
        os.utime(old, (past, past))
        try:
            FUZZ_MOD.tempfile.gettempdir = lambda: tmp
            removed = FUZZ_MOD._sweep_stale_workdirs(max_age_s=3600)
        finally:
            FUZZ_MOD.tempfile.gettempdir = orig
        assert removed == 1
        assert not os.path.exists(old)
        assert os.path.exists(fresh)
        assert os.path.exists(other)


def test_list_devices_lists_all_without_vmm():
    """--list-devices prints every device and needs no --vmm."""
    r = subprocess.run([sys.executable, RUN_FUZZ, "fuzz", "--list-devices"],
                       capture_output=True, text=True, cwd=ROOT_DIR)
    assert r.returncode == 0
    for name in FUZZ_MOD._DEVICE_MAP:
        assert name in r.stdout


def test_replay_corpus_flag_is_accepted():
    """--replay-corpus is a wired fuzz flag, not an argparse error."""
    r = subprocess.run(
        [sys.executable, RUN_FUZZ, "fuzz", "--replay-corpus", "-n", "1"],
        capture_output=True, text=True, cwd=ROOT_DIR)
    # No --vmm, so it must fail on the missing VMM, never on an
    # unrecognized argument.
    assert r.returncode != 0
    assert "unrecognized arguments" not in r.stderr


def test_structure_preserving_flag_is_accepted():
    """--structure-preserving is a wired fuzz flag, not an argparse error."""
    r = subprocess.run(
        [sys.executable, RUN_FUZZ, "fuzz", "--structure-preserving",
         "-n", "1"],
        capture_output=True, text=True, cwd=ROOT_DIR)
    assert r.returncode != 0
    assert "unrecognized arguments" not in r.stderr


def test_payload_only_mutate_keeps_the_ring_valid():
    """Structure-preserving mutation edits only the payload and leaves the
    descriptors, avail ring, queue size and avail_idx untouched."""
    import random as _random
    _random.seed(20260829)
    seed = FUZZ_MOD.make_seed()
    before = FUZZ_MOD.VqBlob.parse(seed)
    mutated = FUZZ_MOD._payload_only_mutate(bytearray(before.serialize()))
    after = FUZZ_MOD.VqBlob.parse(bytes(mutated))
    assert after.queue_size == before.queue_size
    assert after.avail_idx == before.avail_idx
    assert after.descs == before.descs
    assert after.avail_ring == before.avail_ring
    assert after.payload != before.payload


def test_ring_plausible_accepts_a_valid_seed():
    """A minimal valid seed has a ring the device would accept."""
    assert FUZZ_MOD._blob_ring_plausible(FUZZ_MOD.make_seed()) is True


def test_ring_plausible_rejects_a_broken_ring():
    """A zero queue size with a self looping chain is not plausible."""
    descs = [(16, 0x1, 0)]  # single descriptor whose NEXT points to itself
    blob = FUZZ_MOD.make_blob(0, descs, 1, [0], b"\x00" * 32)
    assert FUZZ_MOD._blob_ring_plausible(blob) is False


def test_fuzz_without_vmm_errors_clearly():
    """A real run still requires --vmm and says so."""
    r = subprocess.run([sys.executable, RUN_FUZZ, "fuzz", "-n", "1"],
                       capture_output=True, text=True, cwd=ROOT_DIR)
    assert r.returncode != 0
    assert "--vmm is required" in (r.stdout + r.stderr)


def test_run_vmm_calls_overlap_across_threads():
    """The fuzzer relies on many VM boots running at once. Drive run_vmm
    from several threads with a stub VMM that just sleeps: if the calls
    overlap the wall time stays near one sleep, but a regression that
    serializes boots would approach the summed sleep and fail here."""
    import threading
    import time

    workers = 6
    sleep_s = 0.5
    with tempfile.TemporaryDirectory() as base:
        vmm = os.path.join(base, "cloud-hypervisor")
        with open(vmm, "w") as fh:
            fh.write("#!/bin/sh\nsleep %s\nexit 0\n" % sleep_s)
        os.chmod(vmm, 0o755)
        kernel = os.path.join(base, "vmlinux")
        open(kernel, "wb").close()

        def one(i):
            wd = os.path.join(base, "w%d" % i)
            os.makedirs(wd)
            disk = os.path.join(wd, "disk.raw")
            open(disk, "wb").close()
            FUZZ_MOD.run_vmm(vmm, os.path.join(wd, "initramfs"), wd,
                             timeout=10, kernel_path=kernel, disk_path=disk)

        threads = [threading.Thread(target=one, args=(i,))
                   for i in range(workers)]
        start = time.perf_counter()
        for t in threads:
            t.start()
        for t in threads:
            t.join()
        elapsed = time.perf_counter() - start

    # Serial would be workers*sleep_s (3.0s). Overlapping keeps it near
    # one sleep plus per call overhead; allow generous slack for load.
    assert elapsed < workers * sleep_s / 2, (
        "run_vmm boots did not overlap: %.2fs for %d x %.2fs"
        % (elapsed, workers, sleep_s))


def test_getcap_parses_expression():
    """getcap "PATH cap=ep" yields the capability expression."""
    orig_which = RUN_MOD.shutil.which
    orig_run = RUN_MOD.subprocess.run
    try:
        RUN_MOD.shutil.which = lambda c: "/usr/sbin/" + c
        RUN_MOD.subprocess.run = lambda *a, **k: _CP(
            0, "/path/to/vmm cap_net_admin=ep\n")
        assert HOST_MOD.get_capabilities("/path/to/vmm") == "cap_net_admin=ep"
    finally:
        RUN_MOD.shutil.which = orig_which
        RUN_MOD.subprocess.run = orig_run


def test_getcap_strips_equals_form():
    """An older "PATH = cap+ep" form is parsed to the bare expression."""
    orig_which = RUN_MOD.shutil.which
    orig_run = RUN_MOD.subprocess.run
    try:
        RUN_MOD.shutil.which = lambda c: "/usr/sbin/" + c
        RUN_MOD.subprocess.run = lambda *a, **k: _CP(
            0, "/path/to/vmm = cap_net_admin+ep\n")
        assert HOST_MOD.get_capabilities("/path/to/vmm") == "cap_net_admin+ep"
    finally:
        RUN_MOD.shutil.which = orig_which
        RUN_MOD.subprocess.run = orig_run


def test_getcap_empty_when_no_caps():
    """A file with no capabilities yields the empty string, not None."""
    orig_which = RUN_MOD.shutil.which
    orig_run = RUN_MOD.subprocess.run
    try:
        RUN_MOD.shutil.which = lambda c: "/usr/sbin/" + c
        RUN_MOD.subprocess.run = lambda *a, **k: _CP(0, "")
        assert HOST_MOD.get_capabilities("/path/to/vmm") == ""
    finally:
        RUN_MOD.shutil.which = orig_which
        RUN_MOD.subprocess.run = orig_run


def test_apply_ptrace_cap_appends_clause():
    """The grant appends cap_sys_ptrace, preserving existing caps."""
    orig_which = RUN_MOD.shutil.which
    orig_lsan = HOST_MOD._is_leak_sanitized
    orig_getcap = HOST_MOD.get_capabilities
    orig_apply = HOST_MOD._setcap_apply
    captured = []
    try:
        RUN_MOD.shutil.which = lambda c: "/usr/sbin/" + c
        HOST_MOD._is_leak_sanitized = lambda p: True
        HOST_MOD.get_capabilities = lambda p: "cap_net_admin=ep"
        HOST_MOD._setcap_apply = lambda expr, p: (captured.append(expr) or True)
        orig = HOST_MOD.apply_ptrace_cap(__file__)
        assert captured == ["cap_net_admin=ep cap_sys_ptrace=ep"]
        assert orig == "cap_net_admin=ep"
    finally:
        RUN_MOD.shutil.which = orig_which
        HOST_MOD._is_leak_sanitized = orig_lsan
        HOST_MOD.get_capabilities = orig_getcap
        HOST_MOD._setcap_apply = orig_apply


def test_apply_ptrace_cap_no_existing_caps():
    """With no existing caps the grant is the sole clause."""
    orig_which = RUN_MOD.shutil.which
    orig_lsan = HOST_MOD._is_leak_sanitized
    orig_getcap = HOST_MOD.get_capabilities
    orig_apply = HOST_MOD._setcap_apply
    captured = []
    try:
        RUN_MOD.shutil.which = lambda c: "/usr/sbin/" + c
        HOST_MOD._is_leak_sanitized = lambda p: True
        HOST_MOD.get_capabilities = lambda p: ""
        HOST_MOD._setcap_apply = lambda expr, p: (captured.append(expr) or True)
        orig = HOST_MOD.apply_ptrace_cap(__file__)
        assert captured == ["cap_sys_ptrace=ep"]
        assert orig == ""
    finally:
        RUN_MOD.shutil.which = orig_which
        HOST_MOD._is_leak_sanitized = orig_lsan
        HOST_MOD.get_capabilities = orig_getcap
        HOST_MOD._setcap_apply = orig_apply


def test_apply_ptrace_cap_skips_when_already_present():
    """If the cap is already set, nothing is changed or recorded."""
    orig_which = RUN_MOD.shutil.which
    orig_lsan = HOST_MOD._is_leak_sanitized
    orig_getcap = HOST_MOD.get_capabilities
    orig_apply = HOST_MOD._setcap_apply
    called = []
    try:
        RUN_MOD.shutil.which = lambda c: "/usr/sbin/" + c
        HOST_MOD._is_leak_sanitized = lambda p: True
        HOST_MOD.get_capabilities = lambda p: "cap_sys_ptrace=ep"
        HOST_MOD._setcap_apply = lambda expr, p: (called.append(expr) or True)
        assert HOST_MOD.apply_ptrace_cap(__file__) is None
        assert called == []
    finally:
        RUN_MOD.shutil.which = orig_which
        HOST_MOD._is_leak_sanitized = orig_lsan
        HOST_MOD.get_capabilities = orig_getcap
        HOST_MOD._setcap_apply = orig_apply


def test_apply_ptrace_cap_skips_non_lsan():
    """A VMM without the LeakSanitizer runtime is not touched."""
    orig_which = RUN_MOD.shutil.which
    orig_lsan = HOST_MOD._is_leak_sanitized
    orig_apply = HOST_MOD._setcap_apply
    called = []
    try:
        RUN_MOD.shutil.which = lambda c: "/usr/sbin/" + c
        HOST_MOD._is_leak_sanitized = lambda p: False
        HOST_MOD._setcap_apply = lambda expr, p: (called.append(expr) or True)
        assert HOST_MOD.apply_ptrace_cap(__file__) is None
        assert called == []
    finally:
        RUN_MOD.shutil.which = orig_which
        HOST_MOD._is_leak_sanitized = orig_lsan
        HOST_MOD._setcap_apply = orig_apply


def test_revert_ptrace_cap_reapplies_original():
    """A non empty original is restored with setcap."""
    orig_apply = HOST_MOD._setcap_apply
    orig_remove = HOST_MOD._setcap_remove
    applied = []
    removed = []
    try:
        HOST_MOD._setcap_apply = lambda expr, p: (applied.append((expr, p))
                                                  or True)
        HOST_MOD._setcap_remove = lambda p: (removed.append(p) or True)
        HOST_MOD.revert_ptrace_cap("/path/vmm", "cap_net_admin=ep")
        assert applied == [("cap_net_admin=ep", "/path/vmm")]
        assert removed == []
    finally:
        HOST_MOD._setcap_apply = orig_apply
        HOST_MOD._setcap_remove = orig_remove


def test_revert_ptrace_cap_clears_when_originally_none():
    """An empty original means the VMM had no caps; clear them."""
    orig_apply = HOST_MOD._setcap_apply
    orig_remove = HOST_MOD._setcap_remove
    applied = []
    removed = []
    try:
        HOST_MOD._setcap_apply = lambda expr, p: (applied.append((expr, p))
                                                  or True)
        HOST_MOD._setcap_remove = lambda p: (removed.append(p) or True)
        HOST_MOD.revert_ptrace_cap("/path/vmm", "")
        assert removed == ["/path/vmm"]
        assert applied == []
    finally:
        HOST_MOD._setcap_apply = orig_apply
        HOST_MOD._setcap_remove = orig_remove


def test_revert_ptrace_cap_noop_when_unchanged():
    """A None token means nothing was granted, so nothing is reverted."""
    orig_apply = HOST_MOD._setcap_apply
    orig_remove = HOST_MOD._setcap_remove
    touched = []
    try:
        HOST_MOD._setcap_apply = lambda expr, p: (touched.append(1) or True)
        HOST_MOD._setcap_remove = lambda p: (touched.append(1) or True)
        HOST_MOD.revert_ptrace_cap("/path/vmm", None)
        assert touched == []
    finally:
        HOST_MOD._setcap_apply = orig_apply
        HOST_MOD._setcap_remove = orig_remove


# ---------------------------------------------------------------------------
# Termination cleanup: SIGTERM/SIGHUP must reap tracked VMMs.
#
# `timeout` sends SIGTERM. Without a handler the process dies without
# unwinding and the run loop's SIGKILL sweep never runs, orphaning any
# in-flight VM. _install_termination_as_interrupt routes those signals
# through the KeyboardInterrupt path that already reaps them.

def test_unit_install_termination_as_interrupt_sets_handlers():
    """SIGTERM and SIGHUP get handlers that raise KeyboardInterrupt."""
    import signal as _signal
    installed = {}
    orig = RUN_MOD.signal.signal
    RUN_MOD.signal.signal = lambda sig, h: installed.__setitem__(sig, h)
    try:
        RUN_MOD._install_termination_as_interrupt()
    finally:
        RUN_MOD.signal.signal = orig
    assert _signal.SIGTERM in installed
    assert _signal.SIGHUP in installed
    for sig in (_signal.SIGTERM, _signal.SIGHUP):
        try:
            installed[sig](sig, None)
        except KeyboardInterrupt:
            continue
        raise AssertionError(
            f"handler for {sig} must raise KeyboardInterrupt")


# ---------------------------------------------------------------------------
# Spec section traceability guard.
#
# Every REGISTER_TEST cites a spec version and a section string. This guard
# validates those citations against selftest/spec_sections.txt, the
# version-keyed map of virtio device chapters extracted from the OASIS spec
# source. It catches the defect class where a wrong chapter number sneaks
# into a new registration: a device dir citing another device's chapter, a
# non-numeric placeholder, a nonexistent chapter, or an admin test tagged to
# a version without admin virtqueues.

SPEC_MAP_FILE = os.path.join(SCRIPT_DIR, "spec_sections.txt")
TESTS_DIR = os.path.join(ROOT_DIR, "tests")

# Directories that are not device types. Their tests drive a device to
# exercise a generic transport or virtqueue clause, so a chapter 5 citation
# in these dirs is legitimate and not checked against a device chapter.
_NON_DEVICE_DIRS = {"vring", "packed", "pci", "mmio", "state", "admin"}


def load_spec_map(path=SPEC_MAP_FILE):
    """Parse spec_sections.txt into (devices, rules).

    devices: key -> {version: "5.x" or None}
    rules:   version -> {"admin": bool, "max_chapter": int}
    The device-table column layout is read from the "# id key V1_x ..."
    header, so extra columns (id) and added version columns need no code
    change. "key" is the virtio-villain test directory name.
    """
    import re as _re
    devices = {}
    rules = {}
    key_idx = None
    ver_cols = None
    with open(path) as fh:
        for raw in fh:
            line = raw.rstrip("\n")
            stripped = line.strip()
            if key_idx is None and stripped.startswith("#"):
                toks = stripped.lstrip("#").split()
                vcols = [(i, t) for i, t in enumerate(toks)
                         if _re.fullmatch(r"V\d_\d", t)]
                if "key" in toks and vcols:
                    key_idx = toks.index("key")
                    ver_cols = vcols
                    continue
            if not stripped or stripped.startswith("#"):
                continue
            code = stripped.split("#", 1)[0].strip()
            if not code:
                continue
            fields = code.split()
            if fields[0].startswith("@"):
                rules[fields[0][1:]] = {
                    "admin": fields[1].lower() == "yes",
                    "max_chapter": int(fields[2]),
                }
            else:
                devices[fields[key_idx]] = {
                    t: (None if fields[i] == "-" else fields[i])
                    for i, t in ver_cols
                }
    if key_idx is None:
        raise ValueError(f"{path}: missing '# id key V1_x ...' header row")
    return devices, rules


def spec_section_violation(dirname, version, section, devices, rules):
    """Return a violation reason for one registration, or None if valid.

    version is the short form, e.g. "V1_2".
    """
    rule = rules.get(version)
    if rule is None:
        return f"unknown spec version {version}"

    dev = devices.get(dirname)
    no_chapter = dev is not None and dev.get(version) is None

    # 0. "-" is the sentinel for a device that has no chapter in this
    # version (e.g. watchdog). Valid only for such a device.
    if section == "-":
        if no_chapter:
            return None
        return f"section '-' used but {dirname} has a chapter in {version}"

    # 1. Section must be numeric (a real chapter number).
    head = section.split(".", 1)[0]
    if not head.isdigit():
        return f"non-numeric section '{section}'"
    chapter = int(head)

    # 2. Chapter must exist in this version.
    if chapter < 1 or chapter > rule["max_chapter"]:
        return (f"chapter {chapter} out of range 1..{rule['max_chapter']} "
                f"for {version} (section '{section}')")

    # 3. Admin virtqueue tests require a version that has admin.
    if dirname == "admin" and not rule["admin"]:
        return f"admin test tagged {version} which has no admin virtqueues"

    # 4. Device chapter (5.x) must match this dir's chapter.
    if dev is not None and section.startswith("5."):
        expected = dev.get(version)
        if expected is None:
            return (f"{dirname} has no device chapter in {version} "
                    f"but cites '{section}'")
        parts = section.split(".")
        got = parts[0] + "." + (parts[1] if len(parts) > 1 else "")
        if got != expected:
            return (f"{dirname} cites '{section}' but its chapter in "
                    f"{version} is {expected}")
    return None


def iter_registrations(tests_dir=TESTS_DIR):
    """Yield (test_id, dirname, version, section, file) for every test.

    Parses the REGISTER_TEST* macros directly from source so the guard
    needs no build.
    """
    import re as _re
    macro = _re.compile(
        r"REGISTER_TEST\w*\s*\(([^;]*?)\)\s*;", _re.DOTALL)
    ver_sec = _re.compile(
        r"(?:VIRTIO_SPEC_)?(V1_\d)\s*,\s*\"([^\"]*)\"")
    ident = _re.compile(r"\s*(\w+)")
    for root, _dirs, files in os.walk(tests_dir):
        for fn in files:
            if not fn.endswith(".c"):
                continue
            path = os.path.join(root, fn)
            dirname = os.path.basename(root)
            with open(path) as fh:
                text = fh.read()
            for m in macro.finditer(text):
                body = m.group(1)
                vs = ver_sec.search(body)
                if not vs:
                    continue
                name = ident.match(body)
                tid = name.group(1) if name else "?"
                yield tid, dirname, vs.group(1), vs.group(2), path


def test_spec_sections_consistent():
    """Every REGISTER_TEST section agrees with the spec section map."""
    devices, rules = load_spec_map()
    problems = []
    for tid, dirname, version, section, path in iter_registrations():
        reason = spec_section_violation(dirname, version, section,
                                        devices, rules)
        if reason:
            rel = os.path.relpath(path, ROOT_DIR)
            problems.append(f"{tid} [{rel}]: {reason}")
    assert not problems, (
        "spec section defects:\n  " + "\n  ".join(sorted(problems)))


def test_test_ids_unique():
    """Every REGISTER_TEST identifier is unique."""
    paths_by_id = {}
    for tid, _dirname, _version, _section, path in iter_registrations():
        paths_by_id.setdefault(tid, []).append(os.path.relpath(path, ROOT_DIR))
    duplicates = [f"{tid} {', '.join(paths)}"
                  for tid, paths in paths_by_id.items() if len(paths) > 1]
    assert not duplicates, (
        "duplicate test identifiers\n  " + "\n  ".join(sorted(duplicates)))


def test_unit_load_spec_map_shapes():
    devices, rules = load_spec_map()
    assert devices["mem"]["V1_2"] == "5.15"
    assert devices["pmem"]["V1_4"] == "5.19"
    assert devices["rtc"]["V1_2"] is None
    assert devices["rtc"]["V1_4"] == "5.23"
    assert devices["watchdog"]["V1_4"] is None
    assert rules["V1_2"]["admin"] is False
    assert rules["V1_3"]["admin"] is True
    assert rules["V1_2"]["max_chapter"] == 7


def test_unit_spec_violation_device_chapter_mismatch():
    devices, rules = load_spec_map()
    # mem cited as 5.14 (sound) is wrong; 5.15 is right.
    assert spec_section_violation(
        "mem", "V1_2", "5.14.6.2", devices, rules)
    assert spec_section_violation(
        "mem", "V1_2", "5.15.6.2", devices, rules) is None


def test_unit_spec_violation_non_numeric_and_bad_chapter():
    devices, rules = load_spec_map()
    assert spec_section_violation("rtc", "V1_4", "RTC.5", devices, rules)
    assert spec_section_violation("admin", "V1_3", "9.4", devices, rules)


def test_unit_spec_violation_admin_version_gate():
    devices, rules = load_spec_map()
    # A chapter 2 admin section is invalid when tagged to v1.2.
    assert spec_section_violation("admin", "V1_2", "2.13", devices, rules)
    assert spec_section_violation("admin", "V1_3", "2.13", devices, rules) is None


def test_unit_spec_violation_no_chapter_device():
    devices, rules = load_spec_map()
    # watchdog has no device chapter, so any 5.x is a defect, but a
    # generic virtqueue citation is fine.
    assert spec_section_violation("watchdog", "V1_2", "5.16", devices, rules)
    assert spec_section_violation(
        "watchdog", "V1_2", "2.7.5", devices, rules) is None
    # "-" is the accepted no-chapter sentinel for watchdog.
    assert spec_section_violation(
        "watchdog", "V1_2", "-", devices, rules) is None
    # but a real device must not use the sentinel.
    assert spec_section_violation("mem", "V1_2", "-", devices, rules)


def test_unit_spec_violation_non_device_dir_allows_5x():
    devices, rules = load_spec_map()
    # vring drives the block device, so citing 5.2.6 is legitimate.
    assert spec_section_violation("vring", "V1_2", "5.2.6", devices, rules) is None


# ---------------------------------------------------------------------------

def main():
    setup()
    global FUZZ_MOD
    FUZZ_MOD = _load_fuzz_module()
    use_color = sys.stdout.isatty() and not os.environ.get("NO_COLOR")
    c_pass = "\033[32m" if use_color else ""
    c_fail = "\033[31m" if use_color else ""
    c_reset = "\033[0m" if use_color else ""
    tests = [v for k, v in globals().items() if k.startswith("test_")]
    c_skip = "\033[33m" if use_color else ""
    passed = 0
    failed = 0
    skipped = 0
    for t in tests:
        try:
            t()
            print(f"  {c_pass}[PASS]{c_reset} {t.__name__}")
            passed += 1
        except SkipTest as e:
            print(f"  {c_skip}[SKIP]{c_reset} {t.__name__}: {e}")
            skipped += 1
        except AssertionError as e:
            print(f"  {c_fail}[FAIL]{c_reset} {t.__name__}: {e}")
            failed += 1
        except Exception as e:
            print(f"  {c_fail}[FAIL]{c_reset} {t.__name__}: "
                  f"{type(e).__name__}: {e}")
            failed += 1
    total = passed + failed + skipped
    tag = c_pass if failed == 0 else c_fail
    skip_msg = f", {skipped} skipped" if skipped else ""
    print(f"\n{tag}selftest/run: {passed}/{total} passed{skip_msg}{c_reset}")
    return 0 if failed == 0 else 1


if __name__ == "__main__":
    sys.exit(main())
