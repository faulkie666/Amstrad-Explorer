#!/usr/bin/env python3
"""Fail a firmware build that could overlap the reserved game-library region."""

from pathlib import Path
import sys

FIRMWARE_RESERVE = 4 * 1024 * 1024


def main() -> int:
    if len(sys.argv) != 2:
        print("usage: check_flash_layout.py FIRMWARE.bin", file=sys.stderr)
        return 2
    image = Path(sys.argv[1])
    size = image.stat().st_size
    if size > FIRMWARE_RESERVE:
        print(f"firmware is {size} bytes and overlaps the {FIRMWARE_RESERVE}-byte reserve",
              file=sys.stderr)
        return 1
    print(f"flash layout: firmware={size} reserve={FIRMWARE_RESERVE} "
          f"margin={FIRMWARE_RESERVE - size}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
