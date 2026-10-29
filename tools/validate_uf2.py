#!/usr/bin/env python3
"""Validate CPC Explorer RP2350 UF2 targets, including the guarded E10 block."""

from __future__ import annotations

import argparse
import hashlib
import json
import struct
from pathlib import Path

BLOCK_BYTES = 512
PAYLOAD_CAPACITY = 476
MAGIC0 = 0x0A324655
MAGIC1 = 0x9E5D5157
MAGIC_END = 0x0AB16F30
FAMILY_FLAG = 0x00002000
EXTENSION_FLAG = 0x00008000
ARM_S_FAMILY = 0xE48BFF59
ABSOLUTE_FAMILY = 0xE48BFF57
E10_FLAGS = FAMILY_FLAG | EXTENSION_FLAG
E10_TARGET = 0x10FFFF00
E10_EXTENSION = 0x9957E304
XIP_BASE = 0x10000000
FIRMWARE_LIMIT = XIP_BASE + 4 * 1024 * 1024


class ValidationError(ValueError):
    pass


def validate_bytes(data: bytes) -> dict[str, object]:
    if not data or len(data) % BLOCK_BYTES:
        raise ValidationError("UF2 size must be a nonzero multiple of 512 bytes")
    application: list[tuple[int, int, int, int]] = []
    e10_count = 0
    for physical in range(len(data) // BLOCK_BYTES):
        block = data[physical * BLOCK_BYTES:(physical + 1) * BLOCK_BYTES]
        magic0, magic1, flags, target, payload_size, number, total, family = struct.unpack_from(
            "<IIIIIIII", block)
        end_magic = struct.unpack_from("<I", block, 508)[0]
        if (magic0, magic1, end_magic) != (MAGIC0, MAGIC1, MAGIC_END):
            raise ValidationError(f"block {physical}: invalid UF2 magic")
        if payload_size == 0 or payload_size > PAYLOAD_CAPACITY:
            raise ValidationError(f"block {physical}: invalid payload size {payload_size}")
        if family == ARM_S_FAMILY:
            if flags != FAMILY_FLAG:
                raise ValidationError(f"block {physical}: unexpected application flags 0x{flags:08x}")
            end = target + payload_size
            if end > 0xFFFFFFFF or target < XIP_BASE or end > FIRMWARE_LIMIT:
                raise ValidationError(
                    f"block {physical}: application target 0x{target:08x}..0x{end:08x} "
                    "is outside the first 4 MiB")
            if payload_size != 256 or target % 256:
                raise ValidationError(f"block {physical}: application block is not a 256-byte page")
            application.append((target, end, number, total))
            continue
        if family == ABSOLUTE_FAMILY:
            if e10_count:
                raise ValidationError("more than one absolute-family E10 block")
            extension = struct.unpack_from("<I", block, 32 + payload_size)[0]
            payload = block[32:32 + payload_size]
            if (flags != E10_FLAGS or target != E10_TARGET or payload_size != 256 or
                    number != 0 or total != 2 or extension != E10_EXTENSION or
                    payload != bytes((0xEF,)) * 256):
                raise ValidationError(f"block {physical}: malformed or unexpected absolute-family block")
            e10_count += 1
            continue
        raise ValidationError(f"block {physical}: unexpected family 0x{family:08x}")

    if not application:
        raise ValidationError("UF2 contains no rp2350-arm-s application blocks")
    totals = {entry[3] for entry in application}
    numbers = [entry[2] for entry in application]
    if len(totals) != 1 or totals.pop() != len(application) or sorted(numbers) != list(range(len(application))):
        raise ValidationError("application block numbering/count is inconsistent")
    ordered = sorted(application)
    for previous, current in zip(ordered, ordered[1:]):
        if current[0] < previous[1]:
            raise ValidationError("application target ranges overlap")
    return {
        "physical_blocks": len(data) // BLOCK_BYTES,
        "application_blocks": len(application),
        "application_min_target": f"0x{ordered[0][0]:08x}",
        "application_max_target_exclusive": f"0x{max(entry[1] for entry in ordered):08x}",
        "application_family": f"0x{ARM_S_FAMILY:08x}",
        "e10_blocks": e10_count,
        "e10_family": f"0x{ABSOLUTE_FAMILY:08x}" if e10_count else None,
        "e10_flags": f"0x{E10_FLAGS:08x}" if e10_count else None,
        "e10_target": f"0x{E10_TARGET:08x}" if e10_count else None,
        "e10_payload": "256 bytes of 0xef" if e10_count else None,
        "e10_extension": f"0x{E10_EXTENSION:08x}" if e10_count else None,
        "e10_treated_as_potentially_actionable": bool(e10_count),
        "sha256": hashlib.sha256(data).hexdigest(),
    }


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("uf2", type=Path)
    parser.add_argument("--expect-sha256")
    args = parser.parse_args()
    try:
        data = args.uf2.read_bytes()
        report = validate_bytes(data)
        if args.expect_sha256 and report["sha256"].lower() != args.expect_sha256.lower():
            raise ValidationError("SHA-256 does not match expected value")
    except (OSError, ValidationError) as error:
        print(f"error: {error}")
        return 1
    print(json.dumps(report, indent=2))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
