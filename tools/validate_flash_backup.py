#!/usr/bin/env python3
"""Validate and optionally extract a complete 16 MiB CPC Explorer flash backup."""

from __future__ import annotations

import argparse
import hashlib
import json
import struct
import sys
import zlib
from pathlib import Path

FLASH_BYTES = 16 * 1024 * 1024
LIBRARY_OFFSET = 0x400000
LIBRARY_BYTES = 0xBF0000
CATALOG_BYTES = 0x4000
DATA_START = 0x8000
SNAPSHOT_BYTES = 256 + 65536
E10_OFFSET = 0xFF0000
PACK_START = 0x43B000
PACK_END = 0x467000
BUILTIN_IDS = ("locomotive_basic", "knight_tyme")
EXPECTED_IDS = ("knight_tyme", "manic_miner", "dizzy")


class ValidationError(ValueError):
    pass


def u16(data: bytes | bytearray, offset: int) -> int:
    return struct.unpack_from("<H", data, offset)[0]


def u32(data: bytes | bytearray, offset: int) -> int:
    return struct.unpack_from("<I", data, offset)[0]


def masked_crc(data: bytes | bytearray, offset: int, size: int, masked: int) -> int:
    block = bytearray(data[offset:offset + size])
    block[masked:masked + 4] = b"\0" * 4
    return zlib.crc32(block)


def text_field(data: bytes | bytearray, offset: int, size: int) -> str:
    field = bytes(data[offset:offset + size])
    if b"\0" not in field:
        raise ValidationError(f"unterminated text field at 0x{offset:x}")
    raw = field.split(b"\0", 1)[0]
    if not raw or any(byte < 0x20 or byte > 0x7E for byte in raw):
        raise ValidationError(f"invalid text field at 0x{offset:x}")
    return raw.decode("ascii")


def parse_catalog(image: bytes, slot: int) -> dict[str, object]:
    base = LIBRARY_OFFSET + slot * CATALOG_BYTES
    catalog = image[base:base + CATALOG_BYTES]
    if catalog[:8] != b"CPCLIB1\0" or u16(catalog, 8) != 1 or u16(catalog, 10) != 64:
        raise ValidationError(f"catalogue {slot} header identity is invalid")
    count = u16(catalog, 16)
    if count > 192 or u16(catalog, 18) != 80 or u32(catalog, 20) != DATA_START or \
            u32(catalog, 24) != LIBRARY_BYTES or u32(catalog, 32) != 0x4D4D4F43:
        raise ValidationError(f"catalogue {slot} geometry/commit marker is invalid")
    if masked_crc(catalog, 0, 64, 28) != u32(catalog, 28):
        raise ValidationError(f"catalogue {slot} header CRC is invalid")
    entries: list[dict[str, object]] = []
    for index in range(count):
        at = 64 + index * 80
        if at + 80 > CATALOG_BYTES or masked_crc(catalog, at, 80, 72) != u32(catalog, at + 72):
            raise ValidationError(f"catalogue {slot} descriptor {index} CRC/bounds are invalid")
        game_id = text_field(catalog, at, 16)
        title = text_field(catalog, at + 16, 40)
        machine, ram, version, controls = catalog[at + 56:at + 60]
        offset, length, expected_crc = u32(catalog, at + 60), u32(catalog, at + 64), u32(catalog, at + 68)
        if machine != 1 or ram != 64 or version == 0 or controls > 2 or \
                offset < DATA_START or offset % 4096 or length != SNAPSHOT_BYTES or \
                offset > LIBRARY_BYTES or length > LIBRARY_BYTES - offset:
            raise ValidationError(f"catalogue {slot} descriptor {index} is incompatible or out of range")
        absolute = LIBRARY_OFFSET + offset
        payload = image[absolute:absolute + length]
        if len(payload) != length or zlib.crc32(payload) != expected_crc or \
                payload[:8] != b"MV - SNA" or payload[16] != version or \
                payload[107] != 64 or payload[108] != 0:
            raise ValidationError(f"catalogue {slot} payload {game_id} failed SNA/CRC validation")
        entries.append({
            "id": game_id, "title": title, "machine": "CPC464", "ram_kib": ram,
            "snapshot_version": version, "control_profile": controls,
            "flash_offset": absolute, "length": length, "crc32": f"{expected_crc:08x}",
            "sha256": hashlib.sha256(payload).hexdigest(),
        })
    return {
        "slot": slot, "flash_offset": base, "generation": u32(catalog, 12),
        "count": count, "high_water": u32(catalog, 40), "entries": entries,
        "sha256": hashlib.sha256(catalog).hexdigest(),
    }


def validate(image: bytes, expected_sha256: str | None = None,
             expected_ids: tuple[str, ...] = ()) -> dict[str, object]:
    if len(image) != FLASH_BYTES:
        raise ValidationError(f"backup must be exactly {FLASH_BYTES} bytes, got {len(image)}")
    image_sha = hashlib.sha256(image).hexdigest()
    if expected_sha256 and image_sha.lower() != expected_sha256.lower():
        raise ValidationError("complete backup SHA-256 does not match the independently recorded value")
    catalogs = [parse_catalog(image, slot) for slot in range(2)]
    active = max(catalogs, key=lambda item: int(item["generation"]))
    if expected_ids:
        wanted = set(expected_ids)
        actual = {str(entry["id"]) for entry in active["entries"]}
        if actual != wanted:
            raise ValidationError(
                f"active catalogue {active['slot']} IDs {sorted(actual)} != expected {sorted(wanted)}")
    allocations: dict[tuple[int, int, int], dict[str, object]] = {}
    for catalog in catalogs:
        for entry in catalog["entries"]:
            key = (int(entry["flash_offset"]), int(entry["length"]), int(entry["crc32"], 16))
            existing = allocations.get(key)
            if existing and existing["id"] != entry["id"]:
                raise ValidationError("catalogues assign different IDs to the same allocation")
            allocations[key] = entry
    ranges = sorted((key[0], key[0] + key[1], entry["id"]) for key, entry in allocations.items())
    for previous, current in zip(ranges, ranges[1:]):
        if current[0] < previous[1]:
            raise ValidationError(f"allocations overlap: {previous[2]} and {current[2]}")
    pack_region = image[PACK_START:PACK_END]
    return {
        "flash_bytes": len(image), "sha256": image_sha,
        "firmware_region_sha256": hashlib.sha256(image[:LIBRARY_OFFSET]).hexdigest(),
        "e10_guard_sha256": hashlib.sha256(image[E10_OFFSET:]).hexdigest(),
        "catalogues": catalogs, "active_catalogue": active["slot"],
        "allocations": list(allocations.values()),
        "visible_games": [*BUILTIN_IDS] + [entry["id"] for entry in active["entries"]],
        "proposed_pack_region": {
            "start": PACK_START, "end": PACK_END, "bytes": len(pack_region),
            "erased": all(byte == 0xFF for byte in pack_region),
            "sha256": hashlib.sha256(pack_region).hexdigest(),
        },
    }


def extract(image: bytes, report: dict[str, object], output: Path) -> None:
    output.mkdir(parents=True, exist_ok=False)
    for slot in range(2):
        start = LIBRARY_OFFSET + slot * CATALOG_BYTES
        (output / f"catalog-{slot}.bin").write_bytes(image[start:start + CATALOG_BYTES])
    for entry in report["allocations"]:
        start, length = int(entry["flash_offset"]), int(entry["length"])
        (output / f"{entry['id']}.sna").write_bytes(image[start:start + length])
    (output / "validation.json").write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")


def make_self_test_image() -> bytes:
    image = bytearray(b"\xFF" * FLASH_BYTES)
    image[:LIBRARY_OFFSET] = b"\xA5" * LIBRARY_OFFSET
    image[E10_OFFSET:] = b"\x5A" * (FLASH_BYTES - E10_OFFSET)
    entries = []
    for index, game_id in enumerate(EXPECTED_IDS):
        relative = DATA_START + index * 17 * 4096
        payload = bytearray(SNAPSHOT_BYTES)
        payload[:8] = b"MV - SNA"; payload[16] = 3; payload[107] = 64
        for at in range(256, len(payload)):
            payload[at] = (at + index) & 0xFF
        image[LIBRARY_OFFSET + relative:LIBRARY_OFFSET + relative + len(payload)] = payload
        descriptor = bytearray(80)
        descriptor[:len(game_id)] = game_id.encode("ascii")
        title = game_id.replace("_", " ").title().encode("ascii")
        descriptor[16:16 + len(title)] = title
        descriptor[56:60] = bytes((1, 64, 3, 0))
        struct.pack_into("<III", descriptor, 60, relative, len(payload), zlib.crc32(payload))
        struct.pack_into("<I", descriptor, 72, masked_crc(descriptor, 0, 80, 72))
        entries.append(descriptor)
    for slot, generation in enumerate((10, 11)):
        catalog = bytearray(b"\xFF" * CATALOG_BYTES)
        catalog[:8] = b"CPCLIB1\0"
        struct.pack_into("<HHIHHIII", catalog, 8, 1, 64, generation, len(entries), 80,
                         DATA_START, LIBRARY_BYTES, 0)
        struct.pack_into("<I", catalog, 32, 0x4D4D4F43)
        for index, descriptor in enumerate(entries):
            catalog[64 + index * 80:64 + (index + 1) * 80] = descriptor
        struct.pack_into("<I", catalog, 28, masked_crc(catalog, 0, 64, 28))
        start = LIBRARY_OFFSET + slot * CATALOG_BYTES
        image[start:start + CATALOG_BYTES] = catalog
    return bytes(image)


def self_test() -> None:
    image = make_self_test_image()
    report = validate(image, expected_ids=EXPECTED_IDS)
    if report["visible_games"] != [*BUILTIN_IDS, *EXPECTED_IDS]:
        raise ValidationError("self-test visible game list mismatch")
    corrupt = bytearray(image); corrupt[LIBRARY_OFFSET + DATA_START + 300] ^= 1
    try:
        validate(bytes(corrupt), expected_ids=EXPECTED_IDS)
    except ValidationError:
        print("PASS: 16 MiB backup, both catalogues, allocations, CRCs and corruption rejection")
        return
    raise ValidationError("self-test accepted a corrupted allocation")


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("image", nargs="?", type=Path)
    parser.add_argument("--expect-sha256")
    parser.add_argument("--expect-id", action="append", default=[])
    parser.add_argument("--extract-dir", type=Path)
    parser.add_argument("--self-test", action="store_true")
    args = parser.parse_args()
    try:
        if args.self_test:
            self_test(); return 0
        if args.image is None:
            raise ValidationError("a backup image is required unless --self-test is used")
        image = args.image.read_bytes()
        report = validate(image, args.expect_sha256, tuple(args.expect_id))
        if args.extract_dir:
            extract(image, report, args.extract_dir)
        print(json.dumps(report, indent=2))
    except (OSError, ValidationError) as error:
        print(f"error: {error}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
