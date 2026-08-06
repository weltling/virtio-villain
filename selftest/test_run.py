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
INIT_BINARY = os.path.join(ROOT_DIR, "target", "init")
RUN = os.path.join(ROOT_DIR, "run")
RUN_FUZZ = os.path.join(ROOT_DIR, "run-fuzz")
MOCK_VMM = os.path.join(SCRIPT_DIR, "mock-vmm")


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
    """Create symlink, dummy initramfs, and load run.py module."""
    global RUN_MOD
    if not os.path.exists(MOCK_LINK):
        os.symlink("mock-vmm", MOCK_LINK)
    initramfs = os.path.join(ROOT_DIR, "target", "initramfs.cpio.gz")
    os.makedirs(os.path.dirname(initramfs), exist_ok=True)
    if not os.path.exists(initramfs):
        open(initramfs, "w").close()
    RUN_MOD = _load_run_module()


def run_runner(*args):
    """Run the test runner with mock VMM."""
    cmd = [
        sys.executable, RUN,
        "--vmm", MOCK_LINK,
        "--kernel", "/dev/null",
        "--timeout", "5",
    ] + list(args)
    env = dict(os.environ)
    env["VV_ALLOW_UNKNOWN_TESTS"] = "1"
    result = subprocess.run(cmd, capture_output=True, text=True,
                            cwd=ROOT_DIR, env=env)
    return result


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


def test_unit_qemu_net_device_disables_option_rom():
    be = RUN_MOD.Qemu("/usr/bin/qemu-system-x86_64")
    cmd = be.build_cmd("/k", "/i", "/d.raw", "console=ttyS0", {})
    net_dev = next(arg for arg in cmd
                   if arg.startswith("virtio-net-pci-non-transitional,"))
    assert net_dev.endswith(",romfile=")


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
    assert isinstance(be, RUN_MOD.OpenVmm)
    assert be.name == "openvmm"


def test_unit_openvmm_build_cmd_direct_boot_layout():
    """Every virtio device is placed on its own named PCIe root port."""
    be = RUN_MOD.OpenVmm("/opt/openvmm/openvmm")
    cmd = be.build_cmd("/k", "/i", "/d.raw", "console=ttyS0 vv.test=T0001",
                       {"cpus": 2, "memory": "256M"})
    assert _openvmm_opt(cmd, "-k") == "/k"
    assert _openvmm_opt(cmd, "-r") == "/i"
    assert _openvmm_opt(cmd, "-c") == "console=ttyS0 vv.test=T0001"
    assert _openvmm_opt(cmd, "-m") == "256M"
    assert _openvmm_opt(cmd, "-p") == "2"
    assert _openvmm_opt(cmd, "--pcie-root-complex") == "rc0"
    # Runner output rides on COM1, not the virtio-console device.
    assert _openvmm_opt(cmd, "--com1") == "console"
    # The VM must run in one process so the runner's kill reaps it.
    assert "--single-process" in cmd
    # The disk, net, console and rng ports are always declared.
    assert set(_openvmm_ports(cmd)) >= {"disk", "net", "console", "rng"}


def test_unit_openvmm_build_cmd_blk_on_pcie_port():
    be = RUN_MOD.OpenVmm("/opt/openvmm/openvmm")
    cmd = be.build_cmd("/k", "/i", "/d.raw", "c", {})
    blk = _openvmm_opt(cmd, "--virtio-blk")
    assert blk == "file:/d.raw,pcie_port=disk"


def test_unit_openvmm_build_cmd_blk_direct():
    be = RUN_MOD.OpenVmm("/opt/openvmm/openvmm")
    cmd = be.build_cmd("/k", "/i", "/d.raw", "c", {"direct": True})
    assert _openvmm_opt(cmd, "--virtio-blk") == "file:/d.raw;direct,pcie_port=disk"


def test_unit_openvmm_build_cmd_blk_aio_forces_direct():
    be = RUN_MOD.OpenVmm("/opt/openvmm/openvmm")
    cmd = be.build_cmd("/k", "/i", "/d.raw", "c", {"io_engine": "aio"})
    assert ";direct" in _openvmm_opt(cmd, "--virtio-blk")


def test_unit_openvmm_build_cmd_net_consomme():
    be = RUN_MOD.OpenVmm("/opt/openvmm/openvmm")
    cmd = be.build_cmd("/k", "/i", "/d.raw", "c", {})
    assert _openvmm_opt(cmd, "--virtio-net") == "pcie_port=net:consomme"


def test_unit_openvmm_build_cmd_no_vsock():
    """virtio-vsock has no PCIe port option, so it is never offered."""
    be = RUN_MOD.OpenVmm("/opt/openvmm/openvmm")
    cmd = be.build_cmd("/k", "/i", "/d.raw", "c", {})
    assert not any("vsock" in a for a in cmd)


def test_unit_openvmm_build_cmd_pmem_optional():
    be = RUN_MOD.OpenVmm("/opt/openvmm/openvmm")
    without = be.build_cmd("/k", "/i", "/d.raw", "c", {})
    assert "--virtio-pmem" not in without
    assert "pmem" not in _openvmm_ports(without)
    with_pmem = be.build_cmd("/k", "/i", "/d.raw", "c",
                             {"pmem_path": "/d.pmem"})
    assert _openvmm_opt(with_pmem, "--virtio-pmem") == "pcie_port=pmem:/d.pmem"
    assert "pmem" in _openvmm_ports(with_pmem)


def test_unit_openvmm_build_cmd_fs_optional():
    be = RUN_MOD.OpenVmm("/opt/openvmm/openvmm")
    without = be.build_cmd("/k", "/i", "/d.raw", "c", {})
    assert "--vhost-user" not in without
    with_fs = be.build_cmd("/k", "/i", "/d.raw", "c",
                           {"fs_socket": "/d.fsd"})
    assert _openvmm_opt(with_fs, "--vhost-user") == \
        "/d.fsd,type=fs,tag=vvfs,pcie_port=fs"
    assert "fs" in _openvmm_ports(with_fs)


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
            c.sendall(b'{"return":{}}\n')
            buf = b""
            while b"\n" not in buf:
                chunk = c.recv(4096)
                if not chunk:
                    break
                buf += chunk
            captured["cmd"] = buf.split(b"\n", 1)[0]
            c.sendall(b'{"return":{"status":"running"}}\n')
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
    opts = {"memory": "256M"}
    cmd = be.build_cmd("/k", "/i", "/d.raw", "console=ttyS0", opts)
    assert "virtio-blk-device,drive=vd0" in cmd
    assert not any("virtio-blk-pci" in str(a) for a in cmd)


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
    blob = FUZZ_MOD.make_seed()
    vq = FUZZ_MOD.VqBlob.parse(blob)
    again = FUZZ_MOD.VqBlob.parse(vq.serialize())
    assert again.queue_size == vq.queue_size
    assert again.avail_idx == vq.avail_idx
    assert again.descs == vq.descs
    assert again.avail_ring == vq.avail_ring


def test_fuzz_vmm_command_provisions_auxiliary_devices():
    """Fuzz commands include every device targeted by the guest format."""
    cmd = FUZZ_MOD._fuzz_vmm_cmd(
        "/vmm", "/kernel", "/initramfs", "/disk", "/pmem", "/work",
        1, "128M")
    assert ["--disk", "path=/disk,num_queues=1"] == cmd[5:7]
    assert ["--net", "tap=,num_queues=2"] == cmd[7:9]
    assert "--vsock" in cmd
    assert ["--balloon", "size=0"] == cmd[11:13]
    assert "--watchdog" in cmd
    assert ["--rng", "src=/dev/urandom"] == cmd[14:16]
    assert ["--console", "null"] == cmd[16:18]
    assert ["--pmem", "file=/pmem,size=128M,discard_writes=on"] == cmd[18:20]
    assert "hotplug_method=virtio-mem" in cmd[23]


def test_fuzz_vmm_command_assigns_unique_vsock_cids():
    """Parallel fuzz VMs must not share a vsock CID."""
    first = FUZZ_MOD._fuzz_vmm_cmd(
        "/vmm", "/kernel", "/initramfs", "/disk", "/pmem", "/work",
        1, "128M")
    second = FUZZ_MOD._fuzz_vmm_cmd(
        "/vmm", "/kernel", "/initramfs", "/disk", "/pmem", "/work",
        1, "128M")
    assert first[10] != second[10]


def test_vqblob_serialize_is_blob_size():
    """serialize always emits exactly BLOB_SIZE bytes."""
    vq = FUZZ_MOD.VqBlob.parse(FUZZ_MOD.make_seed())
    assert len(vq.serialize()) == FUZZ_MOD.BLOB_SIZE


def test_vqblob_parse_tolerates_short_blob():
    """A blob shorter than the header parses without raising."""
    vq = FUZZ_MOD.VqBlob.parse(b"\x10\x00")
    assert vq.descs == []
    assert len(vq.serialize()) == FUZZ_MOD.BLOB_SIZE


def test_op_make_chain_links_all_but_last():
    """make_chain sets NEXT on every descriptor except the tail."""
    vq = FUZZ_MOD.VqBlob.parse(FUZZ_MOD.make_seed())
    # Ensure at least three descriptors to exercise the chain.
    vq.descs = [[16, 0, 0], [16, 0, 0], [16, 0, 0]]
    FUZZ_MOD._op_make_chain(vq)
    for i in range(len(vq.descs) - 1):
        assert vq.descs[i][1] & FUZZ_MOD._F_NEXT
        assert vq.descs[i][2] == i + 1
    assert not (vq.descs[-1][1] & FUZZ_MOD._F_NEXT)


def test_op_make_loop_sets_next_in_range():
    """make_loop sets NEXT and points within the descriptor table."""
    vq = FUZZ_MOD.VqBlob.parse(FUZZ_MOD.make_seed())
    vq.descs = [[16, 0, 9], [16, 0, 9]]
    FUZZ_MOD._op_make_loop(vq)
    looped = [d for d in vq.descs if d[1] & FUZZ_MOD._F_NEXT]
    assert looped
    for d in vq.descs:
        if d[1] & FUZZ_MOD._F_NEXT:
            assert 0 <= d[2] < len(vq.descs)


def test_op_make_indirect_clears_next():
    """make_indirect sets INDIRECT and clears NEXT on the target."""
    vq = FUZZ_MOD.VqBlob.parse(FUZZ_MOD.make_seed())
    vq.descs = [[16, FUZZ_MOD._F_NEXT, 1]]
    FUZZ_MOD._op_make_indirect(vq)
    assert vq.descs[0][1] & FUZZ_MOD._F_INDIRECT
    assert not (vq.descs[0][1] & FUZZ_MOD._F_NEXT)


def test_op_clone_desc_aliases():
    """clone_desc duplicates a descriptor so two slots match."""
    vq = FUZZ_MOD.VqBlob.parse(FUZZ_MOD.make_seed())
    vq.descs = [[512, FUZZ_MOD._F_WRITE, 0]]
    FUZZ_MOD._op_clone_desc(vq)
    assert len(vq.descs) == 2
    assert vq.descs[0] == vq.descs[1]


def test_structural_mutate_stays_well_framed():
    """structural mutation always yields a parseable BLOB_SIZE blob."""
    random.seed(1234)
    blob = FUZZ_MOD.make_seed()
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
    blob = FUZZ_MOD.make_seed()
    for _ in range(500):
        blob = bytes(FUZZ_MOD._structural_mutate(blob))
        vq = FUZZ_MOD.VqBlob.parse(blob)
        assert len(vq.descs) <= FUZZ_MOD.SOFT_DESCS
        assert len(vq.avail_ring) <= FUZZ_MOD.SOFT_AVAIL


def test_serialize_masks_oversized_fields():
    """Out of range field values are masked, never crashing struct.pack."""
    vq = FUZZ_MOD.VqBlob.parse(FUZZ_MOD.make_seed())
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
    assert RUN_MOD._is_sanitizer_noise_line(
        "==1497092==Child exited with signal 42.") is True
    # A plain VMM log line that happens to mention a child exit, without
    # the sanitizer == prefix, is not swallowed.
    assert RUN_MOD._is_sanitizer_noise_line(
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
    assert (RUN_MOD._sysctl_path("kernel.yama.ptrace_scope")
            == "/proc/sys/kernel/yama/ptrace_scope")


def test_apply_sysconfigs_changes_only_differing():
    """Only entries whose value differs from want are changed."""
    reads = {"kernel.a": "1", "kernel.b": "0"}
    writes = []
    RUN_MOD._sysctl_read = lambda key: reads.get(key)
    RUN_MOD._sysctl_write = lambda key, val: (writes.append((key, val))
                                              or True)
    configs = [
        {"key": "kernel.a", "want": "0", "why": "x"},
        {"key": "kernel.b", "want": "0", "why": "y"},
    ]
    restore = RUN_MOD.apply_sysconfigs(configs)
    # kernel.a differed (1 -> 0) and was changed; kernel.b already 0.
    assert writes == [("kernel.a", "0")]
    assert restore == {"kernel.a": "1"}


def test_apply_sysconfigs_skips_absent():
    """Absent sysctls are skipped, not changed."""
    writes = []
    RUN_MOD._sysctl_read = lambda key: None
    RUN_MOD._sysctl_write = lambda key, val: (writes.append((key, val))
                                              or True)
    configs = [{"key": "kernel.missing", "want": "0", "why": "x"}]
    restore = RUN_MOD.apply_sysconfigs(configs)
    assert writes == []
    assert restore == {}


def test_apply_sysconfigs_records_only_successful_writes():
    """A failed write is not recorded for revert."""
    RUN_MOD._sysctl_read = lambda key: "1"
    RUN_MOD._sysctl_write = lambda key, val: False
    configs = [{"key": "kernel.a", "want": "0", "why": "x"}]
    restore = RUN_MOD.apply_sysconfigs(configs)
    assert restore == {}


def test_apply_sysconfigs_warns_on_failure():
    """A needed change that fails to write emits a warning summary."""
    import io
    import contextlib
    RUN_MOD._sysctl_read = lambda key: "1"
    RUN_MOD._sysctl_write = lambda key, val: False
    configs = [{"key": "kernel.a", "want": "0", "why": "x"}]
    buf = io.StringIO()
    with contextlib.redirect_stderr(buf):
        restore = RUN_MOD.apply_sysconfigs(configs)
    assert restore == {}
    text = buf.getvalue()
    assert "WARNING" in text
    assert "kernel.a" in text


def test_revert_sysconfigs_restores_values():
    """revert_sysconfigs writes back the saved old values and clears."""
    writes = []
    RUN_MOD._sysctl_write = lambda key, val: (writes.append((key, val))
                                              or True)
    restore = {"kernel.a": "1", "kernel.b": "2"}
    RUN_MOD.revert_sysconfigs(restore)
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


def test_getcap_parses_expression():
    """getcap "PATH cap=ep" yields the capability expression."""
    orig_which = RUN_MOD.shutil.which
    orig_run = RUN_MOD.subprocess.run
    try:
        RUN_MOD.shutil.which = lambda c: "/usr/sbin/" + c
        RUN_MOD.subprocess.run = lambda *a, **k: _CP(
            0, "/path/to/vmm cap_net_admin=ep\n")
        assert RUN_MOD._getcap("/path/to/vmm") == "cap_net_admin=ep"
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
        assert RUN_MOD._getcap("/path/to/vmm") == "cap_net_admin+ep"
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
        assert RUN_MOD._getcap("/path/to/vmm") == ""
    finally:
        RUN_MOD.shutil.which = orig_which
        RUN_MOD.subprocess.run = orig_run


def test_apply_ptrace_cap_appends_clause():
    """The grant appends cap_sys_ptrace, preserving existing caps."""
    orig_which = RUN_MOD.shutil.which
    orig_lsan = RUN_MOD._is_leak_sanitized
    orig_getcap = RUN_MOD._getcap
    orig_apply = RUN_MOD._setcap_apply
    captured = []
    try:
        RUN_MOD.shutil.which = lambda c: "/usr/sbin/" + c
        RUN_MOD._is_leak_sanitized = lambda p: True
        RUN_MOD._getcap = lambda p: "cap_net_admin=ep"
        RUN_MOD._setcap_apply = lambda expr, p: (captured.append(expr) or True)
        orig = RUN_MOD.apply_ptrace_cap(__file__)
        assert captured == ["cap_net_admin=ep cap_sys_ptrace=ep"]
        assert orig == "cap_net_admin=ep"
    finally:
        RUN_MOD.shutil.which = orig_which
        RUN_MOD._is_leak_sanitized = orig_lsan
        RUN_MOD._getcap = orig_getcap
        RUN_MOD._setcap_apply = orig_apply


def test_apply_ptrace_cap_no_existing_caps():
    """With no existing caps the grant is the sole clause."""
    orig_which = RUN_MOD.shutil.which
    orig_lsan = RUN_MOD._is_leak_sanitized
    orig_getcap = RUN_MOD._getcap
    orig_apply = RUN_MOD._setcap_apply
    captured = []
    try:
        RUN_MOD.shutil.which = lambda c: "/usr/sbin/" + c
        RUN_MOD._is_leak_sanitized = lambda p: True
        RUN_MOD._getcap = lambda p: ""
        RUN_MOD._setcap_apply = lambda expr, p: (captured.append(expr) or True)
        orig = RUN_MOD.apply_ptrace_cap(__file__)
        assert captured == ["cap_sys_ptrace=ep"]
        assert orig == ""
    finally:
        RUN_MOD.shutil.which = orig_which
        RUN_MOD._is_leak_sanitized = orig_lsan
        RUN_MOD._getcap = orig_getcap
        RUN_MOD._setcap_apply = orig_apply


def test_apply_ptrace_cap_skips_when_already_present():
    """If the cap is already set, nothing is changed or recorded."""
    orig_which = RUN_MOD.shutil.which
    orig_lsan = RUN_MOD._is_leak_sanitized
    orig_getcap = RUN_MOD._getcap
    orig_apply = RUN_MOD._setcap_apply
    called = []
    try:
        RUN_MOD.shutil.which = lambda c: "/usr/sbin/" + c
        RUN_MOD._is_leak_sanitized = lambda p: True
        RUN_MOD._getcap = lambda p: "cap_sys_ptrace=ep"
        RUN_MOD._setcap_apply = lambda expr, p: (called.append(expr) or True)
        assert RUN_MOD.apply_ptrace_cap(__file__) is None
        assert called == []
    finally:
        RUN_MOD.shutil.which = orig_which
        RUN_MOD._is_leak_sanitized = orig_lsan
        RUN_MOD._getcap = orig_getcap
        RUN_MOD._setcap_apply = orig_apply


def test_apply_ptrace_cap_skips_non_lsan():
    """A VMM without the LeakSanitizer runtime is not touched."""
    orig_which = RUN_MOD.shutil.which
    orig_lsan = RUN_MOD._is_leak_sanitized
    orig_apply = RUN_MOD._setcap_apply
    called = []
    try:
        RUN_MOD.shutil.which = lambda c: "/usr/sbin/" + c
        RUN_MOD._is_leak_sanitized = lambda p: False
        RUN_MOD._setcap_apply = lambda expr, p: (called.append(expr) or True)
        assert RUN_MOD.apply_ptrace_cap(__file__) is None
        assert called == []
    finally:
        RUN_MOD.shutil.which = orig_which
        RUN_MOD._is_leak_sanitized = orig_lsan
        RUN_MOD._setcap_apply = orig_apply


def test_revert_ptrace_cap_reapplies_original():
    """A non empty original is restored with setcap."""
    orig_apply = RUN_MOD._setcap_apply
    orig_remove = RUN_MOD._setcap_remove
    applied = []
    removed = []
    try:
        RUN_MOD._setcap_apply = lambda expr, p: (applied.append((expr, p))
                                                 or True)
        RUN_MOD._setcap_remove = lambda p: (removed.append(p) or True)
        RUN_MOD.revert_ptrace_cap("/path/vmm", "cap_net_admin=ep")
        assert applied == [("cap_net_admin=ep", "/path/vmm")]
        assert removed == []
    finally:
        RUN_MOD._setcap_apply = orig_apply
        RUN_MOD._setcap_remove = orig_remove


def test_revert_ptrace_cap_clears_when_originally_none():
    """An empty original means the VMM had no caps; clear them."""
    orig_apply = RUN_MOD._setcap_apply
    orig_remove = RUN_MOD._setcap_remove
    applied = []
    removed = []
    try:
        RUN_MOD._setcap_apply = lambda expr, p: (applied.append((expr, p))
                                                 or True)
        RUN_MOD._setcap_remove = lambda p: (removed.append(p) or True)
        RUN_MOD.revert_ptrace_cap("/path/vmm", "")
        assert removed == ["/path/vmm"]
        assert applied == []
    finally:
        RUN_MOD._setcap_apply = orig_apply
        RUN_MOD._setcap_remove = orig_remove


def test_revert_ptrace_cap_noop_when_unchanged():
    """A None token means nothing was granted, so nothing is reverted."""
    orig_apply = RUN_MOD._setcap_apply
    orig_remove = RUN_MOD._setcap_remove
    touched = []
    try:
        RUN_MOD._setcap_apply = lambda expr, p: (touched.append(1) or True)
        RUN_MOD._setcap_remove = lambda p: (touched.append(1) or True)
        RUN_MOD.revert_ptrace_cap("/path/vmm", None)
        assert touched == []
    finally:
        RUN_MOD._setcap_apply = orig_apply
        RUN_MOD._setcap_remove = orig_remove


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
