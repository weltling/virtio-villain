#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
"""Host sidecar protocol checks without launching a VMM."""

from pathlib import Path
import subprocess
import sys
import tempfile
import threading
from types import SimpleNamespace
import unittest
from unittest import mock

sys.path.insert(0, str(Path(__file__).resolve().parent.parent / "lib/python"))
from villain import msix_host  # noqa: E402


class HostTests(unittest.TestCase):
    def setUp(self):
        self.directory = tempfile.TemporaryDirectory()
        self.addCleanup(self.directory.cleanup)
        self.disk = Path(self.directory.name) / "disk.raw"
        self.disk.write_bytes(b"\0" * 512 + b"Q" * 512)
        self.ctx = SimpleNamespace(
            disk=str(self.disk), tmpdir=self.directory.name,
            backend=SimpleNamespace(name="qemu"), stop=threading.Event(),
            wait_text=mock.Mock(return_value=True), log=mock.Mock(),
            skip=mock.Mock(side_effect=unittest.SkipTest))

    def test_token_is_exactly_one_sector(self):
        msix_host.write_token(self.ctx, "vv-msix-restored")
        data = self.disk.read_bytes()
        self.assertEqual(data[:512], b"vv-msix-restored".ljust(512, b"\0"))
        self.assertEqual(data[512:], b"Q" * 512)

    def test_long_token_fails(self):
        with self.assertRaises(ValueError):
            msix_host.write_token(self.ctx, "x" * 513)
        self.assertEqual(self.disk.read_bytes()[:512], b"\0" * 512)

    def test_nonraw_lifecycle_is_explicit_skip(self):
        self.ctx.disk += ".qcow2"
        with self.assertRaises(unittest.SkipTest):
            msix_host.check_disk(self.ctx)

    def test_missing_marker_fails_unless_guest_already_finished(self):
        self.ctx.wait_text.return_value = False
        with self.assertRaises(RuntimeError):
            msix_host.wait_phase(self.ctx, "ready")
        self.ctx.stop.set()
        self.assertFalse(msix_host.wait_phase(self.ctx, "ready"))

    def test_raw_capacity(self):
        self.assertEqual(msix_host.disk_capacity(self.ctx), 1024)

    def test_image_capacity_is_logical_not_file_length(self):
        self.ctx.disk += ".qcow2"
        result = subprocess.CompletedProcess([], 0, '{"virtual-size": 16777216}', "")
        with mock.patch.object(msix_host.subprocess, "run", return_value=result):
            self.assertEqual(msix_host.disk_capacity(self.ctx), 16777216)

    def test_bad_image_capacity_fails(self):
        self.ctx.disk += ".vhdx"
        for output in ('{"virtual-size": true}', '{"virtual-size": 0}'):
            result = subprocess.CompletedProcess([], 0, output, "")
            with mock.patch.object(msix_host.subprocess, "run", return_value=result):
                with self.assertRaises(RuntimeError):
                    msix_host.disk_capacity(self.ctx)
        result = subprocess.CompletedProcess([], 1, "", "bad image")
        with mock.patch.object(msix_host.subprocess, "run", return_value=result):
            with self.assertRaisesRegex(RuntimeError, "bad image"):
                msix_host.disk_capacity(self.ctx)

    def test_monitor_errors_are_not_success(self):
        with mock.patch.object(msix_host, "qmp", return_value={"return": "Error"}):
            with self.assertRaisesRegex(RuntimeError, "savevm"):
                msix_host.monitor(self.ctx, "savevm vv-msix")

    def test_ch_actually_restores_after_guest_mutation(self):
        self.ctx.backend.name = "ch"
        events = []
        with mock.patch.object(msix_host, "ch_command",
                               side_effect=lambda ctx, cmd, args=None:
                               events.append(cmd)), \
                mock.patch.object(msix_host, "write_token",
                                  side_effect=lambda ctx, token: events.append(token)), \
                mock.patch.object(msix_host, "wait_phase", return_value=True):
            msix_host.restore_mapping(self.ctx)
        self.assertEqual(events, [
            "pause", "snapshot", "resume", "vv-msix-saved", "pause", "delete",
            "restore", "vv-msix-restored", "resume",
        ])

    def test_qemu_actually_loads_saved_state(self):
        events = []

        def api(ctx, command, args=None):
            events.append(args["command-line"] if command == "human-monitor-command"
                          else command)
            return {"return": "" if command == "human-monitor-command" else {}}

        with mock.patch.object(msix_host, "qmp", side_effect=api), \
                mock.patch.object(msix_host, "write_token",
                                  side_effect=lambda ctx, token: events.append(token)), \
                mock.patch.object(msix_host, "wait_phase", return_value=True):
            msix_host.restore_mapping(self.ctx)
        self.assertEqual(events, [
            "stop", "blockdev-snapshot-sync", "savevm vv-msix", "cont",
            "vv-msix-saved", "stop", "vv-msix-restored", "loadvm vv-msix", "cont",
        ])

    def test_openvmm_uses_device_save_restore(self):
        self.ctx.backend.name = "openvmm"
        with mock.patch.object(msix_host, "ch_command") as control, \
                mock.patch.object(msix_host, "write_token") as token:
            msix_host.restore_mapping(self.ctx)
        control.assert_called_once_with(self.ctx, "pulse-save-restore")
        token.assert_called_once_with(self.ctx, "vv-msix-restored")


if __name__ == "__main__":
    unittest.main()
