#!/usr/bin/env python3
"""Deterministic CPC Explorer snapshot packager and Windows serial uploader."""

from __future__ import annotations

import argparse
import hashlib
import json
import re
import struct
import subprocess
import sys
import time
import zlib
from pathlib import Path

PACKAGE_MAGIC = b"CPCGAME1"
PACKAGE_VERSION = 1
PACKAGE_HEADER_BYTES = 16
DESCRIPTOR_BYTES = 80
PAYLOAD_OFFSET = PACKAGE_HEADER_BYTES + DESCRIPTOR_BYTES
SNA_BYTES = 256 + 65536
CHUNK_BYTES = 1024
MIGRATION_PAGE_BYTES = 256
# Keep each hex response below the device's bounded USB-stdio burst size. The
# firmware accepts up to 128 bytes, but a physical 128-byte response was
# newline-terminated after only 77 bytes. A 64-byte request remains strictly
# framed while avoiding that long sequence of non-blocking printf calls.
MIGRATION_READ_BYTES = 64
MIGRATION_PACK_OFFSET = 0x43B000
MIGRATION_PACK_BYTES = 179360
MIGRATION_BACKUP_OFFSET = 0x400000
MIGRATION_BACKUP_END = MIGRATION_PACK_OFFSET
CATALOGUE_BYTES = 16384
CATALOGUE_SLOTS_BYTES = 2 * CATALOGUE_BYTES
MIGRATION_PACK_CRC32 = 0xEC7FD1E9
MIGRATION_PACK_SHA256 = "27dc071d9360b08e1248e0e90aff484c648cc95bb2d436dc7aa91b1ef1e28fbe"
MIGRATION_FIRMWARES = {
    "28b9c675f099a43ef75bd9ddfa8beb5d7936293fc910a1a129588940a75c8114":
        b"fw=0.4.20-stage4i8migrationexp",
    "2b8860deb112f0ab475ae99d40f38dcdd74c397862843edba6e14c985f6a32c6":
        b"fw=0.4.21-stage4j1qwstexp",
    "fe03344d1a02f804e2db2ea5677939303d778320352bd053bbd22b3cc21ceb53":
        b"fw=0.4.23-stage4j3migrationfix",
}
SNA_MACHINE_OFFSET = 0x6D
SNA_MACHINE_NAMES = {
    0: "CPC464",
    1: "CPC664",
    2: "CPC6128",
    3: "unknown CPC",
    4: "CPC6128 Plus",
    5: "CPC464 Plus",
    6: "GX4000",
}
CONTROL_PROFILES = {
    "none": 0,
    "qw-shift": 1,
    "zx-space": 2,
    "joystick-fire": 3,
    "fruity-legacy": 4,
    "thrust-five-key": 5,
    "cursor-qaop": 6,
    "knight-tyme": 7,
    "wasd-space": 8,
}
CONTROL_PROFILE_NAMES = {value: name for name, value in CONTROL_PROFILES.items()}


def validate_snapshot(data: bytes) -> int:
    if len(data) != SNA_BYTES:
        raise ValueError(f"only standalone 64 KiB SNA files are supported; got {len(data)} bytes")
    if data[:8] != b"MV - SNA":
        raise ValueError("missing CPC SNA signature")
    version = data[16]
    if version not in (1, 2, 3):
        raise ValueError(f"unsupported CPC SNA format version {version}")
    if version >= 2:
        machine = data[SNA_MACHINE_OFFSET]
        if machine != 0:
            name = SNA_MACHINE_NAMES.get(machine, f"machine identity {machine}")
            raise ValueError(
                f"unsupported SNA machine: {name} (CPC464 identity 0 required)")
    dump_kib = data[107] | (data[108] << 8)
    if dump_kib != 64:
        raise ValueError(f"unsupported RAM dump: {dump_kib} KiB (CPC464 64 KiB required)")
    return version


def checked_text(value: str, field: str, maximum: int) -> bytes:
    try:
        encoded = value.encode("ascii")
    except UnicodeEncodeError as error:
        raise ValueError(f"{field} must be printable ASCII") from error
    if not encoded or len(encoded) >= maximum or any(byte < 0x20 or byte > 0x7E for byte in encoded):
        raise ValueError(f"{field} must contain 1..{maximum - 1} printable ASCII characters")
    return encoded + bytes(maximum - len(encoded))


def package_snapshot(data: bytes, game_id: str, title: str, controls: str = "none") -> bytes:
    version = validate_snapshot(data)
    if not re.fullmatch(r"[a-z0-9][a-z0-9_-]{0,14}", game_id):
        raise ValueError("game ID must be 1..15 lowercase ASCII letters/digits/_/-")
    descriptor = bytearray(DESCRIPTOR_BYTES)
    descriptor[0:16] = checked_text(game_id, "game ID", 16)
    descriptor[16:56] = checked_text(title, "title", 40)
    if controls not in CONTROL_PROFILES:
        raise ValueError(f"unsupported control profile {controls!r}")
    descriptor[56:60] = bytes((1, 64, version, CONTROL_PROFILES[controls]))
    struct.pack_into("<III", descriptor, 60, PAYLOAD_OFFSET, len(data), zlib.crc32(data))
    struct.pack_into("<I", descriptor, 72, zlib.crc32(descriptor))
    total = PAYLOAD_OFFSET + len(data)
    header = struct.pack("<8sHHI", PACKAGE_MAGIC, PACKAGE_VERSION, PAYLOAD_OFFSET, total)
    return header + descriptor + data


def parse_package(data: bytes) -> dict[str, object]:
    if len(data) < PAYLOAD_OFFSET:
        raise ValueError("truncated package")
    magic, version, header_bytes, total = struct.unpack_from("<8sHHI", data)
    if magic != PACKAGE_MAGIC or version != PACKAGE_VERSION or header_bytes != PAYLOAD_OFFSET or total != len(data):
        raise ValueError("invalid package header")
    descriptor = bytearray(data[PACKAGE_HEADER_BYTES:PAYLOAD_OFFSET])
    expected_descriptor_crc = struct.unpack_from("<I", descriptor, 72)[0]
    descriptor[72:76] = bytes(4)
    if zlib.crc32(descriptor) != expected_descriptor_crc:
        raise ValueError("descriptor checksum mismatch")
    offset, length, expected_payload_crc = struct.unpack_from("<III", descriptor, 60)
    if offset != PAYLOAD_OFFSET or length != SNA_BYTES or offset + length != len(data):
        raise ValueError("invalid package payload bounds")
    payload = data[offset:offset + length]
    if zlib.crc32(payload) != expected_payload_crc:
        raise ValueError("snapshot checksum mismatch")
    snapshot_version = validate_snapshot(payload)
    if descriptor[59] not in CONTROL_PROFILE_NAMES:
        raise ValueError(f"unsupported control profile value {descriptor[59]}")
    return {
        "id": bytes(descriptor[0:16]).split(b"\0", 1)[0].decode("ascii"),
        "title": bytes(descriptor[16:56]).split(b"\0", 1)[0].decode("ascii"),
        "machine": "CPC464",
        "ram_kib": descriptor[57],
        "snapshot_version": snapshot_version,
        "controls": CONTROL_PROFILE_NAMES[descriptor[59]],
        "payload_bytes": length,
        "payload_crc32": f"{expected_payload_crc:08x}",
        "package_crc32": f"{zlib.crc32(data):08x}",
    }


def validate_launcher_visibility(package: bytes) -> dict[str, object]:
    """Fail before serial access if the installed A-Z launcher cannot show a title."""
    info = parse_package(package)
    title = str(info["title"])
    if not title or not ("A" <= title[0] <= "Z" or "a" <= title[0] <= "z"):
        raise ValueError(
            f"launcher visibility requires an alphabetic title; {title!r} would be hidden")
    return info


def frame(command: int, sequence: int, payload: bytes = b"") -> bytes:
    if len(payload) > CHUNK_BYTES:
        raise ValueError("protocol payload exceeds 1024 bytes")
    return struct.pack("<4sBBHII", b"CPCU", 1, command, len(payload), sequence,
                       zlib.crc32(payload)) + payload


def serial_module():
    try:
        import serial  # type: ignore
        from serial.tools import list_ports  # type: ignore
    except ImportError as error:
        raise RuntimeError("USB commands require pyserial (py -m pip install pyserial)") from error
    return serial, list_ports


def detect_ports() -> list[str]:
    _, list_ports = serial_module()
    return [port.device for port in list_ports.comports()
            if port.vid == 0x2E8A and port.pid == 0x0009]


def selected_port(requested: str | None) -> str:
    if requested:
        return requested
    ports = detect_ports()
    if len(ports) != 1:
        raise RuntimeError(f"expected one CPC Explorer port, found {ports}")
    return ports[0]


def read_protocol_line(port, expected: tuple[bytes, ...]) -> bytes:
    serial_timeout = float(port.timeout or 0)
    deadline = time.monotonic() + max(2.0, min(10.0, serial_timeout * 2.0))
    pending = bytearray()
    complete_lines = 0
    while time.monotonic() < deadline:
        fragment = port.read_until(b"\n", 4096 - len(pending))
        if fragment:
            pending.extend(fragment)
        if len(pending) >= 4096 and not pending.endswith(b"\n"):
            raise RuntimeError("CPC Explorer protocol response exceeds 4095 bytes")
        if not pending.endswith(b"\n"):
            continue
        line = bytes(pending).strip()
        pending.clear()
        complete_lines += 1
        if not line:
            continue
        if line.startswith(b"CPCU1 ERROR"):
            raise RuntimeError(line.decode("ascii", "replace"))
        if any(line.startswith(prefix) for prefix in expected):
            return line
        if complete_lines >= 32:
            break
    raise RuntimeError("timed out waiting for CPC Explorer protocol response")


def query(port_name: str, list_games: bool) -> None:
    serial, _ = serial_module()
    with serial.Serial(port_name, 115200, timeout=2, write_timeout=2) as port:
        port.reset_input_buffer()
        port.write(frame(1, 0))
        print(read_protocol_line(port, (b"CPCU1 READY",)).decode())
        port.write(frame(6, 0))
        while True:
            line = read_protocol_line(port, (b"CPCU1 STATUS", b"CPCU1 GAME", b"CPCU1 END"))
            if line.startswith(b"CPCU1 STATUS") or list_games:
                print(line.decode())
            if line == b"CPCU1 END":
                break


def upload(package: bytes, port_name: str, confirmation: str) -> None:
    if confirmation != "UPLOAD":
        raise ValueError("serial upload requires --confirm UPLOAD")
    package_info = validate_launcher_visibility(package)
    serial, _ = serial_module()
    with serial.Serial(port_name, 115200, timeout=2, write_timeout=2) as port:
        port.reset_input_buffer()
        port.write(frame(1, 0))  # HELLO
        print(read_protocol_line(port, (b"CPCU1 READY",)).decode())
        begin = struct.pack("<II", len(package), zlib.crc32(package))
        port.write(frame(2, 0, begin))
        read_protocol_line(port, (b"CPCU1 BEGIN OK",))
        sent = 0
        for sequence, offset in enumerate(range(0, len(package), CHUNK_BYTES)):
            chunk = package[offset:offset + CHUNK_BYTES]
            port.write(frame(3, sequence, chunk))
            read_protocol_line(port, (f"CPCU1 ACK {sequence} ".encode(),))
            sent += len(chunk)
            print(f"\r{sent}/{len(package)} bytes ({sent * 100 // len(package)}%)", end="", flush=True)
        print()
        port.write(frame(4, 0))
        response = read_protocol_line(port, (b"CPCU1 COMMITTED",))
        match = re.search(rb"\bcrc=([0-9a-fA-F]{8})\b", response)
        expected_payload_crc = package_info["payload_crc32"]
        if match is None or match.group(1).decode("ascii").lower() != expected_payload_crc:
            raise RuntimeError("device readback checksum does not match package payload")
        print(response.decode())


def remove_game(game_id: str, port_name: str, confirmation: str) -> None:
    if confirmation != "REMOVE":
        raise ValueError("game removal requires --confirm REMOVE")
    if not re.fullmatch(r"[a-z0-9][a-z0-9_-]{0,14}", game_id):
        raise ValueError("game ID must be 1..15 lowercase ASCII letters/digits/_/-")
    if game_id in ("basic", "locomotive_basic", "knight_tyme"):
        raise ValueError("built-in BASIC and Knight Tyme entries cannot be removed")
    serial, _ = serial_module()
    with serial.Serial(port_name, 115200, timeout=5, write_timeout=2) as port:
        port.reset_input_buffer()
        port.write(frame(1, 0))
        print(read_protocol_line(port, (b"CPCU1 READY",)).decode())
        port.write(frame(7, 0, b"REMOVE\0" + game_id.encode("ascii")))
        response = read_protocol_line(port, (b"CPCU1 REMOVED",))
        if f"id={game_id}".encode() not in response:
            raise RuntimeError("device removal acknowledgement ID mismatch")
        print(response.decode())


def validate_migration_files(pack: bytes, firmware: bytes, expected_pack: str,
                             expected_firmware: str) -> None:
    pack_sha = hashlib.sha256(pack).hexdigest()
    firmware_sha = hashlib.sha256(firmware).hexdigest()
    if (len(pack) != MIGRATION_PACK_BYTES or zlib.crc32(pack) != MIGRATION_PACK_CRC32 or
            pack_sha != MIGRATION_PACK_SHA256 or expected_pack.lower() != pack_sha):
        raise ValueError("migration pack identity/size/CRC/SHA-256 mismatch")
    if firmware_sha not in MIGRATION_FIRMWARES or expected_firmware.lower() != firmware_sha:
        raise ValueError("approved migration firmware SHA-256 mismatch")


def migration_status(port_name: str) -> None:
    serial, _ = serial_module()
    with serial.Serial(port_name, 115200, timeout=3, write_timeout=2) as port:
        port.reset_input_buffer()
        port.write(frame(1, 0))
        print(read_protocol_line(port, (b"CPCU1 READY",)).decode())
        port.write(frame(9, 0))
        print(read_protocol_line(port, (b"CPCU1 MIGRATION LAYOUT",)).decode())
        port.write(frame(8, 0))
        print(read_protocol_line(port, (b"CPCU1 MIGRATION STATUS",)).decode())


def read_migration_range(port, offset: int, length: int) -> bytes:
    result = bytearray()
    for chunk_index, physical in enumerate(range(offset, offset + length, MIGRATION_READ_BYTES)):
        count = min(MIGRATION_READ_BYTES, offset + length - physical)
        port.write(frame(15, 0, struct.pack("<II", physical, count)))
        line = read_protocol_line(port, (b"CPCU1 MIGRATION DATA ",))
        parts = line.split(b" ", 4)
        if len(parts) != 5 or int(parts[3]) != physical:
            actual_offset = parts[3].decode("ascii", "replace") if len(parts) > 3 else "missing"
            raise RuntimeError(
                f"migration readback offset mismatch at chunk={chunk_index}: "
                f"expected={physical} actual={actual_offset}")
        try:
            block = bytes.fromhex(parts[4].decode("ascii"))
        except (UnicodeDecodeError, ValueError) as error:
            raise RuntimeError(
                f"migration readback corrupt hex at chunk={chunk_index} offset=0x{physical:06x}") from error
        if len(block) != count:
            raise RuntimeError(
                f"migration readback length mismatch at chunk={chunk_index} "
                f"offset=0x{physical:06x}: expected={count} actual={len(block)} "
                f"line_bytes={len(line)}")
        result.extend(block)
    return bytes(result)


def backup_legacy_library(port_name: str, output: Path) -> None:
    serial, _ = serial_module()
    with serial.Serial(port_name, 115200, timeout=4, write_timeout=2) as port:
        port.reset_input_buffer()
        port.write(frame(1, 0))
        ready = read_protocol_line(port, (b"CPCU1 READY",))
        if b"flash=EF-40-18" not in ready or b"capacity=16777216" not in ready:
            raise RuntimeError("unexpected physical flash identity/capacity")
        data = read_migration_range(port, MIGRATION_BACKUP_OFFSET,
                                    MIGRATION_BACKUP_END - MIGRATION_BACKUP_OFFSET)
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_bytes(data)
    print(f"wrote {output} bytes={len(data)} sha256={hashlib.sha256(data).hexdigest()}")


def masked_crc32(data: bytes, offset: int, length: int, field: int) -> int:
    block = bytearray(data[offset:offset + length])
    block[field:field + 4] = bytes(4)
    return zlib.crc32(block)


def parse_catalogue_slots(data: bytes) -> list[dict[str, object]]:
    if len(data) != CATALOGUE_SLOTS_BYTES:
        raise ValueError("catalogue evidence must contain exactly two 16 KiB slots")
    slots = []
    for slot in range(2):
        base = slot * CATALOGUE_BYTES
        catalog = data[base:base + CATALOGUE_BYTES]
        if catalog[:8] != b"CPCLIB1\0" or struct.unpack_from("<HH", catalog, 8) != (1, 64):
            raise ValueError(f"catalogue slot {slot} identity is invalid")
        generation = struct.unpack_from("<I", catalog, 12)[0]
        count, descriptor_bytes = struct.unpack_from("<HH", catalog, 16)
        high_water = struct.unpack_from("<I", catalog, 40)[0]
        if count > 192 or descriptor_bytes != 80 or \
                struct.unpack_from("<I", catalog, 32)[0] != 0x4D4D4F43 or \
                masked_crc32(catalog, 0, 64, 28) != struct.unpack_from("<I", catalog, 28)[0]:
            raise ValueError(f"catalogue slot {slot} header is invalid")
        entries = []
        seen = set()
        for index in range(count):
            at = 64 + index * 80
            descriptor = catalog[at:at + 80]
            if len(descriptor) != 80 or masked_crc32(descriptor, 0, 80, 72) != \
                    struct.unpack_from("<I", descriptor, 72)[0]:
                raise ValueError(f"catalogue slot {slot} descriptor {index} CRC is invalid")
            game_id = descriptor[:16].split(b"\0", 1)[0].decode("ascii")
            title = descriptor[16:56].split(b"\0", 1)[0].decode("ascii")
            data_offset, length, payload_crc = struct.unpack_from("<III", descriptor, 60)
            if game_id in seen or length != SNA_BYTES or data_offset < 0x8000 or \
                    data_offset + 17 * 4096 > 0xBF0000:
                raise ValueError(f"catalogue slot {slot} descriptor {index} is invalid")
            seen.add(game_id)
            entries.append({"index": index, "id": game_id, "title": title,
                            "data_offset": data_offset, "length": length,
                            "payload_crc32": f"{payload_crc:08x}"})
        slots.append({"slot": slot, "generation": generation, "count": count,
                      "high_water": high_water, "entries": entries,
                      "sha256": hashlib.sha256(catalog).hexdigest()})
    return slots


def audit_catalogues(port_name: str, output: Path) -> None:
    serial, _ = serial_module()
    with serial.Serial(port_name, 115200, timeout=4, write_timeout=2) as port:
        port.reset_input_buffer()
        port.write(frame(1, 0))
        ready = read_protocol_line(port, (b"CPCU1 READY",))
        if b"fw=0.4.28-stage4k7migrationrecoverfix" not in ready or \
                b"flash=EF-40-18" not in ready or b"capacity=16777216" not in ready:
            raise RuntimeError("unexpected device identity for catalogue audit")
        data = read_migration_range(port, MIGRATION_BACKUP_OFFSET, CATALOGUE_SLOTS_BYTES)
    slots = parse_catalogue_slots(data)
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_bytes(data)
    report = output.with_suffix(".json")
    report.write_text(json.dumps({"ready": ready.decode(), "slots": slots}, indent=2) + "\n",
                      encoding="utf-8")
    print(f"wrote {output} bytes={len(data)} sha256={hashlib.sha256(data).hexdigest()}")
    print(f"wrote {report}")


def require_migration_device(port, expected_firmware_sha: str | None = None) -> bytes:
    port.reset_input_buffer()
    port.write(frame(1, 0))
    ready = read_protocol_line(port, (b"CPCU1 READY",))
    accepted_identity = MIGRATION_FIRMWARES.get(expected_firmware_sha or "")
    if accepted_identity is None:
        accepted_identity = next((identity for identity in MIGRATION_FIRMWARES.values()
                                  if identity in ready), None)
    if (accepted_identity is None or accepted_identity not in ready or b"flash=EF-40-18" not in ready or
            b"capacity=16777216" not in ready):
        raise RuntimeError("unexpected firmware or physical flash identity/capacity")
    return ready


def staged_image(pack: bytes) -> bytes:
    expected = bytearray(pack)
    expected[0:256] = b"\xff" * 256
    expected[4096:4352] = b"\xff" * 256
    return bytes(expected)


def stage_library(pack: bytes, firmware: bytes, port_name: str | None, confirmation: str | None,
                  execute: bool, expected_pack: str, expected_firmware: str) -> None:
    validate_migration_files(pack, firmware, expected_pack, expected_firmware)
    print(f"pack: bytes={len(pack)} crc32={zlib.crc32(pack):08x} sha256={hashlib.sha256(pack).hexdigest()}")
    print(f"firmware: sha256={hashlib.sha256(firmware).hexdigest()}")
    if not execute:
        print("DRY RUN ONLY: no serial port opened and no device write attempted")
        return
    if confirmation != "STAGE" or not port_name:
        raise ValueError("physical staging requires --port, --execute and --confirm STAGE")
    serial, _ = serial_module()
    with serial.Serial(port_name, 115200, timeout=4, write_timeout=2) as port:
        print(require_migration_device(port, expected_firmware.lower()).decode())
        port.write(frame(9, 0))
        print(read_protocol_line(port, (b"CPCU1 MIGRATION LAYOUT",)).decode())
        begin = (b"MIGRATE\0" + struct.pack("<IIII", 1, MIGRATION_PACK_OFFSET,
                 len(pack), zlib.crc32(pack)) + bytes.fromhex(MIGRATION_PACK_SHA256))
        port.write(frame(10, 0, begin))
        print(read_protocol_line(port, (b"CPCU1 MIGRATION BEGIN OK",)).decode())
        for sequence, offset in enumerate(range(0, len(pack), MIGRATION_PAGE_BYTES)):
            page = pack[offset:offset + MIGRATION_PAGE_BYTES]
            port.write(frame(11, sequence, page))
            read_protocol_line(port, (f"CPCU1 MIGRATION ACK {sequence} ".encode(),))
        port.write(frame(12, 0))
        print(read_protocol_line(port, (b"CPCU1 MIGRATION VERIFIED",)).decode())
        if read_migration_range(port, MIGRATION_PACK_OFFSET, len(pack)) != staged_image(pack):
            raise RuntimeError("pre-activation readback mismatch; outcome is HOLD")
        port.write(frame(8, 0))
        status = read_protocol_line(port, (b"CPCU1 MIGRATION STATUS",))
        if b"state=verified" not in status:
            raise RuntimeError("staged transaction is not durably verified; outcome is HOLD")
        print(status.decode())
        print("STAGED AND VERIFIED: catalogue activation was not requested")


def activate_library(pack: bytes, port_name: str | None, confirmation: str | None,
                     execute: bool, expected_pack: str) -> None:
    if (len(pack) != MIGRATION_PACK_BYTES or zlib.crc32(pack) != MIGRATION_PACK_CRC32 or
            hashlib.sha256(pack).hexdigest() != MIGRATION_PACK_SHA256 or
            expected_pack.lower() != MIGRATION_PACK_SHA256):
        raise ValueError("migration pack identity/size/CRC/SHA-256 mismatch")
    if not execute:
        print("DRY RUN ONLY: activation not requested and no serial port opened")
        return
    if confirmation != "ACTIVATE" or not port_name:
        raise ValueError("physical activation requires --port, --execute and --confirm ACTIVATE")
    serial, _ = serial_module()
    with serial.Serial(port_name, 115200, timeout=4, write_timeout=2) as port:
        print(require_migration_device(port).decode())
        port.write(frame(8, 0))
        status = read_protocol_line(port, (b"CPCU1 MIGRATION STATUS",))
        if b"state=verified" not in status:
            raise RuntimeError("activation requires an independently verified staged transaction")
        if read_migration_range(port, MIGRATION_PACK_OFFSET, len(pack)) != staged_image(pack):
            raise RuntimeError("independent staged readback mismatch; activation refused")
        port.write(frame(13, 0))
        print(read_protocol_line(port, (b"CPCU1 MIGRATION ACTIVE",)).decode())
        active = read_migration_range(port, MIGRATION_PACK_OFFSET, len(pack))
        if hashlib.sha256(active).hexdigest() != MIGRATION_PACK_SHA256:
            raise RuntimeError("post-activation SHA-256 readback mismatch; outcome is HOLD")
        port.write(frame(8, 0))
        final_status = read_protocol_line(port, (b"CPCU1 MIGRATION STATUS",))
        if b"state=active" not in final_status:
            raise RuntimeError("activation did not establish durable active state")
        print(final_status.decode())
        print("independent active-image SHA-256: PASS")


def migrate_library(pack: bytes, firmware: bytes, port_name: str | None, confirmation: str | None,
                    execute: bool, expected_pack: str, expected_firmware: str) -> None:
    validate_migration_files(pack, firmware, expected_pack, expected_firmware)
    print(f"pack: bytes={len(pack)} crc32={zlib.crc32(pack):08x} sha256={hashlib.sha256(pack).hexdigest()}")
    print(f"firmware: sha256={hashlib.sha256(firmware).hexdigest()}")
    print("target: EF-40-18 capacity=16777216 offset=0x43b000 reserved_end=0x467000")
    if not execute:
        print("DRY RUN ONLY: no serial port opened and no device write attempted")
        return
    if confirmation != "MIGRATE":
        raise ValueError("physical migration requires --execute --confirm MIGRATE")
    if not port_name:
        raise ValueError("physical migration requires an explicit --port")
    serial, _ = serial_module()
    with serial.Serial(port_name, 115200, timeout=4, write_timeout=2) as port:
        port.reset_input_buffer()
        port.write(frame(1, 0))
        ready = read_protocol_line(port, (b"CPCU1 READY",))
        if b"flash=EF-40-18" not in ready or b"capacity=16777216" not in ready:
            raise RuntimeError("unexpected physical flash identity/capacity")
        print(ready.decode())
        port.write(frame(9, 0))
        print(read_protocol_line(port, (b"CPCU1 MIGRATION LAYOUT",)).decode())
        begin = (b"MIGRATE\0" + struct.pack("<IIII", 1, MIGRATION_PACK_OFFSET,
                 len(pack), zlib.crc32(pack)) + bytes.fromhex(MIGRATION_PACK_SHA256))
        port.write(frame(10, 0, begin))
        print(read_protocol_line(port, (b"CPCU1 MIGRATION BEGIN OK",)).decode())
        for sequence, offset in enumerate(range(0, len(pack), MIGRATION_PAGE_BYTES)):
            page = pack[offset:offset + MIGRATION_PAGE_BYTES]
            port.write(frame(11, sequence, page))
            read_protocol_line(port, (f"CPCU1 MIGRATION ACK {sequence} ".encode(),))
        port.write(frame(12, 0))
        print(read_protocol_line(port, (b"CPCU1 MIGRATION VERIFIED",)).decode())

        staged = read_migration_range(port, MIGRATION_PACK_OFFSET, len(pack))
        expected_staged = bytearray(pack)
        expected_staged[0:256] = b"\xff" * 256
        expected_staged[4096:4352] = b"\xff" * 256
        if staged != expected_staged:
            raise RuntimeError("pre-activation readback mismatch; outcome is HOLD")

        port.write(frame(13, 0))
        print(read_protocol_line(port, (b"CPCU1 MIGRATION ACTIVE",)).decode())
        readback = read_migration_range(port, MIGRATION_PACK_OFFSET, len(pack))
        if hashlib.sha256(readback).hexdigest() != MIGRATION_PACK_SHA256:
            raise RuntimeError("post-activation SHA-256 readback mismatch; outcome is HOLD")
        print("independent readback SHA-256: PASS")


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    sub = parser.add_subparsers(dest="command", required=True)
    make = sub.add_parser("import-sna", help="validate and package a standalone 64 KiB CPC464 SNA")
    make.add_argument("snapshot", type=Path)
    make.add_argument("--id", required=True)
    make.add_argument("--title", required=True)
    make.add_argument("--controls", choices=tuple(CONTROL_PROFILES), default="none")
    make.add_argument("--output", required=True, type=Path)
    knight = sub.add_parser("import-knight", help="run the proven Knight Tyme host loader, then package")
    knight.add_argument("source_directory", type=Path)
    knight.add_argument("--builder", required=True, type=Path)
    knight.add_argument("--output", required=True, type=Path)
    inspect = sub.add_parser("inspect", help="validate and describe a .cpcgame package")
    inspect.add_argument("package", type=Path)
    send = sub.add_parser("upload", help="send a package to an explicit Stage 4 upload-mode receiver")
    send.add_argument("package", type=Path)
    send.add_argument("--port")
    send.add_argument("--confirm", required=True)
    detect = sub.add_parser("detect", help="find Pico SDK CPC Explorer serial ports")
    status = sub.add_parser("status", help="query firmware, flash capacity, and upload state")
    status.add_argument("--port")
    listing = sub.add_parser("list", help="list built-in and installed games")
    listing.add_argument("--port")
    removal = sub.add_parser("remove", help="transactionally remove one installed game by ID")
    removal.add_argument("game_id")
    removal.add_argument("--port")
    removal.add_argument("--confirm", required=True)
    migration_query = sub.add_parser("migration-status", help="read-only CPXPCK2 migration state")
    migration_query.add_argument("--port")
    migration_backup = sub.add_parser("backup-library", help="read-only backup of catalogues and legacy allocations")
    migration_backup.add_argument("--port", required=True)
    migration_backup.add_argument("--output", required=True, type=Path)
    catalogue_audit = sub.add_parser("audit-catalogues", help="read-only two-slot CPCLIB1 descriptor audit")
    catalogue_audit.add_argument("--port", required=True)
    catalogue_audit.add_argument("--output", required=True, type=Path)
    migration = sub.add_parser("migrate-library", help="dry-run by default; install the fixed Stage 4I.7 pack")
    migration.add_argument("pack", type=Path)
    migration.add_argument("--firmware", required=True, type=Path)
    migration.add_argument("--expect-pack-sha256", required=True)
    migration.add_argument("--expect-firmware-sha256", required=True)
    migration.add_argument("--port")
    migration.add_argument("--execute", action="store_true")
    migration.add_argument("--confirm")
    staging = sub.add_parser("stage-library", help="dry-run by default; program and verify without activation")
    staging.add_argument("pack", type=Path)
    staging.add_argument("--firmware", required=True, type=Path)
    staging.add_argument("--expect-pack-sha256", required=True)
    staging.add_argument("--expect-firmware-sha256", required=True)
    staging.add_argument("--port")
    staging.add_argument("--execute", action="store_true")
    staging.add_argument("--confirm")
    activation = sub.add_parser("activate-library", help="dry-run by default; activate an already verified pack")
    activation.add_argument("pack", type=Path)
    activation.add_argument("--expect-pack-sha256", required=True)
    activation.add_argument("--port")
    activation.add_argument("--execute", action="store_true")
    activation.add_argument("--confirm")
    args = parser.parse_args(argv)
    try:
        if args.command == "import-sna":
            packaged = package_snapshot(args.snapshot.read_bytes(), args.id, args.title,
                                         args.controls)
            args.output.parent.mkdir(parents=True, exist_ok=True)
            args.output.write_bytes(packaged)
            info = parse_package(packaged)
            print(f"wrote {args.output} ({len(packaged)} bytes, crc32={info['package_crc32']})")
        elif args.command == "import-knight":
            temporary = args.output.with_suffix(".validated.sna")
            try:
                subprocess.run([str(args.builder), "knight-tyme", str(args.source_directory),
                                str(temporary)], check=True)
                packaged = package_snapshot(temporary.read_bytes(), "knight_tyme", "Knight Tyme")
                args.output.parent.mkdir(parents=True, exist_ok=True)
                args.output.write_bytes(packaged)
            finally:
                temporary.unlink(missing_ok=True)
            print(f"wrote {args.output} ({len(packaged)} bytes)")
        elif args.command == "inspect":
            for key, value in parse_package(args.package.read_bytes()).items():
                print(f"{key}: {value}")
        elif args.command == "detect":
            ports = detect_ports()
            if not ports:
                raise RuntimeError("no VID 2E8A/PID 0009 CPC Explorer found")
            print("\n".join(ports))
        elif args.command in ("status", "list"):
            query(selected_port(args.port), args.command == "list")
        elif args.command == "upload":
            package = args.package.read_bytes()
            # This must precede selected_port(): rejected titles cannot even begin
            # device discovery, let alone send BEGIN or trigger flash operations.
            validate_launcher_visibility(package)
            upload(package, selected_port(args.port), args.confirm)
            print("upload committed")
        elif args.command == "migration-status":
            migration_status(selected_port(args.port))
        elif args.command == "backup-library":
            backup_legacy_library(args.port, args.output)
        elif args.command == "audit-catalogues":
            audit_catalogues(args.port, args.output)
        elif args.command == "migrate-library":
            migrate_library(args.pack.read_bytes(), args.firmware.read_bytes(), args.port,
                            args.confirm, args.execute, args.expect_pack_sha256,
                            args.expect_firmware_sha256)
        elif args.command == "stage-library":
            stage_library(args.pack.read_bytes(), args.firmware.read_bytes(), args.port,
                          args.confirm, args.execute, args.expect_pack_sha256,
                          args.expect_firmware_sha256)
        elif args.command == "activate-library":
            activate_library(args.pack.read_bytes(), args.port, args.confirm, args.execute,
                             args.expect_pack_sha256)
        else:
            remove_game(args.game_id, selected_port(args.port), args.confirm)
            print("removal committed")
    except (OSError, ValueError, RuntimeError, subprocess.CalledProcessError) as error:
        print(f"error: {error}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
