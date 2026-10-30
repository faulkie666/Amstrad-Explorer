#!/usr/bin/env python3
"""Build and verify deterministic CPXPCK2 host-only library images.

This tool has no device transport.  It consumes validated CPC464 snapshots and
produces a variable-length image for format and capacity testing only.
"""

from __future__ import annotations

import argparse
import hashlib
import json
import struct
import sys
import zlib
from pathlib import Path


MAGIC = b"CPXPCK2\0"
VERSION = 2
HEADER_SIZE = 128
HEADER_SLOT_SIZE = 4096
RECORD_SIZE = 160
RECORDS_OFFSET = HEADER_SLOT_SIZE * 2
CODEC_SNA_HEADER_LZ4_RAM = 1
SNA_HEADER_BYTES = 256
HEADER = struct.Struct("<8sHHHHIIIIIIIII32s44s")
RECORD = struct.Struct("<32s64sBBBBBBBBIIIII32s4s")


def align(value: int, boundary: int) -> int:
    return (value + boundary - 1) & ~(boundary - 1)


def crc32(data: bytes) -> int:
    return zlib.crc32(data) & 0xFFFFFFFF


def fixed_text(value: str, size: int, field: str) -> bytes:
    encoded = value.encode("utf-8")
    if not encoded or len(encoded) >= size:
        raise ValueError(f"{field} must be 1..{size - 1} UTF-8 bytes")
    return encoded + bytes(size - len(encoded))


def load_lz4(vendor_dir: Path | None):
    if vendor_dir is not None:
        sys.path.insert(0, str(vendor_dir.resolve()))
    try:
        import lz4.block  # type: ignore
    except ImportError as error:
        raise RuntimeError("lz4.block is required; pass --vendor-dir") from error
    return lz4.block


def validate_sna(data: bytes) -> None:
    if len(data) != 65_792:
        raise ValueError(f"snapshot must be exactly 65792 bytes, got {len(data)}")
    if data[:8] != b"MV - SNA":
        raise ValueError("invalid SNA signature")
    version = data[0x10]
    if version not in (1, 2, 3):
        raise ValueError(f"unsupported SNA version {version}")
    if data[0x6B] != 64:
        raise ValueError(f"snapshot RAM is {data[0x6B]} KiB, expected 64")
    if version >= 2 and data[0x6D] != 0:
        raise ValueError(f"SNA machine identity {data[0x6D]} is not CPC464")


def make_header(slot: int, generation: int, count: int, records_offset: int,
                records_len: int,
                data_offset: int, image_len: int, records_crc: int,
                payload_crc: int, build_sha: bytes) -> bytes:
    values = [MAGIC, VERSION, HEADER_SIZE, RECORD_SIZE, slot, generation, count,
              records_offset, records_len, data_offset, image_len, records_crc,
              payload_crc, 0, build_sha, bytes(44)]
    raw = bytearray(HEADER.pack(*values))
    values[13] = crc32(raw)
    return HEADER.pack(*values)


def build(manifest_path: Path, output: Path, vendor_dir: Path | None) -> dict:
    lz4 = load_lz4(vendor_dir)
    manifest_raw = manifest_path.read_bytes()
    entries = json.loads(manifest_raw)
    if not isinstance(entries, list) or not entries:
        raise ValueError("manifest must be a non-empty JSON array")
    # The runtime menu is deliberately flash-backed.  Store records in the
    # order it presents them so it never needs an SRAM-resident sort table.
    entries = sorted(entries, key=lambda item: (
        {"Arcade": 1, "Simulation": 2}[item["category"]],
        item.get("letter", item["title"][0]).upper(),
        item["title"].casefold(), item["id"]))
    ids = [item["id"] for item in entries]
    if len(ids) != len(set(ids)):
        raise ValueError("duplicate game ID")

    records_len = len(entries) * RECORD_SIZE
    records_a_offset = RECORDS_OFFSET
    records_b_offset = align(records_a_offset + records_len, HEADER_SLOT_SIZE)
    data_offset = align(records_b_offset + records_len, HEADER_SLOT_SIZE)
    cursor = data_offset
    records = bytearray()
    payloads: list[tuple[int, bytes]] = []
    report_entries = []
    for item in entries:
        game_id = item["id"]
        if not game_id.isascii() or not game_id.replace("_", "").isalnum():
            raise ValueError(f"invalid stable game ID {game_id!r}")
        category = {"Arcade": 1, "Simulation": 2}[item["category"]]
        letter = item.get("letter", item["title"][0]).upper()
        if len(letter) != 1 or not ("A" <= letter <= "Z"):
            raise ValueError(f"invalid A-Z letter {letter!r}")
        snapshot_path = (manifest_path.parent / item["snapshot"]).resolve()
        raw = snapshot_path.read_bytes()
        validate_sna(raw)
        compressed_ram = lz4.compress(raw[SNA_HEADER_BYTES:], mode="high_compression",
                                      compression=12, store_size=False)
        compressed = raw[:SNA_HEADER_BYTES] + compressed_ram
        if (raw[:SNA_HEADER_BYTES] +
                lz4.decompress(compressed_ram,
                               uncompressed_size=len(raw) - SNA_HEADER_BYTES)) != raw:
            raise RuntimeError(f"LZ4 round-trip failed for {game_id}")
        source_sha = bytes.fromhex(item["source_sha256"])
        if len(source_sha) != 32:
            raise ValueError(f"invalid source SHA-256 for {game_id}")
        cursor = align(cursor, 16)
        control = int(item.get("control_profile", 0))
        if control < 0 or control > 8:
            raise ValueError(f"invalid control profile {control} for {game_id}")
        compatibility = int(item.get("compatibility", 1))
        record = RECORD.pack(
            fixed_text(game_id, 32, "id"), fixed_text(item["title"], 64, "title"),
            category, ord(letter), 0, 64, CODEC_SNA_HEADER_LZ4_RAM, compatibility,
            control, 0, cursor, len(compressed), len(raw), crc32(raw),
            crc32(compressed), source_sha, bytes(4))
        records.extend(record)
        payloads.append((cursor, compressed))
        report_entries.append({"id": game_id, "raw": len(raw),
                               "compressed": len(compressed), "offset": cursor,
                               "ratio": len(compressed) / len(raw)})
        cursor += len(compressed)

    image_len = align(cursor, 16)
    image = bytearray(image_len)
    image[records_a_offset:records_a_offset + records_len] = records
    image[records_b_offset:records_b_offset + records_len] = records
    for offset, payload in payloads:
        image[offset:offset + len(payload)] = payload
    payload_crc = crc32(image[data_offset:image_len])
    build_sha = hashlib.sha256(manifest_raw).digest()
    image[:HEADER_SIZE] = make_header(0, 1, len(entries), records_a_offset,
                                      records_len, data_offset, image_len,
                                      crc32(records), payload_crc, build_sha)
    image[HEADER_SLOT_SIZE:HEADER_SLOT_SIZE + HEADER_SIZE] = make_header(
        1, 1, len(entries), records_b_offset, records_len, data_offset, image_len,
        crc32(records), payload_crc, build_sha)
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_bytes(image)
    result = inspect(output, vendor_dir)
    result["entries_detail"] = report_entries
    return result


def parse_header(raw: bytes, expected_slot: int) -> dict:
    values = list(HEADER.unpack(raw))
    if values[0] != MAGIC or values[1:4] != [VERSION, HEADER_SIZE, RECORD_SIZE]:
        raise ValueError(f"invalid header in slot {expected_slot}")
    if values[4] != expected_slot:
        raise ValueError(f"slot identity mismatch in slot {expected_slot}")
    stored_crc = values[13]
    values[13] = 0
    if crc32(HEADER.pack(*values)) != stored_crc:
        raise ValueError(f"header CRC mismatch in slot {expected_slot}")
    return {"generation": values[5], "count": values[6], "records_offset": values[7],
            "records_len": values[8], "data_offset": values[9],
            "image_len": values[10], "records_crc": values[11],
            "payload_crc": values[12], "build_sha256": values[14].hex()}


def inspect(image_path: Path, vendor_dir: Path | None) -> dict:
    lz4 = load_lz4(vendor_dir)
    image = image_path.read_bytes()
    candidates = []
    header_errors = []
    for slot, start in enumerate((0, HEADER_SLOT_SIZE)):
        try:
            candidate = parse_header(image[start:start + HEADER_SIZE], slot)
            if candidate["image_len"] != len(image):
                raise ValueError("image length mismatch")
            records = image[candidate["records_offset"]:
                            candidate["records_offset"] + candidate["records_len"]]
            if (len(records) != candidate["records_len"] or
                    crc32(records) != candidate["records_crc"]):
                raise ValueError("record table CRC mismatch")
            if candidate["records_len"] != candidate["count"] * RECORD_SIZE:
                raise ValueError("record count/length mismatch")
            if crc32(image[candidate["data_offset"]:]) != candidate["payload_crc"]:
                raise ValueError("payload-area CRC mismatch")
            candidates.append((slot, candidate, records))
        except (ValueError, struct.error) as error:
            header_errors.append(f"slot {slot}: {error}")
    if not candidates:
        raise ValueError("no valid A/B header: " + "; ".join(header_errors))
    candidates.sort(key=lambda item: (item[1]["generation"], item[0]), reverse=True)
    _, h, records = candidates[0]
    if len(candidates) == 2 and candidates[0][1]["generation"] == candidates[1][1]["generation"]:
        common = ("count", "records_len", "data_offset", "image_len", "records_crc",
                  "payload_crc", "build_sha256")
        if any(candidates[0][1][key] != candidates[1][1][key] for key in common):
            raise ValueError("equally generated A/B catalogs disagree")
    ids: set[str] = set()
    previous_menu_key = None
    ranges = []
    entries = []
    for index in range(h["count"]):
        fields = RECORD.unpack_from(records, index * RECORD_SIZE)
        game_id = fields[0].split(b"\0", 1)[0].decode("ascii")
        title = fields[1].split(b"\0", 1)[0].decode("utf-8")
        if game_id in ids:
            raise ValueError("duplicate game ID in image")
        ids.add(game_id)
        category, letter, machine, ram, codec = fields[2:7]
        menu_key = (category, letter, title.casefold(), game_id)
        if previous_menu_key is not None and menu_key < previous_menu_key:
            raise ValueError("record table is not in category/A-Z/title order")
        previous_menu_key = menu_key
        offset, compressed_len, raw_len, raw_crc, compressed_crc = fields[10:15]
        if category not in (1, 2) or not 65 <= letter <= 90 or machine != 0 or ram != 64:
            raise ValueError(f"invalid compatibility metadata for {game_id}")
        if (codec != CODEC_SNA_HEADER_LZ4_RAM or compressed_len <= SNA_HEADER_BYTES or
                offset < h["data_offset"] or offset + compressed_len > len(image)):
            raise ValueError(f"invalid payload bounds/codec for {game_id}")
        ranges.append((offset, offset + compressed_len, game_id))
        compressed = image[offset:offset + compressed_len]
        if crc32(compressed) != compressed_crc:
            raise ValueError(f"compressed CRC mismatch for {game_id}")
        raw = (compressed[:SNA_HEADER_BYTES] +
               lz4.decompress(compressed[SNA_HEADER_BYTES:],
                              uncompressed_size=raw_len - SNA_HEADER_BYTES))
        if len(raw) != raw_len or crc32(raw) != raw_crc:
            raise ValueError(f"decoded integrity mismatch for {game_id}")
        validate_sna(raw)
        entries.append({"id": game_id, "title": title, "compressed": compressed_len,
                        "raw": raw_len, "source_sha256": fields[15].hex()})
    ranges.sort()
    for previous, current in zip(ranges, ranges[1:]):
        if previous[1] > current[0]:
            raise ValueError(f"overlapping payloads: {previous[2]} and {current[2]}")
    return {"format": "CPXPCK2", "version": VERSION, "entries": len(entries),
            "image_bytes": len(image), "sha256": hashlib.sha256(image).hexdigest(),
            "valid_headers": len(candidates), "header_errors": header_errors,
            "records_crc32": f"{h['records_crc']:08x}",
            "payload_crc32": f"{h['payload_crc']:08x}", "games": entries}


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--vendor-dir", type=Path)
    sub = parser.add_subparsers(dest="command", required=True)
    make = sub.add_parser("build")
    make.add_argument("manifest", type=Path)
    make.add_argument("output", type=Path)
    check = sub.add_parser("inspect")
    check.add_argument("image", type=Path)
    args = parser.parse_args()
    try:
        result = (build(args.manifest, args.output, args.vendor_dir)
                  if args.command == "build" else inspect(args.image, args.vendor_dir))
        print(json.dumps(result, indent=2))
        return 0
    except (OSError, ValueError, RuntimeError, json.JSONDecodeError) as error:
        print(f"error: {error}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
