#!/usr/bin/env python3
"""
Pretty-print PDP-8 trace output from the webdp8 REST front-end.

Example:
  python3 tools/webdp_trace.py --start 0200 --cycles 512 --pc 0320 --instr 4076
"""

from __future__ import annotations

import argparse
import json
import sys
import urllib.parse
import urllib.request
from typing import Dict, Iterable, List, Sequence, Tuple


FIELD_DEFS: Dict[str, Tuple[str, str]] = {
    "step": ("Step", "step"),
    "pc": ("PC", "pc_before"),
    "instr": ("Instr", "instr"),
    "pc_after": ("PC'", "pc_after"),
    "ac_before": ("AC", "ac_before"),
    "ac_after": ("AC'", "ac_after"),
    "link_before": ("L", "link_before"),
    "link_after": ("L'", "link_after"),
    "halted": ("Halt", "halted"),
    "cycles": ("Cycles", "cycles"),
}

DEFAULT_FIELDS = ("step", "pc", "instr", "ac_before", "ac_after", "link_before", "halted")


def fetch_trace(server: str, start: str, cycles: int) -> Dict:
    params = urllib.parse.urlencode({"start": start, "cycles": cycles})
    url = f"{server.rstrip('/')}/trace?{params}"
    request = urllib.request.Request(url)
    with urllib.request.urlopen(request, timeout=30) as resp:
        try:
            payload = json.loads(resp.read().decode("utf-8"))
        except json.JSONDecodeError as exc:
            raise RuntimeError(f"Invalid JSON from {url}: {exc}") from exc
    return payload


def select_fields(field_names: Sequence[str]) -> List[Tuple[str, str]]:
    if not field_names:
        field_names = DEFAULT_FIELDS
    selected: List[Tuple[str, str]] = []
    for name in field_names:
        key = name.strip().lower()
        if key not in FIELD_DEFS:
            available = ", ".join(sorted(FIELD_DEFS))
            raise ValueError(f"Unknown field '{name}'. Available: {available}")
        selected.append(FIELD_DEFS[key])
    return selected


def match_one(value: str, candidates: Iterable[str]) -> bool:
    reference = value.upper()
    return any(reference == candidate.upper() for candidate in candidates)


def filter_steps(
    steps: Sequence[Dict],
    pcs: Sequence[str],
    instrs: Sequence[str],
    halted_only: bool,
    step_ids: Sequence[int],
) -> List[Dict]:
    filtered = list(steps)
    if pcs:
        filtered = [step for step in filtered if match_one(step.get("pc_before", ""), pcs)]
    if instrs:
        filtered = [step for step in filtered if match_one(step.get("instr", ""), instrs)]
    if halted_only:
        filtered = [step for step in filtered if bool(step.get("halted"))]
    if step_ids:
        wanted = set(step_ids)
        filtered = [step for step in filtered if int(step.get("step", -1)) in wanted]
    return filtered


def render_table(steps: Sequence[Dict], fields: Sequence[Tuple[str, str]]) -> str:
    if not steps:
        return "(no steps to display)"

    headers = [title for title, _ in fields]
    columns: List[List[str]] = []
    for _, source in fields:
        column: List[str] = []
        for step in steps:
            value = step.get(source, "")
            if isinstance(value, bool):
                column.append("1" if value else "0")
            else:
                column.append(str(value))
        columns.append(column)

    widths = []
    for header, column in zip(headers, columns):
        widths.append(max(len(header), *(len(cell) for cell in column)))

    lines = []
    header_line = "  ".join(header.ljust(width) for header, width in zip(headers, widths))
    divider = "  ".join("-" * width for width in widths)
    lines.extend([header_line, divider])

    for row_values in zip(*columns):
        line = "  ".join(cell.ljust(width) for cell, width in zip(row_values, widths))
        lines.append(line)
    return "\n".join(lines)


def parse_args(argv: Sequence[str]) -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Inspect a PDP-8 execution trace from webdp8.")
    parser.add_argument("--server", default="http://127.0.0.1:5000", help="Base URL of webdp8 (default: %(default)s)")
    parser.add_argument("--start", default="0100", help="Starting PC (octal string)")
    parser.add_argument("--cycles", type=int, default=512, help="Cycles to execute (default: %(default)s)")
    parser.add_argument("--pc", action="append", default=[], help="Filter by pc_before (octal string). Repeatable.")
    parser.add_argument("--instr", action="append", default=[], help="Filter by opcode word (octal). Repeatable.")
    parser.add_argument("--step", action="append", type=int, default=[], help="Filter by step index. Repeatable.")
    parser.add_argument("--halted-only", action="store_true", help="Show only steps where the CPU halted.")
    parser.add_argument("--fields", help="Comma-separated list of columns to display.")
    parser.add_argument("--limit", type=int, help="Limit number of displayed rows after filtering.")
    return parser.parse_args(argv)


def main(argv: Sequence[str]) -> int:
    args = parse_args(argv)
    try:
        fields = select_fields(args.fields.split(",") if args.fields else [])
    except ValueError as exc:
        print(exc, file=sys.stderr)
        return 2

    try:
        payload = fetch_trace(args.server, args.start, args.cycles)
    except Exception as exc:  # pragma: no cover - network failures
        print(f"Trace request failed: {exc}", file=sys.stderr)
        return 3

    steps = payload.get("steps", [])
    filtered = filter_steps(steps, args.pc, args.instr, args.halted_only, args.step)
    if args.limit is not None:
        filtered = filtered[: max(args.limit, 0)]

    summary = (
        f"Trace begin_pc={payload.get('begin_pc','????')} "
        f"halted={payload.get('halted')} "
        f"steps={len(steps)} filtered={len(filtered)}"
    )
    print(summary)
    print(render_table(filtered, fields))
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))

