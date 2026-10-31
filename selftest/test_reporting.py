#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
"""Exact human output shared by the virtio and external suite runners."""

import io
from pathlib import Path
import re
import sys
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "lib/python"))
from villain import reporting  # noqa: E402


class ReportingTests(unittest.TestCase):
    def test_listing_reserves_reference_columns_for_every_test(self):
        for referenced in (False, True):
            tests = {
                "ML0008": ("x" * 200, "", ""),
                "MR0004": ("y" * 200, "", ""),
            }
            if referenced:
                tests["TL0001"] = ("short", "6.0b", "7.8")
            for cols in (80, 140):
                with self.subTest(referenced=referenced, cols=cols):
                    rows = reporting.format_test_list(tests, cols)
                    width = cols - 20
                    for name, row in zip(tests, rows):
                        self.assertEqual(len(row), cols)
                        if name.startswith(("ML", "MR")):
                            self.assertEqual(row[-12:], " " * 12)
                            self.assertEqual(row[7:7 + width],
                                             tests[name][0][:width - 1] + "~")
                        else:
                            self.assertEqual(row[-12:], f"{'6.0b':<4} {'7.8':<7}")

    def test_long_unreferenced_result_titles_stop_before_reference_columns(self):
        tests = {
            "ML0008": ("x" * 200, "", ""),
            "MR0004": ("y" * 200, "", ""),
            "TL0001": ("short", "6.0b", "7.8"),
        }
        for cols in (80, 140):
            for color in (False, True):
                for name, info in tests.items():
                    with self.subTest(cols=cols, color=color, name=name):
                        row = reporting.format_test_line(name, "PASS", info, 200, cols, color)
                        plain = re.sub(r"\x1b\[[0-9;]*m", "", row)
                        self.assertEqual(len(plain), cols)
                        self.assertEqual(plain[-17:], f"{info[1]:<4} {info[2]:<12}")
                        if not info[1]:
                            self.assertTrue(plain[:-18].rstrip().endswith("~"))

    def test_listing_uses_native_terminal_padding_and_description_floor(self):
        tests = {
            "B0001": ("short", "1.2", "5.2.6"),
            "TL0001": ("x" * 100, "6.0b", "7.8"),
            "ML0001": ("Linux UAPI", "", ""),
        }
        for cols, width in ((30, 20), (80, 60), (140, 120)):
            with self.subTest(cols=cols):
                description = "x" * 100 if width >= 100 else "x" * (width - 1) + "~"
                self.assertEqual(reporting.format_test_list(tests, cols), [
                    f"B0001  {'short':<{width}} {'1.2':<4} {'5.2.6':<7}",
                    f"TL0001 {description:<{width}} {'6.0b':<4} {'7.8':<7}",
                    f"ML0001 {'Linux UAPI':<{width}} {'':<4} {'':<7}",
                ])

    def test_standard_result_columns(self):
        row = reporting.format_test_line(
            "B0001", "PASS", ("hello world", "1.3", "5.2"), 11)
        self.assertEqual(row, "[PASS]   B0001   hello world 1.3  5.2         ")

    def test_external_suite_uses_the_same_columns_without_fake_spec_fields(self):
        row = reporting.format_test_line(
            "ML001", "PASS", ("create partition", "", ""), 16)
        self.assertEqual(row[:17], "[PASS]   ML001   ")
        self.assertEqual(row[17:33], "create partition")
        self.assertEqual(row[33:], " " * 18)

    def test_specification_columns_follow_one_padded_description_column(self):
        cases = (
            ("x" * 100, "6.0b", "7.8"),
            ("short", "6.0b", "7.2"),
            ("register validation", "6.0b", "7.10.1"),
            ("virtio case", "1.3", "5.2"),
            ("long metadata", "123.456", "123.456.789.10"),
        )
        for cols, column in ((80, 63), (140, 118)):
            for color in (False, True):
                for info in cases:
                    with self.subTest(cols=cols, color=color, info=info):
                        row = reporting.format_test_line(
                            "TL0001", "PASS", info, 100, cols, color)
                        plain = re.sub(r"\x1b\[[0-9;]*m", "", row)
                        self.assertEqual(plain.index(info[1]), column)
                        self.assertEqual(
                            plain[column:], f"{info[1]:<4} {info[2]:<12}")

    def test_width_truncation_and_color_preserve_visible_columns(self):
        info = ("x" * 100, "1.3", "5.2")
        plain = reporting.format_test_line("B0001", "FAIL", info, 100)
        colored = reporting.format_test_line("B0001", "FAIL", info, 100, color=True)
        self.assertEqual(len(plain), 80)
        self.assertEqual(plain[17:62], "x" * 44 + "~")
        self.assertEqual(re.sub(r"\x1b\[[0-9;]*m", "", colored), plain)
        self.assertTrue(colored.startswith("\033[31m[FAIL]  \033[0m"))
        self.assertEqual(reporting.desc_column_width(200, 40, 35), 40)
        self.assertEqual(reporting.desc_column_width(60, 40, 35), 25)
        self.assertEqual(reporting.desc_column_width(30, 40, 35), 10)

    def test_summary_and_infrastructure_status(self):
        counts = dict.fromkeys(reporting.STYLES, 1)
        counts["PASS"] = 2
        expected = ("\n2/9 tests passed, 1 failed, 1 rejected, 1 wedged, "
                    "1 skipped, 1 xfail, 1 xpass, 1 infra, 1 retried")
        self.assertEqual(reporting.format_summary(counts, 9), expected)
        colored = reporting.format_summary(counts, 9, color=True)
        self.assertEqual(re.sub(r"\x1b\[[0-9;]*m", "", colored), expected)
        self.assertEqual(reporting.format_summary({"PASS": 7}, 7), "\n7/7 tests passed")

    def test_progress_and_failure_details(self):
        self.assertEqual(
            reporting.format_progress({"PASS": 2}, 2, 7, "ML002"),
            "\r\033[K2/7 \033[32m2P\033[0m | ML002")
        self.assertEqual(reporting.format_detail("expected 0, actual -1"),
                         "       expected 0, actual -1")
        self.assertEqual(
            reporting.format_failure_list([("B0001", "first case")]),
            "\nFAILED:\n  B0001   first case")
        self.assertEqual(
            reporting.format_failure_list([("ML001", "first case")], "INFRA"),
            "\nINFRASTRUCTURE:\n  ML001   first case")
        self.assertEqual(reporting.terminal_width(io.StringIO()), 80)
