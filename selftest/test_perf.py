#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
"""Selftests for the performance runner."""

import importlib.machinery
import importlib.util
import os
import subprocess
from unittest import mock

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))


def load_module():
    path = os.path.join(ROOT, "run-perf")
    loader = importlib.machinery.SourceFileLoader("vv_perf_test", path)
    spec = importlib.util.spec_from_loader("vv_perf_test", loader)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def main():
    module = load_module()
    output = (
        "VVPERF workload=blk_read round=1 block_size=4096 "
        "iterations=100 duration_ns=2000000\n"
        "VVPERF workload=blk_read round=2 block_size=4096 "
        "iterations=100 duration_ns=1000000\n")
    samples = module.parse_results(output)
    assert len(samples) == 2
    assert samples[0]["iops"] == 50000
    assert samples[1]["average_latency_ns"] == 10000
    assert samples[1]["throughput_mib_s"] == 390.625
    summary = module.summarize(samples)
    assert summary["total_requests"] == 200
    assert summary["total_duration_ns"] == 3000000
    assert round(summary["iops"], 2) == 66666.67
    assert summary["average_latency_ns"] == 15000
    timeout = subprocess.TimeoutExpired(
        ["vmm"], 1, output=output.encode())
    with mock.patch.object(module.subprocess, "run", side_effect=timeout):
        assert len(module.run_vmm(["vmm"], {}, 2)) == 2
    partial_timeout = subprocess.TimeoutExpired(
        ["vmm"], 1, output=output.splitlines()[0].encode())
    with mock.patch.object(module.subprocess, "run",
                           side_effect=partial_timeout):
        try:
            module.run_vmm(["vmm"], {}, 2)
            assert False
        except RuntimeError as error:
            assert "missing guest results" in str(error)
    failed_version = subprocess.CompletedProcess(
        ["openvmm", "--version"], 2, "",
        "error: unexpected argument '--version' found")
    with mock.patch.object(module.subprocess, "run",
                           return_value=failed_version):
        assert module.get_version("openvmm") is None
    sanitizer_version = subprocess.CompletedProcess(
        ["cloud-hypervisor", "--version"], 1,
        "cloud-hypervisor v53.0\n", "LeakSanitizer fatal error")
    with mock.patch.object(module.subprocess, "run",
                           return_value=sanitizer_version):
        assert module.get_version("cloud-hypervisor") == (
            "cloud-hypervisor v53.0")
    assert module.parse_args(["-m", "vmm"]).device == "blk"
    for device in ("blk", "rng", "net", "vsock"):
        assert module.parse_args(
            ["-m", "vmm", "--device", device]).device == device
    with mock.patch.object(module.argparse.ArgumentParser, "error",
                           side_effect=ValueError):
        try:
            module.parse_args(["-m", "vmm", "--device", "balloon"])
            assert False
        except ValueError:
            pass
    args = module.parse_args(["-m", "openvmm", "--device", "blk"])
    backend = type("Backend", (), {"name": "openvmm"})()
    with mock.patch.object(module, "get_version", return_value=None):
        report = module.make_report(args, backend, samples)
    human = module.format_human(report)
    assert "virtio block read latency" in human
    assert "Requests:      200" in human
    assert "Result:        15.00 us per request" in human
    assert "Variation:     10.00 to 20.00 us between rounds" in human
    assert "Round  Requests" not in human
    assert "VMM unknown" not in human
    verbose = module.format_human(report, verbose=True)
    assert "VMM:           openvmm" in verbose
    assert "Round  Requests" in verbose
    assert "20.00" in verbose
    assert "10.00" in verbose
    report["config"]["device"] = "net"
    report["samples"][0]["block_size"] = 64
    assert "Request size:  64 bytes" in module.format_human(report)
    assert "vsock" not in module.BACKEND_DEVICES["openvmm"]
    assert "vsock" in module.BACKEND_DEVICES["ch"]
    print("performance runner tests passed")


if __name__ == "__main__":
    main()
