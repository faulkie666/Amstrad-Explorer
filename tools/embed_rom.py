#!/usr/bin/env python3
"""Generate a C header containing the unmodified CPC 464 OS and BASIC ROMs."""

from pathlib import Path
import hashlib
import sys


EXPECTED_SHA256 = "00960d9bf75b2b90856c970f1aa078e1e2aa028b2c104f1dded0262f5d37b15e"


def emit_array(name: str, data: bytes) -> str:
    lines = [f"alignas(4) static const unsigned char {name}[16384] = {{"]
    for offset in range(0, len(data), 16):
        chunk = ", ".join(f"0x{value:02x}" for value in data[offset:offset + 16])
        lines.append(f"    {chunk},")
    lines.append("};")
    return "\n".join(lines)


def main() -> int:
    if len(sys.argv) != 3:
        print("usage: embed_rom.py INPUT_CPC464_ROM OUTPUT_HEADER", file=sys.stderr)
        return 2
    source, output = map(Path, sys.argv[1:])
    image = source.read_bytes()
    digest = hashlib.sha256(image).hexdigest()
    if len(image) != 32768:
        raise SystemExit(f"expected 32768-byte CPC464.ROM, got {len(image)} bytes")
    if digest != EXPECTED_SHA256:
        raise SystemExit(f"unexpected CPC464.ROM SHA-256: {digest}")
    text = "\n".join([
        "// Generated from an unmodified CPC464.ROM; do not edit.",
        "#pragma once",
        "#include <cstddef>",
        emit_array("cpc464_os_rom", image[:16384]),
        emit_array("cpc464_basic_rom", image[16384:]),
        f'static constexpr const char* cpc464_rom_sha256 = "{digest}";',
        "",
    ])
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_text(text, encoding="ascii", newline="\n")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
