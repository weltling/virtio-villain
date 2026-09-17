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


def expect_runtime_error(function, text):
    try:
        function()
        assert False
    except RuntimeError as error:
        assert text in str(error)


def main():
    module = load_module()
    listener_socket = mock.Mock()
    listener_thread = mock.Mock()
    connection = mock.Mock()
    with mock.patch.object(module.socket, "socket",
                           return_value=listener_socket), \
            mock.patch.object(module.threading, "Thread",
                              return_value=listener_thread):
        listener = module.VsockListener(5678)
        listener.start()
        listener.connections.append(connection)
        listener.stop()
    listener_socket.bind.assert_called_once_with(
        (module.socket.VMADDR_CID_ANY, 5678))
    listener_socket.listen.assert_called_once_with(module.socket.SOMAXCONN)
    listener_socket.settimeout.assert_called_once_with(0.1)
    listener_thread.start.assert_called_once_with()
    listener_thread.join.assert_called_once_with(timeout=1)
    connection.close.assert_called_once_with()
    unix_socket = mock.Mock()
    unix_thread = mock.Mock()
    with mock.patch.object(module.socket, "socket",
                           return_value=unix_socket) as socket_factory, \
            mock.patch.object(module.threading, "Thread",
                              return_value=unix_thread):
        listener = module.VsockListener(5678, path="/tmp/vsock_5678")
        listener.start()
        listener.stop()
    socket_factory.assert_called_once_with(module.socket.AF_UNIX,
                                           module.socket.SOCK_STREAM)
    unix_socket.bind.assert_called_once_with("/tmp/vsock_5678")
    failed_socket = mock.Mock()
    failed_socket.bind.side_effect = OSError("bind failed")
    with mock.patch.object(module.socket, "socket",
                           return_value=failed_socket):
        expect_runtime_error(module.VsockListener(5678).start,
                             "Cannot listen on host vsock port 5678")
    failed_socket.close.assert_called_once_with()
    output = (
        "VVPERF version=3 experiment=queue changed=depth "
        "workload=blk operation=read round=1 request_bytes=4096 "
        "iterations=100 duration_ns=2000000 queue_format=split "
        "queue_depth=1 batch_size=1 submissions=100 completions=100 "
        "notifications=100 timing_mode=throughput "
        "clock_source=monotonic features=0x0\n"
        "VVPERF version=3 experiment=queue changed=depth "
        "workload=blk operation=read round=2 request_bytes=4096 "
        "iterations=100 duration_ns=1000000 queue_format=split "
        "queue_depth=1 batch_size=1 submissions=100 completions=100 "
        "notifications=100 timing_mode=throughput "
        "clock_source=monotonic features=0x0\n")
    samples = module.parse_results(output)
    assert len(samples) == 2
    assert samples[0]["iops"] == 50000
    assert samples[1]["mean_service_time_ns"] == 10000
    assert samples[1]["throughput_mib_s"] == 390.625
    assert samples[0]["request_bytes"] == 4096
    assert samples[0]["notifications"] == 100
    expect_runtime_error(
        lambda: module.parse_results(output.replace(" features=0x0", "", 1)),
        "missing features")
    expect_runtime_error(
        lambda: module.parse_results(output + output.splitlines()[0] + "\n"),
        "Duplicate performance round 1")
    expect_runtime_error(
        lambda: module.parse_results(output.replace("queue_depth=1",
                                                    "queue_depth=2", 1)),
        "settings changed between rounds")
    expect_runtime_error(
        lambda: module.parse_results(output.replace("duration_ns=2000000",
                                                    "duration_ns=invalid", 1)),
        "Invalid numeric performance result field")
    summary = module.summarize(samples)
    assert summary["total_requests"] == 200
    assert summary["total_duration_ns"] == 3000000
    assert round(summary["iops"], 2) == 66666.67
    assert summary["mean_service_time_ns"] == 15000
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
    args = module.parse_args(["-m", "openvmm", "--device", "blk",
                              "--changed-dimension", "depth"])
    backend = type("Backend", (), {"name": "openvmm"})()
    with mock.patch.object(module, "get_version", return_value=None):
        report = module.make_report(args, backend, samples, "/boot/vmlinux")
    assert report["schema_version"] == 3
    assert report["experiment"] == {"class": "queue",
                                    "changed_dimension": "depth"}
    assert report["queue"]["format"] == "split"
    assert report["queue"]["depth"] == 1
    assert report["workload"]["device"] == "blk"
    assert report["backend"]["io_engine"] == "default"
    assert report["guest"]["kernel"] == "/boot/vmlinux"
    assert report["vmm"]["process_mode"] == "single"
    assert module.compatibility_mismatches(report, report) == []
    changed = module.json.loads(module.json.dumps(report))
    changed["queue"]["depth"] = 16
    assert module.compatibility_mismatches(report, changed) == []
    changed["workload"]["device"] = "rng"
    assert "workload.device" in module.compatibility_mismatches(report, changed)
    human = module.format_human(report)
    assert "virtio block read latency" in human
    assert "Requests:      200" in human
    assert "Mean service:  15.00 us per request" in human
    assert "Variation:     10.00 to 20.00 us between rounds" in human
    assert "Round  Requests" not in human
    assert "VMM unknown" not in human
    verbose = module.format_human(report, verbose=True)
    assert "VMM:           openvmm" in verbose
    assert "Round  Requests" in verbose
    assert "20.00" in verbose
    assert "10.00" in verbose
    report["workload"]["device"] = "net"
    report["samples"][0]["request_bytes"] = 64
    assert "Request size:  64 bytes" in module.format_human(report)
    assert "vsock" not in module.BACKEND_DEVICES["openvmm"]
    assert "vsock" in module.BACKEND_DEVICES["ch"]
    print("performance runner tests passed")


if __name__ == "__main__":
    main()
