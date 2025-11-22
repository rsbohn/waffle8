#!/usr/bin/env python3
"""Pytest coverage for demo/clock.asm using the interactive monitor."""

from __future__ import annotations

import re
import subprocess
from datetime import datetime
from pathlib import Path
from typing import Dict, Iterable, Tuple

REPO_ROOT = Path(__file__).resolve().parents[1]
MEM_DUMP = re.compile(r"^0224:\s+([0-7]{4})\s+([0-7]{4})\s+([0-7]{4})\s+([0-7]{4})", re.MULTILINE)


def minutes_since_midnight() -> int:
    now = datetime.now()
    return now.hour * 60 + now.minute


def assemble_clock() -> None:
    subprocess.run(
        ["python3", "tools/8asm.py", "demo/clock.asm", "demo/clock.srec"],
        cwd=REPO_ROOT,
        check=True,
        capture_output=True,
        text=True,
    )


def run_monitor(commands: Iterable[str]) -> str:
    cmd = ["python3", "tools/monitor_driver.py", "--no-echo", "--image", "demo/core.srec"]
    cmd.extend(commands)
    completed = subprocess.run(
        cmd,
        cwd=REPO_ROOT,
        check=True,
        capture_output=True,
        text=True,
    )
    return completed.stdout


def parse_mem_lines(output: str) -> Tuple[Dict[str, int], Dict[str, int]]:
    matches = MEM_DUMP.findall(output)
    if len(matches) != 2:
        raise AssertionError(f"Expected two memory dumps for 0224, saw {len(matches)}\n{output}")

    def to_fields(words: Tuple[str, str, str, str]) -> Dict[str, int]:
        prtc, time_now, hours, mins = (int(word, 8) & 0x0FFF for word in words)
        return {"prtc": prtc, "time": time_now, "hours": hours, "mins": mins}

    decoded = [to_fields(words) for words in matches]
    return decoded[0], decoded[1]


def test_clock_demo_matches_host_hour() -> None:
    assemble_clock()
    start_minutes = minutes_since_midnight()
    output = run_monitor(
        [
            "read demo/clock.srec",
            "run 0200 5000",
            "mem 0224 4",
            "switch load 0211",
            "run 0211 5000",
            "mem 0224 4",
            "quit",
        ]
    )
    end_minutes = minutes_since_midnight()

    first, second = parse_mem_lines(output)
    assert first["hours"] == second["hours"], f"hour mismatch between routines\n{output}"

    expected_hours = {start_minutes // 60, end_minutes // 60}
    assert first["hours"] in expected_hours, (
        f"demo/clock.asm computed hour {first['hours']:o}, "
        f"expected one of {[f'{h:o}' for h in expected_hours]}\n{output}"
    )
