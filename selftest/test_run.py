#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
"""Selftests for the virtio-villain test runner."""

import importlib.machinery
import importlib.util
import os
import random
import shutil
import subprocess
import sys
import tempfile

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
ROOT_DIR = os.path.dirname(SCRIPT_DIR)
RUN = os.path.join(ROOT_DIR, "run")
MOCK_VMM = os.path.join(SCRIPT_DIR, "mock-vmm")

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
                 "skip": 1, "xfail": 1, "xpass": 1}


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

def main():
    setup()
    global FUZZ_MOD
    FUZZ_MOD = _load_fuzz_module()
    use_color = sys.stdout.isatty() and not os.environ.get("NO_COLOR")
    c_pass = "\033[32m" if use_color else ""
    c_fail = "\033[31m" if use_color else ""
    c_reset = "\033[0m" if use_color else ""
    tests = [v for k, v in globals().items() if k.startswith("test_")]
    passed = 0
    failed = 0
    for t in tests:
        try:
            t()
            print(f"  {c_pass}[PASS]{c_reset} {t.__name__}")
            passed += 1
        except AssertionError as e:
            print(f"  {c_fail}[FAIL]{c_reset} {t.__name__}: {e}")
            failed += 1
        except Exception as e:
            print(f"  {c_fail}[FAIL]{c_reset} {t.__name__}: "
                  f"{type(e).__name__}: {e}")
            failed += 1
    tag = c_pass if failed == 0 else c_fail
    print(f"\n{tag}selftest/run: {passed}/{passed + failed} passed{c_reset}")
    return 0 if failed == 0 else 1


if __name__ == "__main__":
    sys.exit(main())
