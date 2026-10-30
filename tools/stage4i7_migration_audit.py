#!/usr/bin/env python3
"""Offline-only Stage 4I.7 migration audit and interruption simulation.

This program deliberately has no serial, USB, drive, or flash backend.  It
validates supplied evidence and models the required publication ordering; it
cannot perform a physical migration.
"""

from __future__ import annotations

import argparse
import hashlib
import json
import struct
import sys
import zlib
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from validate_uf2 import ValidationError, validate_bytes  # noqa: E402

FLASH_BYTES = 16 * 1024 * 1024
EXPECTED_JEDEC = "EF-40-18"
CATALOG_VERSION = 1
PACK_OFFSET = 0x43B000
E10_GUARD = 0xFF0000
SECTOR = 4096


def align_sector(value: int) -> int:
    return (value + SECTOR - 1) & ~(SECTOR - 1)


def validate_pack(data: bytes) -> dict[str, int | str]:
    if len(data) < 16384 or data[:7] != b"CPXPCK2":
        raise ValueError("pack magic/size")
    version, header_size = struct.unpack_from("<HH", data, 8)
    image_size = struct.unpack_from("<I", data, 36)[0]
    stored_crc = struct.unpack_from("<I", data, 48)[0]
    header = bytearray(data[:128])
    struct.pack_into("<I", header, 48, 0)
    if version != 2 or header_size != 128 or image_size != len(data):
        raise ValueError("pack header")
    if zlib.crc32(header) & 0xFFFFFFFF != stored_crc:
        raise ValueError("pack header CRC")
    end = PACK_OFFSET + len(data)
    reserved_end = align_sector(end)
    if end > E10_GUARD or reserved_end > E10_GUARD:
        raise ValueError("pack overlaps E10 guard")
    return {
        "sha256": hashlib.sha256(data).hexdigest(),
        "bytes": len(data),
        "offset": PACK_OFFSET,
        "end_exclusive": end,
        "reserved_end_exclusive": reserved_end,
        "saved_high_water_relative": reserved_end - 0x400000,
        "free_bytes": E10_GUARD - reserved_end,
    }


def simulate(pack: bytes) -> list[dict[str, object]]:
    """Model every interruption boundary; no byte is written to a device."""
    info = validate_pack(pack)
    phases = [
        ("preflight", False, False, False, True),
        ("catalog_b_reserved_verified", True, False, False, True),
        ("catalog_a_reserved_verified", True, True, False, True),
        ("pack_sectors_erased", True, True, False, True),
        ("payload_and_tables_programmed_verified", True, True, False, True),
        ("header_b_programmed", True, True, True, True),
        ("header_a_programmed_full_hash_verified", True, True, True, True),
        ("uf2_may_be_deployed_separately", True, True, True, True),
    ]
    result = []
    for name, allocator_safe, pack_program_allowed, pack_mounts, legacy_boots in phases:
        result.append({
            "interrupted_after": name,
            "legacy_catalog_preserved": legacy_boots,
            "old_firmware_allocator_protected": allocator_safe,
            "stage4i7_pack_mounts": pack_mounts,
            "safe_to_program_pack_next": pack_program_allowed,
            "automatic_retry_allowed": False,
        })
    assert info["reserved_end_exclusive"] <= E10_GUARD
    return result


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--pack", required=True, type=Path)
    parser.add_argument("--uf2", required=True, type=Path)
    parser.add_argument("--expect-pack-sha256", required=True)
    parser.add_argument("--expect-uf2-sha256", required=True)
    parser.add_argument("--jedec", required=True)
    parser.add_argument("--capacity", required=True, type=int)
    parser.add_argument("--catalog-version", required=True, type=int)
    args = parser.parse_args()
    try:
        if args.jedec.upper() != EXPECTED_JEDEC or args.capacity != FLASH_BYTES:
            raise ValueError("unexpected JEDEC identity or capacity")
        if args.catalog_version != CATALOG_VERSION:
            raise ValueError("unexpected catalogue version")
        pack_data = args.pack.read_bytes()
        uf2_data = args.uf2.read_bytes()
        pack = validate_pack(pack_data)
        uf2 = validate_bytes(uf2_data)
        if pack["sha256"] != args.expect_pack_sha256.lower():
            raise ValueError("pack SHA-256 mismatch")
        if uf2["sha256"] != args.expect_uf2_sha256.lower():
            raise ValueError("UF2 SHA-256 mismatch")
        if uf2["e10_blocks"] != 1:
            raise ValueError("authorized E10 block missing")
    except (OSError, ValueError, ValidationError) as error:
        print(f"HOLD: {error}")
        return 1
    print(json.dumps({
        "mode": "offline-only; no device transport exists",
        "preflight": "PASS",
        "pack": pack,
        "uf2": uf2,
        "interruption_simulation": simulate(pack_data),
        "physical_writer": "NOT IMPLEMENTED",
        "deployment_authorized": False,
    }, indent=2))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
