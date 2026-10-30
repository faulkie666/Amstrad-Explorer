#!/usr/bin/env python3
"""Build an isolated AMSDOS DSK from copied source files via konCePCja IPC."""

from __future__ import annotations

import argparse
from pathlib import Path

from stage4i4_koncepcja import Emulator, quoted


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--exe", type=Path, required=True)
    parser.add_argument("--cfg", type=Path, required=True)
    parser.add_argument("--cwd", type=Path, required=True)
    parser.add_argument("--input-dir", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("files", nargs="+")
    args = parser.parse_args()
    args.output.parent.mkdir(parents=True, exist_ok=True)

    # `disk new` writes a deterministic blank image but does not insert it.
    first = Emulator(args.exe, args.cfg, args.cwd, None)
    try:
        response = first.command(f"disk new {quoted(args.output)} data sector")
        if not response.startswith("OK"):
            raise RuntimeError(response)
    finally:
        first.close()

    second = Emulator(args.exe, args.cfg, args.cwd, args.output)
    try:
        for name in args.files:
            source = args.input_dir / name
            if not source.is_file():
                raise FileNotFoundError(source)
            response = second.command(
                f"disk put A {quoted(source)} {name} auto", timeout=20.0
            )
            if not response.startswith("OK"):
                raise RuntimeError(f"disk put {name}: {response}")
        listing = second.command("disk ls A")
        response = second.command(f"disk save A {quoted(args.output)} dsk")
        if not response.startswith("OK"):
            raise RuntimeError(response)
        print(listing)
    finally:
        second.close()
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
