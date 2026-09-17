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
        "workload=blk operation=blk_read round=1 request_bytes=4096 "
        "iterations=100 duration_ns=2000000 queue_format=split "
        "queue_depth=1 batch_size=1 submissions=100 completions=100 "
        "notifications=100 timing_mode=throughput "
        "clock_source=CLOCK_MONOTONIC_RAW features=0x0\n"
        "VVPERF version=3 experiment=queue changed=depth "
        "workload=blk operation=blk_read round=2 request_bytes=4096 "
        "iterations=100 duration_ns=1000000 queue_format=split "
        "queue_depth=1 batch_size=1 submissions=100 completions=100 "
        "notifications=100 timing_mode=throughput "
        "clock_source=CLOCK_MONOTONIC_RAW features=0x0\n")
    samples = module.parse_results(output)
    assert len(samples) == 2
    assert samples[0]["operations_per_second"] == 50000
    assert samples[1]["mean_service_time_ns"] == 10000
    assert samples[1]["payload_bytes_per_second"] == 409600000
    assert samples[0]["notifications_per_submission"] == 1
    assert samples[0]["request_bytes"] == 4096
    assert samples[0]["batch_size"] == 1
    assert samples[0]["submissions"] == 100
    assert samples[0]["completions"] == 100
    assert samples[0]["notifications"] == 100
    network_samples = module.parse_results(
        output.replace("workload=blk", "workload=net").replace(
            "request_bytes=4096", "request_bytes=64"))
    assert "payload_bytes_per_second" not in network_samples[0]
    assert "payload_bytes_per_second" not in module.summarize(network_samples)
    batched_output = output.replace("batch_size=1", "batch_size=4").replace(
        "notifications=100", "notifications=25")
    batched_samples = module.parse_results(batched_output)
    assert batched_samples[0]["batch_size"] == 4
    assert batched_samples[0]["submissions"] == 100
    assert batched_samples[0]["completions"] == 100
    assert batched_samples[0]["notifications"] == 25
    assert batched_samples[0]["notifications_per_submission"] == 0.25
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
    expect_runtime_error(
        lambda: module.parse_results(output.replace("submissions=100",
                                                    "submissions=0", 1)),
        "queue counts must be positive")
    expect_runtime_error(
        lambda: module.parse_results(output.replace("notifications=100",
                                                    "notifications=0", 1)),
        "queue counts must be positive")
    summary = module.summarize(samples)
    assert summary["total_requests"] == 200
    assert summary["total_duration_ns"] == 3000000
    assert round(summary["operations_per_second"], 2) == 66666.67
    assert summary["mean_service_time_ns"] == 15000
    assert round(summary["payload_bytes_per_second"], 2) == 273066666.67
    assert summary["notifications_per_submission"] == 1
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
    cpuinfo = mock.mock_open(read_data="model name : Example CPU 1000\n")
    with mock.patch("builtins.open", cpuinfo):
        assert module.get_processor_model() == "Example CPU 1000"
    assert module.parse_args(["-m", "vmm"]).device == "blk"
    assert module.parse_args(
        ["-m", "vmm", "--queue-depth", "16"]).queue_depth == 16
    assert module.parse_args(
        ["-m", "vmm", "--queue-depth", "16",
         "--batch-size", "16"]).batch_size == 16
    assert module.parse_args(
        ["-m", "vmm", "--timing-mode", "throughput"]).timing_mode == (
            "throughput")
    with mock.patch.object(module.argparse.ArgumentParser, "error",
                           side_effect=ValueError):
        try:
            module.parse_args(["-m", "vmm", "--queue-depth", "3"])
            assert False
        except ValueError:
            pass
        try:
            module.parse_args(["-m", "vmm", "--queue-depth", "4",
                               "--batch-size", "8"])
            assert False
        except ValueError:
            pass
    cmdline_args = module.parse_args(
        ["-m", "vmm", "--device", "blk", "--queue-depth", "16",
         "--batch-size", "16"])
    command_backend = mock.Mock()
    command_backend.name = "qemu"
    command_backend.console_device = "ttyS0"
    command_backend.build_cmd.return_value = ["vmm"]
    with mock.patch.object(module, "run_vmm", return_value=samples):
        module.run_guest(cmdline_args, mock.Mock(
            detect_vmm=mock.Mock(return_value=command_backend),
            fetch_kernel=mock.Mock(return_value="kernel")))
    guest_cmdline = command_backend.build_cmd.call_args.args[3]
    assert "vv.perf_queue_depth=16" in guest_cmdline
    assert "vv.perf_batch_size=16" in guest_cmdline
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
    assert report["timing"] == {
        "mode": "throughput", "clock_source": "CLOCK_MONOTONIC_RAW"}
    assert report["workload"]["device"] == "blk"
    assert report["backend"]["io_engine"] == "default"
    assert report["backend"]["type"] == "file"
    assert report["guest"]["kernel"] == "/boot/vmlinux"
    assert report["vmm"]["process_mode"] == "single"
    assert module.compatibility_mismatches(report, report) == []
    changed = module.json.loads(module.json.dumps(report))
    changed["queue"]["depth"] = 16
    assert module.compatibility_mismatches(report, changed) == []
    changed["timing"]["clock_source"] = "another_clock"
    assert "timing.clock_source" in module.compatibility_mismatches(
        report, changed)
    changed["timing"]["clock_source"] = "CLOCK_MONOTONIC_RAW"
    changed["vmm"]["machine"] = "another_machine"
    assert "vmm.machine" in module.compatibility_mismatches(report, changed)
    changed["vmm"].pop("machine")
    changed["workload"]["device"] = "rng"
    assert "workload.device" in module.compatibility_mismatches(report, changed)
    qemu_args = module.parse_args(["-m", "qemu", "--device", "net"])
    qemu_backend = type("Backend", (), {"name": "qemu"})()
    with mock.patch.object(module, "get_version", return_value="QEMU 10"):
        qemu_report = module.make_report(
            qemu_args, qemu_backend, network_samples, "/boot/vmlinux")
    assert qemu_report["vmm"]["accelerator"] == "kvm"
    assert qemu_report["vmm"]["machine"] in {"default", "virt"}
    assert qemu_report["backend"] == {
        "type": "network", "endpoint": "user"}
    candidate = module.json.loads(module.json.dumps(report))
    candidate["summary"]["operations_per_second"] = 80000
    candidate["summary"]["operations_per_second_min"] = 60000
    candidate["summary"]["operations_per_second_max"] = 100000
    comparison = module.compare_reports(report, candidate)
    assert round(comparison["change_percent"], 2) == 20
    assert comparison["round_ranges_overlap"] is True
    assert round(comparison["baseline"]["relative_spread_percent"], 2) == 75
    assert comparison["baseline"]["vmm"]["path"] == report["vmm"]["path"]
    candidate["summary"]["operations_per_second"] = 115000
    candidate["summary"]["operations_per_second_min"] = 110000
    candidate["summary"]["operations_per_second_max"] = 120000
    comparison = module.compare_reports(report, candidate)
    assert comparison["round_ranges_overlap"] is False
    incompatible = module.json.loads(module.json.dumps(candidate))
    incompatible["timing"]["clock_source"] = "another_clock"
    expect_runtime_error(
        lambda: module.compare_reports(report, incompatible),
        "Incompatible performance reports")
    invalid_experiment = module.json.loads(module.json.dumps(report))
    invalid_experiment["experiment"]["class"] = "unknown"
    expect_runtime_error(
        lambda: module.compare_reports(invalid_experiment,
                                       invalid_experiment),
        "invalid experiment class")
    invalid_schema = module.json.loads(module.json.dumps(report))
    invalid_schema["schema_version"] = 99
    expect_runtime_error(
        lambda: module.compare_reports(invalid_schema, invalid_schema),
        "Unsupported performance report schema")
    invalid_dimension = module.json.loads(module.json.dumps(report))
    invalid_dimension["experiment"]["changed_dimension"] = "deph"
    expect_runtime_error(
        lambda: module.compare_reports(invalid_dimension,
                                       invalid_dimension),
        "invalid changed dimension")
    invalid_range = module.json.loads(module.json.dumps(report))
    invalid_range["summary"]["operations_per_second_min"] = 90000
    expect_runtime_error(
        lambda: module.compare_reports(report, invalid_range),
        "round range is invalid")
    single_round = module.json.loads(module.json.dumps(report))
    single_round["summary"]["operations_per_second_min"] = (
        single_round["summary"]["operations_per_second"])
    single_round["summary"]["operations_per_second_max"] = (
        single_round["summary"]["operations_per_second"])
    single_comparison = module.compare_reports(single_round, single_round)
    assert single_comparison["baseline"]["relative_spread_percent"] == 0
    comparison_text = module.format_comparison(comparison)
    assert "Change:       +72.50%" in comparison_text
    assert "Ranges overlap:  no" in comparison_text
    assert module.parse_args(
        ["--compare", "baseline.json", "candidate.json"]).compare == [
            "baseline.json", "candidate.json"]
    with mock.patch.object(module, "load_report", side_effect=[report, report]), \
            mock.patch.object(module, "build_guest") as build_guest, \
            mock.patch("builtins.print") as print_output:
        assert module.main(
            ["--compare", "baseline.json", "candidate.json"]) == 0
    build_guest.assert_not_called()
    assert "Change:       +0.00%" in print_output.call_args.args[0]
    human = module.format_human(report)
    assert "virtio block read throughput" in human
    assert "Requests:      200" in human
    assert "Operation rate: 66666.67 operations/s" in human
    assert "Round range:    50000.00 to 100000.00 operations/s" in human
    assert "Mean service:  15.00 us per operation" in human
    assert "Notify ratio:  1.0000 per submission" in human
    assert "Payload rate: 260.42 MiB/s" in human
    assert "Queue depth:   1" in human
    assert "Batch size:    1" in human
    assert "Round  Requests" not in human
    assert "VMM unknown" not in human
    verbose = module.format_human(report, verbose=True)
    assert "VMM:           openvmm" in verbose
    assert "Round  Requests" in verbose
    assert "50000.00" in verbose
    assert "100000.00" in verbose
    report["workload"]["device"] = "net"
    report["samples"][0]["request_bytes"] = 64
    assert "Request size:  64 bytes" in module.format_human(report)
    report["queue"]["depth"] = 16
    assert "Queue depth:   16" in module.format_human(report)
    assert "vsock" not in module.BACKEND_DEVICES["openvmm"]
    assert "vsock" in module.BACKEND_DEVICES["ch"]
    with open(os.path.join(ROOT, "bin", "perf.c"), encoding="utf-8") as source_file:
        guest_source = source_file.read()
    request_start = guest_source.index(
        "static enum perf_request_result run_requests")
    request_end = guest_source.index("static const char *request_error")
    assert "clock_gettime" not in guest_source[request_start:request_end]
    assert guest_source.count("clock_gettime(CLOCK_MONOTONIC_RAW") == 2
    print("performance runner tests passed")


if __name__ == "__main__":
    main()
