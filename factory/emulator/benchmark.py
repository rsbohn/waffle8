"""
Python PDP-8 microbenchmark runner.

This script exercises the same tight loops as ``tools/pdp8_bench.c`` so the
pure Python emulator can be compared directly with the native implementation.
"""

from __future__ import annotations

import argparse
import sys
import time
from dataclasses import dataclass
from typing import Callable, List, Optional, Sequence

from .main import PDP8


def _oct(value: int) -> str:
    return f"{value:04o}"


@dataclass
class BenchStats:
    elapsed_sec: float
    loops: int
    instructions: int


BenchLoader = Callable[[PDP8], None]


def load_plain_loop(cpu: PDP8) -> None:
    cpu.reset(clear_memory=True)
    cpu.write_mem(0o0000, 0o7000)  # NOP
    cpu.write_mem(0o0001, 0o5000)  # JMP 0000
    cpu.set_pc(0o0000)


def load_auto_increment_loop(cpu: PDP8) -> None:
    cpu.reset(clear_memory=True)
    cpu.write_mem(0o0000, 0o1410)  # TAD I 0010
    cpu.write_mem(0o0001, 0o5000)  # JMP 0000
    cpu.write_mem(0o0010, 0o0000)  # Auto-increment pointer
    cpu.set_pc(0o0000)
    cpu.set_ac(0)
    cpu.set_link(0)


def load_jms_operate_loop(cpu: PDP8) -> None:
    cpu.reset(clear_memory=True)
    cpu.write_mem(0o0000, 0o4010)  # JMS 0010
    cpu.write_mem(0o0001, 0o5000)  # JMP 0000
    cpu.write_mem(0o0011, 0o7001)  # IAC
    cpu.write_mem(0o0012, 0o7002)  # BSW
    cpu.write_mem(0o0013, 0o7010)  # RAR
    cpu.write_mem(0o0014, 0o5410)  # JMP I 0010
    cpu.set_pc(0o0000)
    cpu.set_ac(0)
    cpu.set_link(0)


def run_benchmark(
    label: str,
    loader: BenchLoader,
    instructions_per_loop: int,
    loop_iterations: int,
) -> BenchStats:
    cpu = PDP8()
    loader(cpu)

    target_instructions = loop_iterations * instructions_per_loop
    chunk_size = 1_000_000 * instructions_per_loop
    executed = 0

    start = time.perf_counter()
    while executed < target_instructions:
        remaining = target_instructions - executed
        request = remaining if remaining < chunk_size else chunk_size
        ran = cpu.run(request)
        if ran <= 0:
            raise RuntimeError(f"[{label}] emulator stopped after {executed} instructions")
        executed += ran
    end = time.perf_counter()

    return BenchStats(elapsed_sec=end - start, loops=loop_iterations, instructions=executed)


def print_stats(label: str, stats: BenchStats) -> None:
    loops_per_sec = stats.loops / stats.elapsed_sec
    instr_per_sec = stats.instructions / stats.elapsed_sec
    print(label)
    print(f"  Loop iterations: {stats.loops} ({stats.loops / 1e6:.2f} million)")
    print(f"  Instructions executed: {stats.instructions} ({stats.instructions / 1e6:.2f} million)")
    print(f"  Elapsed time: {stats.elapsed_sec:.3f} s")
    print(f"  Throughput: {loops_per_sec / 1e6:.2f} Mloops/s, {instr_per_sec / 1e6:.2f} MIPS\n")


def parse_args(argv: Optional[Sequence[str]] = None) -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Benchmark the Python PDP-8 emulator.")
    parser.add_argument(
        "loop_count",
        nargs="?",
        type=int,
        default=5_000_000,
        help="Loop iterations per scenario (default: %(default)s).",
    )
    parser.add_argument(
        "--quiet",
        action="store_true",
        help="Suppress the per-case header banner.",
    )
    return parser.parse_args(argv)


def main(argv: Optional[Sequence[str]] = None) -> int:
    args = parse_args(argv)
    loop_iterations = args.loop_count
    if loop_iterations <= 0:
        raise SystemExit("loop_count must be positive")

    bench_cases: List[tuple[str, BenchLoader, int]] = [
        ("NOP/JMP loop", load_plain_loop, 2),
        ("Auto-increment loop", load_auto_increment_loop, 2),
        ("JMS/operate loop", load_jms_operate_loop, 6),
    ]

    if not args.quiet:
        print(f"PDP-8 microbenchmarks (loop iterations per scenario = {loop_iterations})\n")

    results: List[tuple[str, BenchStats]] = []
    for label, loader, instructions_per_loop in bench_cases:
        stats = run_benchmark(label, loader, instructions_per_loop, loop_iterations)
        results.append((label, stats))

    for label, stats in results:
        print_stats(label, stats)

    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except RuntimeError as exc:
        print(exc, file=sys.stderr)
        raise SystemExit(1)
