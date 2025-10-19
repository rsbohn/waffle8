#!/usr/bin/env python3
"""Helper for issuing sequential commands to ./monitor.

The monitor polls its console input between command prompts, so feeding
multiple commands through a pipe can drop later inputs. This script keeps a
persistent session, writes commands one at a time, and waits for the prompt
(`pdp8> ` by default) to reappear before continuing.
"""

import argparse
import os
import subprocess
import sys
from collections import deque
from typing import Iterable, List, Sequence


def parse_args(argv: Sequence[str]) -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--monitor",
        default="./monitor",
        help="Path to monitor executable (default: ./monitor)",
    )
    parser.add_argument(
        "--image",
        help="Optional startup image passed to the monitor (e.g. demo/core.srec)",
    )
    parser.add_argument(
        "--prompt",
        default="pdp8> ",
        help="Prompt string to wait for between commands (default: %(default)s)",
    )
    parser.add_argument(
        "--command-file",
        type=str,
        help="Read newline-delimited commands from the given file",
    )
    parser.add_argument(
        "command",
        nargs="*",
        help="Commands to send to the monitor (order preserved)",
    )
    parser.add_argument(
        "--echo",
        dest="echo",
        action="store_true",
        default=True,
        help="Print monitor output as it arrives (default)",
    )
    parser.add_argument(
        "--no-echo",
        dest="echo",
        action="store_false",
        help="Collect output silently (still returned on stdout at the end)",
    )
    parser.add_argument(
        "--timeout",
        type=float,
        default=10.0,
        help="Seconds to wait for monitor output before giving up",
    )
    return parser.parse_args(argv)


def gather_commands(args: argparse.Namespace) -> List[str]:
    commands: List[str] = []
    if args.command_file:
        with open(args.command_file, "r", encoding="ascii") as handle:
            for line in handle:
                stripped = line.strip()
                if stripped and not stripped.startswith("#"):
                    commands.append(stripped)
    commands.extend(args.command)
    return commands


def stream_until_prompt(
    proc: subprocess.Popen,
    prompt: str,
    echo: bool,
    timeout: float,
) -> str:
    buffer: List[str] = []
    tail: deque[str] = deque(maxlen=len(prompt))
    proc_stdout = proc.stdout
    if proc_stdout is None:
        return ""

    proc.poll()  # refresh state
    while True:
        ch = proc_stdout.read(1)
        if ch == "":
            code = proc.poll()
            if code is not None:
                break
            proc_stdout.flush()
            continue
        buffer.append(ch)
        tail.append(ch)
        if echo:
            sys.stdout.write(ch)
            sys.stdout.flush()
        if len(tail) == len(prompt) and "".join(tail) == prompt:
            break
    return "".join(buffer)


def main(argv: Sequence[str]) -> int:
    args = parse_args(argv)
    commands = gather_commands(args)

    monitor_path = args.monitor
    if not os.path.exists(monitor_path):
        print(f"monitor executable '{monitor_path}' not found", file=sys.stderr)
        return 1

    launch_cmd = [monitor_path]
    if args.image:
        launch_cmd.append(args.image)

    proc = subprocess.Popen(
        launch_cmd,
        stdin=subprocess.PIPE,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        text=True,
        bufsize=1,
    )

    transcript: List[str] = []
    try:
        transcript.append(stream_until_prompt(proc, args.prompt, args.echo, args.timeout))
        for cmd in commands:
            if proc.stdin is None:
                break
            proc.stdin.write(cmd + "\n")
            proc.stdin.flush()
            transcript.append(stream_until_prompt(proc, args.prompt, args.echo, args.timeout))
            if cmd.strip().lower() in {"quit", "exit"}:
                break
    finally:
        if proc.stdin:
            proc.stdin.close()

    stdout_remaining = ""
    stderr_remaining = ""
    if proc.stdout:
        stdout_remaining = proc.stdout.read() or ""
    if proc.stderr:
        stderr_remaining = proc.stderr.read() or ""

    if not args.echo and transcript:
        sys.stdout.write("".join(transcript))
    if stdout_remaining:
        sys.stdout.write(stdout_remaining)
    if stderr_remaining:
        sys.stderr.write(stderr_remaining)

    return proc.wait()


if __name__ == "__main__":
    raise SystemExit(main(sys.argv[1:]))
