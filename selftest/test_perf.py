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
    values = [10, 20, 30, 40, 50, 60, 70, 80, 90, 100]
    assert module.nearest_rank(values, 50) == 50
    assert module.nearest_rank(values, 90) == 90
    assert module.nearest_rank(values, 99) == 100
    assert module.nearest_rank(values, 99.9) == 100
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
    network_socket = mock.Mock()
    network_thread = mock.Mock()
    with mock.patch.object(module.socket, "socket",
                           return_value=network_socket), \
            mock.patch.object(module.threading, "Thread",
                              return_value=network_thread):
        sender = module.NetworkSender(41001, 41000)
        sender.start()
        sender.stop()
    network_socket.bind.assert_called_once_with(("127.0.0.1", 41001))
    network_thread.start.assert_called_once_with()
    network_thread.join.assert_called_once_with(timeout=1)
    network_socket.close.assert_called_once_with()
    broadcast_socket = mock.Mock()
    broadcast_thread = mock.Mock()
    with mock.patch.object(module.socket, "socket",
                           return_value=broadcast_socket), \
            mock.patch.object(module.threading, "Thread",
                              return_value=broadcast_thread):
        sender = module.NetworkSender(
            41001, 41000, source_address="0.0.0.0",
            destination_address="198.18.7.255", broadcast=True)
        sender.start()
        sender.stop()
    broadcast_socket.setsockopt.assert_called_once_with(
        module.socket.SOL_SOCKET, module.socket.SO_BROADCAST, 1)
    broadcast_socket.bind.assert_called_once_with(("0.0.0.0", 41001))
    sender = module.NetworkSender(41001, 41000)
    sender.socket = mock.Mock()
    sender.running = True
    sender.socket.sendto.side_effect = lambda frame, destination: setattr(
        sender, "running", False)
    sender._send()
    frame, destination = sender.socket.sendto.call_args.args
    assert destination == ("127.0.0.1", 41000)
    assert frame == bytes.fromhex(
        "ffffffffffff02020202020288b5") + bytes([0x42]) * 50
    sender = module.NetworkSender(41001, 41000, payload_only=True)
    sender.socket = mock.Mock()
    sender.running = True
    sender.socket.sendto.side_effect = lambda frame, destination: setattr(
        sender, "running", False)
    sender._send()
    frame, destination = sender.socket.sendto.call_args.args
    assert destination == ("127.0.0.1", 41000)
    assert frame == bytes([0x42]) * 22
    sender = module.NetworkSender(
        41001, 41000, payload_only=True, source_address="0.0.0.0",
        destination_address="198.18.7.255", broadcast=True)
    sender.socket = mock.Mock()
    sender.running = True
    sender.socket.sendto.side_effect = lambda frame, destination: setattr(
        sender, "running", False)
    sender._send()
    frame, destination = sender.socket.sendto.call_args.args
    assert destination == ("198.18.7.255", 41000)
    assert frame == bytes([0x42]) * 22
    output = (
        "VVPERF version=3 experiment=queue changed=depth "
        "workload=blk operation=blk_read address_pattern=fixed "
        "round=1 request_bytes=4096 "
        "iterations=100 duration_ns=2000000 queue_format=split "
        "descriptor_layout=direct notification_policy=always "
        "queue_depth=1 batch_size=1 device_queues=1 "
        "submissions=100 completions=100 "
        "notifications=100 timing_mode=throughput "
        "clock_source=CLOCK_MONOTONIC_RAW sample_every=0 features=0x0\n"
        "VVPERF version=3 experiment=queue changed=depth "
        "workload=blk operation=blk_read address_pattern=fixed "
        "round=2 request_bytes=4096 "
        "iterations=100 duration_ns=1000000 queue_format=split "
        "descriptor_layout=direct notification_policy=always "
        "queue_depth=1 batch_size=1 device_queues=1 "
        "submissions=100 completions=100 "
        "notifications=100 timing_mode=throughput "
        "clock_source=CLOCK_MONOTONIC_RAW sample_every=0 features=0x0\n")
    samples = module.parse_results(output)
    assert len(samples) == 2
    assert samples[0]["operations_per_second"] == 50000
    assert samples[1]["mean_service_time_ns"] == 10000
    assert samples[1]["payload_bytes_per_second"] == 409600000
    assert samples[0]["notifications_per_submission"] == 1
    assert samples[0]["request_bytes"] == 4096
    assert samples[0]["address_pattern"] == "fixed"
    assert samples[0]["batch_size"] == 1
    assert samples[0]["device_queues"] == 1
    assert samples[0]["descriptor_layout"] == "direct"
    assert samples[0]["notification_policy"] == "always"
    assert samples[0]["submissions"] == 100
    assert samples[0]["completions"] == 100
    assert samples[0]["notifications"] == 100
    write_samples = module.parse_results(
        output.replace("operation=blk_read", "operation=blk_write").replace(
            "address_pattern=fixed", "address_pattern=random"))
    assert write_samples[0]["operation"] == "blk_write"
    assert write_samples[0]["address_pattern"] == "random"
    latency_output = (
        "VVPERF_LATENCY round=1 sample=1 latency_ns=100\n"
        "VVPERF_LATENCY round=1 sample=2 latency_ns=300\n"
        "VVPERF_LATENCY round=2 sample=1 latency_ns=200\n"
        "VVPERF_LATENCY round=2 sample=2 latency_ns=400\n" +
        output.replace("timing_mode=throughput", "timing_mode=latency").replace(
            "clock_source=CLOCK_MONOTONIC_RAW sample_every=0",
            "clock_source=RDTSCP sample_every=50"))
    latency_samples = module.parse_results(latency_output)
    assert latency_samples[0]["latency_samples_ns"] == [100, 300]
    assert latency_samples[1]["latency_samples_ns"] == [200, 400]
    latency_summary = module.summarize(latency_samples)
    assert latency_summary["latency_sample_count"] == 4
    assert latency_summary["latency_p50_ns"] == 200
    assert latency_summary["latency_p90_ns"] == 400
    assert latency_summary["latency_p99_ns"] == 400
    assert latency_summary["latency_p999_ns"] == 400
    expect_runtime_error(
        lambda: module.parse_results(latency_output.replace(
            "VVPERF_LATENCY round=1 sample=1 latency_ns=100\n", "")),
        "sample identifiers are invalid")
    expect_runtime_error(
        lambda: module.parse_results(
            output.replace("timing_mode=throughput", "timing_mode=latency").
            replace("clock_source=CLOCK_MONOTONIC_RAW sample_every=0",
                    "clock_source=RDTSCP sample_every=50")),
        "Latency result lacks sample records")
    expect_runtime_error(
        lambda: module.parse_results(
            "VVPERF_LATENCY round=1 sample=1\n" + output),
        "Invalid latency sample record")
    expect_runtime_error(
        lambda: module.parse_results(
            "VVPERF_LATENCY round=1 sample=1 latency_ns=500\n" +
            latency_output),
        "Duplicate latency sample identifier")
    expect_runtime_error(
        lambda: module.parse_results(latency_output.replace(
            "latency_ns=100", "latency_ns=0", 1)),
        "Latency sample values must be positive")
    expect_runtime_error(
        lambda: module.parse_results(
            "VVPERF_LATENCY round=3 sample=1 latency_ns=500\n" +
            latency_output),
        "unknown round")
    expect_runtime_error(
        lambda: module.parse_results(output.replace(
            "sample_every=0", "sample_every=10")),
        "Throughput result has a sampling interval")
    expect_runtime_error(
        lambda: module.parse_results(latency_output.replace(
            "clock_source=RDTSCP", "clock_source=CLOCK_MONOTONIC_RAW")),
        "Latency result has an invalid clock source")
    network_samples = module.parse_results(
        output.replace("workload=blk", "workload=net").replace(
            "request_bytes=4096", "request_bytes=64").replace(
            "operation=blk_read address_pattern=fixed",
            "operation=net_tx address_pattern=none"))
    assert "payload_bytes_per_second" not in network_samples[0]
    assert "payload_bytes_per_second" not in module.summarize(network_samples)
    network_receive_samples = module.parse_results(
        output.replace("workload=blk", "workload=net").replace(
            "request_bytes=4096", "request_bytes=64").replace(
            "operation=blk_read address_pattern=fixed",
            "operation=net_rx address_pattern=none"))
    assert network_receive_samples[0]["payload_bytes_per_second"] == 3200000
    assert module.summarize(network_receive_samples)[
        "payload_bytes_per_second"] == 4266666.666666667
    batched_output = output.replace("batch_size=1", "batch_size=4").replace(
        "notifications=100", "notifications=25")
    batched_samples = module.parse_results(batched_output)
    assert batched_samples[0]["batch_size"] == 4
    assert batched_samples[0]["submissions"] == 100
    assert batched_samples[0]["completions"] == 100
    assert batched_samples[0]["notifications"] == 25
    assert batched_samples[0]["notifications_per_submission"] == 0.25
    multiqueue_output = output.replace(
        "device_queues=1", "device_queues=4").replace(
            "features=0x0", "features=0x1000")
    multiqueue_samples = module.parse_results(multiqueue_output)
    assert multiqueue_samples[0]["device_queues"] == 4
    assert multiqueue_samples[0]["negotiated_features"] == "0x1000"
    indirect_output = output.replace(
        "descriptor_layout=direct", "descriptor_layout=indirect").replace(
            "features=0x0", "features=0x10000000")
    indirect_samples = module.parse_results(indirect_output)
    assert indirect_samples[0]["descriptor_layout"] == "indirect"
    packed_output = output.replace(
        "queue_format=split", "queue_format=packed").replace(
            "features=0x0", "features=0x400000000")
    packed_samples = module.parse_results(packed_output)
    assert packed_samples[0]["queue_format"] == "packed"
    event_idx_output = output.replace(
        "notification_policy=always", "notification_policy=event_idx").replace(
            "notifications=100", "notifications=0").replace(
            "features=0x0", "features=0x20000000")
    event_idx_samples = module.parse_results(event_idx_output)
    assert event_idx_samples[0]["notification_policy"] == "event_idx"
    assert event_idx_samples[0]["notifications"] == 0
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
        "Always notify result lacks notifications")
    expect_runtime_error(
        lambda: module.parse_results(output.replace(
            "operation=blk_read", "operation=blk_flush")),
        "invalid operation")
    expect_runtime_error(
        lambda: module.parse_results(output.replace(
            "workload=blk operation=blk_read address_pattern=fixed",
            "workload=net operation=net_drop address_pattern=none")),
        "Network result has an invalid operation")
    expect_runtime_error(
        lambda: module.parse_results(output.replace(
            "address_pattern=fixed", "address_pattern=unknown")),
        "invalid address pattern")
    expect_runtime_error(
        lambda: module.parse_results(output.replace(
            "workload=blk operation=blk_read address_pattern=fixed",
            "workload=rng operation=rng_fill address_pattern=random")),
        "Nonblock result has an address pattern")
    expect_runtime_error(
        lambda: module.parse_results(output.replace(
            "device_queues=1", "device_queues=0")),
        "device queue count must be positive")
    expect_runtime_error(
        lambda: module.parse_results(output.replace(
            "device_queues=1", "device_queues=2")),
        "multiqueue result lacks MQ")
    expect_runtime_error(
        lambda: module.parse_results(multiqueue_output.replace(
            "workload=blk operation=blk_read address_pattern=fixed",
            "workload=rng operation=rng_fill address_pattern=none")),
        "Nonblock result has multiple device queues")
    expect_runtime_error(
        lambda: module.parse_results(output.replace(
            "descriptor_layout=direct", "descriptor_layout=unknown")),
        "invalid descriptor layout")
    expect_runtime_error(
        lambda: module.parse_results(output.replace(
            "descriptor_layout=direct", "descriptor_layout=indirect")),
        "Indirect result lacks descriptor feature")
    expect_runtime_error(
        lambda: module.parse_results(output.replace(
            "queue_format=split", "queue_format=unknown")),
        "invalid queue format")
    expect_runtime_error(
        lambda: module.parse_results(output.replace(
            "queue_format=split", "queue_format=packed")),
        "Packed result lacks packed ring feature")
    expect_runtime_error(
        lambda: module.parse_results(output.replace(
            "notification_policy=always", "notification_policy=unknown")),
        "invalid notification policy")
    expect_runtime_error(
        lambda: module.parse_results(output.replace(
            "notification_policy=always", "notification_policy=event_idx")),
        "Event index result lacks event index feature")
    summary = module.summarize(samples)
    assert summary["total_requests"] == 200
    assert summary["total_duration_ns"] == 3000000
    assert round(summary["operations_per_second"], 2) == 66666.67
    assert summary["operations_per_second_median"] == 75000
    assert summary["operations_per_second_mad"] == 25000
    assert round(summary["operations_per_second_mad_percent"], 2) == 33.33
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
        ["-m", "vmm", "--queue-depth", "32"]).queue_depth == 32
    assert module.parse_args(
        ["-m", "vmm", "--queue-depth", "16",
         "--batch-size", "16"]).batch_size == 16
    assert module.parse_args(
        ["-m", "vmm", "--queue-depth", "32",
         "--batch-size", "32"]).batch_size == 32
    assert module.parse_args(
        ["-m", "vmm", "--device", "blk",
         "--device-queues", "4"]).device_queues == 4
    assert module.parse_args(
        ["-m", "vmm", "--descriptor-layout", "indirect"]).descriptor_layout == (
            "indirect")
    assert module.parse_args(
        ["-m", "vmm", "--queue-format", "packed"]).queue_format == "packed"
    assert module.parse_args(
        ["-m", "vmm", "--notification-policy", "event_idx"]
    ).notification_policy == "event_idx"
    assert module.parse_args(
        ["-m", "vmm", "--timing-mode", "throughput"]).timing_mode == (
            "throughput")
    block_args = module.parse_args(
        ["-m", "vmm", "--device", "blk", "--block-operation", "write",
         "--block-pattern", "random", "--block-request-size", "1024"])
    assert block_args.block_operation == "write"
    assert block_args.block_pattern == "random"
    assert block_args.block_request_size == 1024
    net_rx_args = module.parse_args(
        ["-m", "vmm", "--device", "net", "--net-operation", "receive"])
    assert net_rx_args.net_operation == "receive"
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
        try:
            module.parse_args(["-m", "vmm", "--device", "rng",
                               "--block-operation", "write"])
            assert False
        except ValueError:
            pass
        try:
            module.parse_args(["-m", "vmm", "--device", "rng",
                               "--block-request-size", "1024"])
            assert False
        except ValueError:
            pass
        try:
            module.parse_args(["-m", "vmm", "--device", "rng",
                               "--device-queues", "2"])
            assert False
        except ValueError:
            pass
        try:
            module.parse_args(["-m", "vmm", "--device", "rng",
                               "--net-operation", "receive"])
            assert False
        except ValueError:
            pass
        try:
            module.parse_args(["-m", "vmm", "--queue-format", "packed",
                               "--notification-policy", "event_idx"])
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
    assert "vv.perf_device_queues=1" in guest_cmdline
    assert "vv.perf_descriptor_layout=direct" in guest_cmdline
    assert "vv.perf_queue_format=split" in guest_cmdline
    assert "vv.perf_notification_policy=always" in guest_cmdline
    assert "vv.perf_block_operation=read" in guest_cmdline
    assert "vv.perf_block_pattern=fixed" in guest_cmdline
    assert "vv.perf_block_request_size=4096" in guest_cmdline
    with mock.patch.object(module, "run_vmm", return_value=samples):
        module.run_guest(block_args, mock.Mock(
            detect_vmm=mock.Mock(return_value=command_backend),
            fetch_kernel=mock.Mock(return_value="kernel")))
    block_cmdline = command_backend.build_cmd.call_args.args[3]
    assert "vv.perf_block_operation=write" in block_cmdline
    assert "vv.perf_block_pattern=random" in block_cmdline
    assert "vv.perf_block_request_size=1024" in block_cmdline
    with mock.patch.object(module, "run_vmm",
                           return_value=network_receive_samples), \
            mock.patch.object(module, "unused_udp_port",
                              side_effect=[41001, 41000]), \
            mock.patch.object(module, "NetworkSender") as sender:
        module.run_guest(net_rx_args, mock.Mock(
            detect_vmm=mock.Mock(return_value=command_backend),
            fetch_kernel=mock.Mock(return_value="kernel")))
    net_rx_cmdline = command_backend.build_cmd.call_args.args[3]
    net_rx_opts = command_backend.build_cmd.call_args.args[4]
    assert "vv.perf_net_operation=receive" in net_rx_cmdline
    assert "vv.perf_net_header_size=10" in net_rx_cmdline
    assert net_rx_opts["net_rx_source_port"] == 41001
    assert net_rx_opts["net_rx_port"] == 41000
    sender.return_value.start.assert_called_once_with()
    sender.return_value.stop.assert_called_once_with()
    openvmm_backend = mock.Mock(name="openvmm_backend")
    openvmm_backend.name = "openvmm"
    openvmm_backend.console_device = "ttyS0"
    openvmm_backend.build_cmd.return_value = ["openvmm"]
    with mock.patch.object(module, "run_vmm",
                           return_value=network_receive_samples), \
            mock.patch.object(module, "unused_udp_port",
                              side_effect=[41001, 41000]), \
            mock.patch.object(module, "NetworkSender") as sender:
        module.run_guest(net_rx_args, mock.Mock(
            detect_vmm=mock.Mock(return_value=openvmm_backend),
            fetch_kernel=mock.Mock(return_value="kernel")))
    net_rx_cmdline = openvmm_backend.build_cmd.call_args.args[3]
    net_rx_opts = openvmm_backend.build_cmd.call_args.args[4]
    assert "vv.perf_net_header_size=12" in net_rx_cmdline
    assert net_rx_opts["net_rx_port"] == 41000
    assert "net_rx_source_port" not in net_rx_opts
    sender.assert_called_once_with(41001, 41000, payload_only=True)
    sender.return_value.start.assert_called_once_with()
    sender.return_value.stop.assert_called_once_with()
    ch_backend = mock.Mock(name="ch_backend")
    ch_backend.name = "ch"
    ch_backend.console_device = "ttyS0"
    ch_backend.build_cmd.return_value = ["cloud-hypervisor"]
    expect_runtime_error(
        lambda: module.run_guest(net_rx_args, mock.Mock(
            detect_vmm=mock.Mock(return_value=ch_backend),
            fetch_kernel=mock.Mock(return_value="kernel"),
            _getcap=mock.Mock(return_value=""))),
        "requires cap_net_admin+ep")
    with mock.patch.object(module, "run_vmm",
                           return_value=network_receive_samples), \
            mock.patch.object(module, "unused_udp_port",
                              side_effect=[41001, 41000]), \
            mock.patch.object(module.os, "getpid", return_value=6), \
            mock.patch.object(module, "NetworkSender") as sender:
        module.run_guest(net_rx_args, mock.Mock(
            detect_vmm=mock.Mock(return_value=ch_backend),
            fetch_kernel=mock.Mock(return_value="kernel"),
            _getcap=mock.Mock(return_value="cap_net_admin=ep")))
    net_rx_cmdline = ch_backend.build_cmd.call_args.args[3]
    net_rx_opts = ch_backend.build_cmd.call_args.args[4]
    assert "vv.perf_net_header_size=12" in net_rx_cmdline
    assert net_rx_opts["net_rx_tap"] == "vvperf6"
    assert net_rx_opts["net_rx_ip"] == "198.18.7.1"
    sender.assert_called_once_with(
        41001, 41000, payload_only=True, source_address="0.0.0.0",
        destination_address="198.18.7.255", broadcast=True)
    sender.return_value.start.assert_called_once_with()
    sender.return_value.stop.assert_called_once_with()
    assert module.has_effective_capability("cap_net_admin=ep",
                                           "cap_net_admin")
    assert module.has_effective_capability(
        "cap_net_admin,cap_sys_ptrace=ep", "cap_net_admin")
    assert not module.has_effective_capability("cap_net_admin=p",
                                               "cap_net_admin")
    assert not module.has_effective_capability("cap_net_admin=e",
                                               "cap_net_admin")
    queue_args = module.parse_args(
        ["-m", "vmm", "--device", "blk", "--device-queues", "4"])
    with mock.patch.object(module, "run_vmm", return_value=samples):
        module.run_guest(queue_args, mock.Mock(
            detect_vmm=mock.Mock(return_value=command_backend),
            fetch_kernel=mock.Mock(return_value="kernel")))
    queue_cmdline = command_backend.build_cmd.call_args.args[3]
    queue_opts = command_backend.build_cmd.call_args.args[4]
    assert "vv.perf_device_queues=4" in queue_cmdline
    assert queue_opts["blk_queues"] == 4
    assert queue_opts["cpus"] == 4
    indirect_args = module.parse_args(
        ["-m", "vmm", "--device", "blk",
         "--descriptor-layout", "indirect"])
    with mock.patch.object(module, "run_vmm", return_value=indirect_samples):
        module.run_guest(indirect_args, mock.Mock(
            detect_vmm=mock.Mock(return_value=command_backend),
            fetch_kernel=mock.Mock(return_value="kernel")))
    indirect_cmdline = command_backend.build_cmd.call_args.args[3]
    assert "vv.perf_descriptor_layout=indirect" in indirect_cmdline
    packed_args = module.parse_args(
        ["-m", "vmm", "--queue-format", "packed"])
    with mock.patch.object(module, "run_vmm", return_value=packed_samples):
        module.run_guest(packed_args, mock.Mock(
            detect_vmm=mock.Mock(return_value=command_backend),
            fetch_kernel=mock.Mock(return_value="kernel")))
    packed_cmdline = command_backend.build_cmd.call_args.args[3]
    packed_opts = command_backend.build_cmd.call_args.args[4]
    assert "vv.perf_queue_format=packed" in packed_cmdline
    assert packed_opts["packed"] is True
    event_idx_args = module.parse_args(
        ["-m", "vmm", "--notification-policy", "event_idx"])
    with mock.patch.object(module, "run_vmm", return_value=event_idx_samples):
        module.run_guest(event_idx_args, mock.Mock(
            detect_vmm=mock.Mock(return_value=command_backend),
            fetch_kernel=mock.Mock(return_value="kernel")))
    event_idx_cmdline = command_backend.build_cmd.call_args.args[3]
    assert "vv.perf_notification_policy=event_idx" in event_idx_cmdline
    latency_args = module.parse_args(
        ["-m", "vmm", "--device", "blk", "--timing-mode", "latency",
         "--sample-every", "25"])
    with mock.patch.object(module, "run_vmm", return_value=latency_samples):
        module.run_guest(latency_args, mock.Mock(
            detect_vmm=mock.Mock(return_value=command_backend),
            fetch_kernel=mock.Mock(return_value="kernel")))
    latency_cmdline = command_backend.build_cmd.call_args.args[3]
    latency_opts = command_backend.build_cmd.call_args.args[4]
    assert "vv.perf_timing_mode=latency" in latency_cmdline
    assert "vv.perf_sample_every=25" in latency_cmdline
    assert latency_opts["cpu_model"] == "host"
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
        write_report = module.make_report(
            block_args, backend, write_samples, "/boot/vmlinux")
        queue_report = module.make_report(
            queue_args, backend, multiqueue_samples, "/boot/vmlinux")
        indirect_report = module.make_report(
            indirect_args, backend, indirect_samples, "/boot/vmlinux")
        packed_report = module.make_report(
            packed_args, backend, packed_samples, "/boot/vmlinux")
        event_idx_report = module.make_report(
            event_idx_args, backend, event_idx_samples, "/boot/vmlinux")
    assert report["schema_version"] == 3
    assert report["experiment"] == {"class": "queue",
                                    "changed_dimension": "depth"}
    assert report["queue"]["format"] == "split"
    assert report["queue"]["depth"] == 1
    assert report["queue"]["device_queues"] == 1
    assert queue_report["queue"]["device_queues"] == 4
    assert queue_report["execution"]["cpus"] == 4
    assert queue_report["queue"]["negotiated_features"] == "0x1000"
    assert indirect_report["queue"]["descriptor_layout"] == "indirect"
    assert packed_report["queue"]["format"] == "packed"
    assert event_idx_report["queue"]["notification_policy"] == "event_idx"
    assert report["timing"] == {
        "mode": "throughput", "clock_source": "CLOCK_MONOTONIC_RAW",
        "sample_every": 0}
    with mock.patch.object(module, "get_version", return_value=None):
        latency_report = module.make_report(
            latency_args, backend, latency_samples, "/boot/vmlinux")
    assert latency_report["timing"] == {
        "mode": "latency", "clock_source": "RDTSCP", "sample_every": 50}
    assert "timing.mode" in module.compatibility_mismatches(
        report, latency_report)
    assert report["workload"]["device"] == "blk"
    assert report["workload"]["operation"] == "blk_read"
    assert report["workload"]["address_pattern"] == "fixed"
    assert write_report["workload"]["operation"] == "blk_write"
    assert write_report["workload"]["address_pattern"] == "random"
    assert report["backend"]["io_engine"] == "default"
    assert report["backend"]["type"] == "file"
    assert report["guest"]["kernel"] == "/boot/vmlinux"
    assert report["vmm"]["process_mode"] == "single"
    assert module.compatibility_mismatches(report, report) == []
    changed = module.json.loads(module.json.dumps(report))
    changed["queue"]["depth"] = 16
    assert module.compatibility_mismatches(report, changed) == []
    report["experiment"]["changed_dimension"] = "format"
    changed = module.json.loads(module.json.dumps(report))
    changed["queue"]["format"] = "packed"
    assert module.compatibility_mismatches(report, changed) == []
    report["experiment"]["changed_dimension"] = "notification_policy"
    changed = module.json.loads(module.json.dumps(report))
    changed["queue"]["notification_policy"] = "event_idx"
    assert module.compatibility_mismatches(report, changed) == []
    changed["queue"]["depth"] = 1
    changed["queue"]["device_queues"] = 2
    assert "queue.device_queues" in module.compatibility_mismatches(
        report, changed)
    changed = module.json.loads(module.json.dumps(report))
    changed["queue"]["descriptor_layout"] = "indirect"
    assert "queue.descriptor_layout" in module.compatibility_mismatches(
        report, changed)
    report["experiment"]["changed_dimension"] = "descriptor_layout"
    changed["experiment"]["changed_dimension"] = "descriptor_layout"
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
    changed = module.json.loads(module.json.dumps(report))
    changed["workload"]["address_pattern"] = "random"
    assert "workload.address_pattern" in module.compatibility_mismatches(
        report, changed)
    qemu_args = module.parse_args(["-m", "qemu", "--device", "net"])
    qemu_backend = type("Backend", (), {"name": "qemu"})()
    with mock.patch.object(module, "get_version", return_value="QEMU 10"):
        qemu_report = module.make_report(
            qemu_args, qemu_backend, network_samples, "/boot/vmlinux")
    assert qemu_report["vmm"]["accelerator"] == "kvm"
    assert qemu_report["vmm"]["machine"] in {"default", "virt"}
    assert qemu_report["vmm"]["cpu_model"] == "default"
    assert qemu_report["backend"] == {
        "type": "network", "endpoint": "user"}
    with mock.patch.object(module, "get_version", return_value="QEMU 10"):
        qemu_latency_report = module.make_report(
            latency_args, qemu_backend, latency_samples, "/boot/vmlinux")
    assert qemu_latency_report["vmm"]["cpu_model"] == "host"
    changed_cpu = module.json.loads(module.json.dumps(qemu_latency_report))
    changed_cpu["vmm"]["cpu_model"] = "default"
    assert "vmm.cpu_model" in module.compatibility_mismatches(
        qemu_latency_report, changed_cpu)
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
    assert "Address pattern: fixed" in human
    write_human = module.format_human(write_report)
    assert "virtio block write throughput" in write_human
    assert "Address pattern: random" in write_human
    assert "Requests:      200" in human
    assert "Operation rate: 66666.67 operations/s" in human
    assert "Median rate:    75000.00 operations/s" in human
    assert "Dispersion:     25000.00 operations/s MAD, 33.33%" in human
    assert "Round range:    50000.00 to 100000.00 operations/s" in human
    assert "Mean service:  15.00 us per operation" in human
    assert "Notify ratio:  1.0000 per submission" in human
    assert "Payload rate: 260.42 MiB/s" in human
    assert "Queue depth:   1" in human
    assert "Queue format:  split" in human
    assert "Device queues: 1" in human
    assert "Descriptors:   direct" in human
    assert "Notifications: always" in human
    assert "Batch size:    1" in human
    assert "Round  Requests" not in human
    assert "VMM unknown" not in human
    latency_human = module.format_human(latency_report)
    assert "virtio block read latency" in latency_human
    assert "Latency samples: 4" in latency_human
    assert "Latency p50:  200 ns" in latency_human
    assert "Latency p99.9: 400 ns" in latency_human
    verbose = module.format_human(report, verbose=True)
    assert "VMM:           openvmm" in verbose
    assert "Round  Requests" in verbose
    assert "50000.00" in verbose
    assert "100000.00" in verbose
    report["workload"]["device"] = "net"
    report["samples"][0]["request_bytes"] = 64
    assert "Request size:  64 bytes" in module.format_human(report)
    net_rx_report = module.make_report(
        net_rx_args, qemu_backend, network_receive_samples, "/boot/vmlinux")
    net_rx_human = module.format_human(net_rx_report)
    assert net_rx_report["backend"] == {
        "type": "network", "endpoint": "socket"}
    assert "virtio network receive throughput" in net_rx_human
    assert "Payload rate:" in net_rx_human
    openvmm_net_rx_report = module.make_report(
        net_rx_args, backend, network_receive_samples, "/boot/vmlinux")
    assert openvmm_net_rx_report["backend"] == {
        "type": "network", "endpoint": "consomme"}
    ch_net_rx_report = module.make_report(
        net_rx_args, ch_backend, network_receive_samples, "/boot/vmlinux")
    assert ch_net_rx_report["backend"] == {
        "type": "network", "endpoint": "tap"}
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
    assert guest_source.count("clock_gettime(CLOCK_MONOTONIC_RAW") == 4
    print("performance runner tests passed")


if __name__ == "__main__":
    main()
