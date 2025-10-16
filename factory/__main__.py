"""
Module entry point so `python -m factory` runs the factory driver CLI.
"""

import sys

from .driver import EmulatorError, main


def _run() -> None:
    try:
        exit_code = main()
    except EmulatorError as exc:
        print(f"factory: {exc}", file=sys.stderr)
        exit_code = 1
    sys.exit(exit_code)


if __name__ == "__main__":
    _run()
