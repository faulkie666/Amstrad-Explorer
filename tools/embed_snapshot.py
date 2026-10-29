#!/usr/bin/env python3
"""Validate and embed a 64 KiB CPC SNA for the flash/XIP startup path."""

from pathlib import Path
import hashlib
import sys


def main() -> int:
    if len(sys.argv) != 3:
        print("usage: embed_snapshot.py INPUT_SNA OUTPUT_HEADER", file=sys.stderr)
        return 2
    source, output = map(Path, sys.argv[1:])
    data = source.read_bytes()
    if len(data) != 256 + 65536:
        raise SystemExit(f"expected 65792-byte 64K CPC SNA, got {len(data)} bytes")
    if data[:8] != b"MV - SNA" or data[107:109] != bytes((64, 0)):
        raise SystemExit("snapshot is not a valid 64K CPC SNA")
    digest = hashlib.sha256(data).hexdigest()
    lines = [
        "// Generated from a local Knight Tyme snapshot; do not publish.",
        "#pragma once",
        "#include <cstddef>",
        f"alignas(4) static const unsigned char knight_tyme_sna[{len(data)}] = {{",
    ]
    for offset in range(0, len(data), 16):
        lines.append("    " + ", ".join(f"0x{value:02x}" for value in data[offset:offset + 16]) + ",")
    lines.extend([
        "};",
        f'static constexpr const char* knight_tyme_sna_sha256 = "{digest}";',
        "",
    ])
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_text("\n".join(lines), encoding="ascii", newline="\n")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
