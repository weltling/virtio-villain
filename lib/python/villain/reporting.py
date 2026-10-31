# SPDX-License-Identifier: Apache-2.0
"""Human result formatting shared by virtio and external appliance suites."""

import os

RESET = "\033[0m"
DIM = "\033[2m"
STYLES = {
    "PASS": ("\033[32m", "P", ""),
    "FAIL": ("\033[31m", "F", "failed"),
    "REJECT": ("\033[35m", "R", "rejected"),
    "WEDGED": ("\033[33m", "W", "wedged"),
    "SKIP": ("\033[36m", "S", "skipped"),
    "XFAIL": ("\033[34m", "XF", "xfail"),
    "XPASS": ("\033[31m", "XP", "xpass"),
    "INFRA": ("\033[31m", "I", "infra"),
    "RETRIED": ("\033[93m", "RT", "retried"),
}


def terminal_width(stream):
    try:
        return os.get_terminal_size(stream.fileno()).columns
    except (OSError, ValueError):
        return 80


def desc_column_width(cols, longest_desc, fixed):
    return max(10, min(cols - fixed, longest_desc))


def format_test_list(tests, cols=80):
    name_width = max(4, max(map(len, tests), default=0))
    version_width = max(4, max((len(info[1]) for info in tests.values()), default=0))
    section_width = max(7, max((len(info[2]) for info in tests.values()), default=0))
    width = max(20, cols - name_width - version_width - section_width - 3)
    lines = []
    for name, (description, version, section) in tests.items():
        if len(description) > width:
            description = description[:width - 1] + "~"
        lines.append(
            f"{name:<{name_width}} {description:<{width}} "
            f"{version:<{version_width}} {section:<{section_width}}")
    return lines


def format_test_line(name, status, info, longest_desc, cols=80, color=False):
    tag = f"[{status}]".ljust(8)
    if color:
        tag = f"{STYLES.get(status, ('', '', ''))[0]}{tag}{RESET}"
    width = desc_column_width(cols, longest_desc, 35)
    description, version, section = info
    if len(description) > width:
        description = description[:width - 1] + "~"
    return f"{tag} {name:<7} {description:<{width}} {version:<4} {section:<12}"


def format_detail(line, color=False):
    return f"{DIM}       {line}{RESET}" if color else f"       {line}"


def format_progress(counts, done, total, current="", cols=80):
    parts = [f"{done}/{total}"]
    for status, (color, abbreviation, _) in STYLES.items():
        count = counts.get(status, 0)
        if count:
            parts.append(f"{color}{count}{abbreviation}{RESET}")
    line = " ".join(parts)
    if current:
        line += f" | {current}"
    return f"\r\033[K{line[:cols]}"


def format_summary(counts, total, color=False):
    summary = f"{counts.get('PASS', 0)}/{total} tests passed"
    if color:
        summary = f"\033[1m{summary}{RESET}"
    for status, (escape, _, label) in STYLES.items():
        count = counts.get(status, 0)
        if status != "PASS" and count:
            text = f"{count} {label}"
            summary += ", " + (f"{escape}{text}{RESET}" if color else text)
    return "\n" + summary


def format_failure_list(entries, status="FAIL", color=False):
    title = "INFRASTRUCTURE:" if status == "INFRA" else "FAILED:"
    if color:
        title = f"{STYLES[status][0]}{title}{RESET}"
    return "\n" + title + "\n" + "\n".join(
        f"  {name:<7} {description}" for name, description in entries)
