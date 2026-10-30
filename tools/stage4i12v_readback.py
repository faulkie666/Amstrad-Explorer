#!/usr/bin/env python3
"""Read-only, no-retry verification of an already staged CPXPCK2 transaction."""

from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path
import sys

sys.path.insert(0, str(Path(__file__).resolve().parent))

import cpc_game
import validate_flash_backup


WINDOW_BYTES = 4096
EXPECTED_PACK_SHA256 = "27dc071d9360b08e1248e0e90aff484c648cc95bb2d436dc7aa91b1ef1e28fbe"
EXPECTED_BACKUP_SHA256 = "c0a6c28983cd092b2294613c163854b9098a5505068da9d57a7d986a360d74c1"
TEST_BYTES = b"CPC Explorer Stage 4I.12V.1 evidence path\x00\xff\x5a\xa5"


def sha256(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def save_binary(path: Path, data: bytes) -> str:
    path.write_bytes(data)
    saved = path.read_bytes()
    if saved != data:
        raise RuntimeError(f"saved evidence mismatch: {path}")
    return sha256(saved)


def prepare_evidence_dir(path: Path, resume: bool = False) -> None:
    path.mkdir(parents=True, exist_ok=True)
    if not path.is_dir():
        raise RuntimeError(f"evidence destination is not a directory: {path}")
    existing = list(path.iterdir())
    if existing and not resume:
        raise RuntimeError(f"evidence directory is not empty: {path}")
    probe = path / ".stage4i12v1-write-test.tmp"
    save_binary(probe, TEST_BYTES)
    probe.unlink()
    if probe.exists():
        raise RuntimeError("temporary evidence-path test file was not removed")


def write_manifest(output: Path, manifest: list[dict[str, object]]) -> None:
    target = output / "readback-manifest.json"
    temporary = output / ".readback-manifest.tmp"
    temporary.write_text(json.dumps({"ranges": manifest}, indent=2) + "\n",
                         encoding="utf-8")
    temporary.replace(target)


def evidence_name(name: str, index: int, offset: int, count: int) -> str:
    return f"{name}-{index:03d}-{offset:06x}-{offset + count - 1:06x}.bin"


def capture_windows(port, output: Path, name: str, start: int, length: int,
                    expected: bytes, manifest: list[dict[str, object]]) -> bytes:
    blocks: list[bytes] = []
    for index, offset in enumerate(range(start, start + length, WINDOW_BYTES)):
        count = min(WINDOW_BYTES, start + length - offset)
        wanted = expected[offset - start:offset - start + count]
        filename = evidence_name(name, index, offset, count)
        path = output / filename
        if path.exists():
            block = path.read_bytes()
        else:
            try:
                block = cpc_game.read_migration_range(port, offset, count)
            except Exception as error:
                raise RuntimeError(
                    f"{name} window={index} offset=0x{offset:06x} "
                    f"bytes={count}: {error}") from error
        if len(block) != count:
            raise RuntimeError(f"{name} window {index} length mismatch")
        if block != wanted:
            first = next(at for at, pair in enumerate(zip(block, wanted))
                         if pair[0] != pair[1])
            raise RuntimeError(
                f"{name} window {index} content mismatch at 0x{offset + first:06x}")
        digest = sha256(block) if path.exists() else save_binary(path, block)
        manifest.append({"name": name, "index": index, "offset": offset,
                         "bytes": count, "sha256": digest, "file": filename})
        write_manifest(output, manifest)
        blocks.append(block)
    return b"".join(blocks)


def capture_catalogues(port, output: Path, manifest: list[dict[str, object]],
                       known_hashes: dict[str, str]) -> bytes:
    blocks: list[bytes] = []
    for index, offset in enumerate(range(0x400000, 0x408000, WINDOW_BYTES)):
        filename = evidence_name("catalog", index, offset, WINDOW_BYTES)
        path = output / filename
        if path.exists():
            block = path.read_bytes()
            if filename not in known_hashes or sha256(block) != known_hashes[filename]:
                raise RuntimeError(f"unmanifested or changed catalogue evidence: {filename}")
        else:
            try:
                block = cpc_game.read_migration_range(port, offset, WINDOW_BYTES)
            except Exception as error:
                raise RuntimeError(
                    f"catalog window={index} offset=0x{offset:06x} "
                    f"bytes={WINDOW_BYTES}: {error}") from error
        if len(block) != WINDOW_BYTES:
            raise RuntimeError(f"catalogue window {index} length mismatch")
        digest = sha256(block) if path.exists() else save_binary(path, block)
        manifest.append({"name": "catalog", "index": index, "offset": offset,
                         "bytes": WINDOW_BYTES, "sha256": digest, "file": filename})
        write_manifest(output, manifest)
        blocks.append(block)
    return b"".join(blocks)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--port", required=True)
    parser.add_argument("--evidence-dir", required=True, type=Path)
    parser.add_argument("--pack", required=True, type=Path)
    parser.add_argument("--backup", required=True, type=Path)
    parser.add_argument("--resume", action="store_true")
    args = parser.parse_args()

    # Complete the host filesystem and immutable-input gates before importing
    # PySerial or opening the physical port.
    prepare_evidence_dir(args.evidence_dir, resume=args.resume)
    pack = args.pack.read_bytes()
    backup = args.backup.read_bytes()
    if len(pack) != cpc_game.MIGRATION_PACK_BYTES or sha256(pack) != EXPECTED_PACK_SHA256:
        raise RuntimeError("approved pack identity mismatch")
    if len(backup) != validate_flash_backup.FLASH_BYTES or sha256(backup) != EXPECTED_BACKUP_SHA256:
        raise RuntimeError("Stage 4I.10 backup identity mismatch")

    expected_destination = (cpc_game.staged_image(pack) +
                            bytes([0xFF]) * (0x2C000 - len(pack)))
    expected_payload = backup[0x408000:0x43B000]
    prior_manifest = args.evidence_dir / "readback-manifest.json"
    known_hashes: dict[str, str] = {}
    if prior_manifest.exists():
        prior = json.loads(prior_manifest.read_text(encoding="utf-8"))
        known_hashes = {item["file"]: item["sha256"] for item in prior.get("ranges", [])}
    manifest: list[dict[str, object]] = []

    serial, _ = cpc_game.serial_module()
    with serial.Serial(args.port, 115200, timeout=4, write_timeout=2) as port:
        ready = cpc_game.require_migration_device(port)
        port.write(cpc_game.frame(8, 0))
        status = cpc_game.read_protocol_line(port, (b"CPCU1 MIGRATION STATUS",))
        if not all(token in status for token in
                   (b"state=verified", b"received=179360", b"error=none")):
            raise RuntimeError(f"transaction is not verified: {status.decode(errors='replace')}")
        destination = capture_windows(port, args.evidence_dir, "destination",
                                      0x43B000, 0x2C000, expected_destination, manifest)
        catalogues = capture_catalogues(port, args.evidence_dir, manifest, known_hashes)
        payload = capture_windows(port, args.evidence_dir, "legacy-payload",
                                  0x408000, 0x33000, expected_payload, manifest)

    legacy = catalogues + payload
    destination_sha = save_binary(
        args.evidence_dir / "staged-destination-43b000-466fff.bin", destination)
    catalog_sha = save_binary(
        args.evidence_dir / "post-stage-catalogues-400000-407fff.bin", catalogues)
    payload_sha = save_binary(
        args.evidence_dir / "legacy-payload-408000-43afff.bin", payload)
    legacy_sha = save_binary(
        args.evidence_dir / "post-stage-legacy-400000-43afff.bin", legacy)

    composite = bytearray(backup)
    composite[0x400000:0x43B000] = legacy
    composite[0x43B000:0x467000] = destination
    validation = validate_flash_backup.validate(bytes(composite))
    for catalog in validation["catalogues"]:
        ids = [entry["id"] for entry in catalog["entries"]]
        if ids != ["manic_miner", "dizzy"] or catalog["high_water"] != 0x67000:
            raise RuntimeError(
                f"unexpected catalogue slot {catalog['slot']}: "
                f"ids={ids} high_water={catalog['high_water']}")

    result = {
        "device_ready": ready.decode(), "migration_status": status.decode(),
        "approved_pack_bytes": len(pack), "approved_pack_sha256": sha256(pack),
        "staged_destination_bytes": len(destination),
        "staged_destination_sha256": destination_sha,
        "expected_staged_destination_sha256": sha256(expected_destination),
        "staged_image_bytes": len(pack),
        "staged_image_sha256": sha256(destination[:len(pack)]),
        "expected_staged_image_sha256": sha256(cpc_game.staged_image(pack)),
        "header_a_erased": destination[:256] == bytes([0xFF]) * 256,
        "header_b_erased": destination[4096:4352] == bytes([0xFF]) * 256,
        "staged_non_header_bytes_match_pack": (
            destination[256:4096] == pack[256:4096] and
            destination[4352:len(pack)] == pack[4352:]),
        "destination_padding_bytes": len(destination) - len(pack),
        "destination_padding_erased": all(value == 0xFF for value in
                                           destination[len(pack):]),
        "catalogue_bytes": len(catalogues), "catalogue_sha256": catalog_sha,
        "legacy_payload_bytes": len(payload), "legacy_payload_sha256": payload_sha,
        "legacy_payload_expected_sha256": sha256(expected_payload),
        "legacy_payload_exact": payload == expected_payload,
        "legacy_bytes": len(legacy), "legacy_sha256": legacy_sha,
        "catalogues": validation["catalogues"],
        "active_catalogue": validation["active_catalogue"],
        "visible_legacy_games": validation["visible_games"],
        "window_count": len(manifest), "window_bytes": WINDOW_BYTES,
        "protocol_read_bytes": cpc_game.MIGRATION_READ_BYTES,
    }
    write_manifest(args.evidence_dir, manifest)
    (args.evidence_dir / "validation.json").write_text(
        json.dumps(result, indent=2) + "\n", encoding="utf-8")
    print(json.dumps(result, indent=2))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
