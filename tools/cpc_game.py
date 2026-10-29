#!/usr/bin/env python3
"""Deterministic CPC Explorer snapshot packager and Windows serial uploader."""

from __future__ import annotations

import argparse
import re
import struct
import subprocess
import sys
import zlib
from pathlib import Path

PACKAGE_MAGIC = b"CPCGAME1"
PACKAGE_VERSION = 1
PACKAGE_HEADER_BYTES = 16
DESCRIPTOR_BYTES = 80
PAYLOAD_OFFSET = PACKAGE_HEADER_BYTES + DESCRIPTOR_BYTES
SNA_BYTES = 256 + 65536
CHUNK_BYTES = 1024
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
CONTROL_PROFILES = {"none": 0, "qw-shift": 1, "zx-space": 2}
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
    for _ in range(32):
        line = port.readline().strip()
        if not line:
            continue
        if line.startswith(b"CPCU1 ERROR"):
            raise RuntimeError(line.decode("ascii", "replace"))
        if any(line.startswith(prefix) for prefix in expected):
            return line
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
    parse_package(package)
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
        expected_payload_crc = parse_package(package)["payload_crc32"]
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
            upload(args.package.read_bytes(), selected_port(args.port), args.confirm)
            print("upload committed")
        else:
            remove_game(args.game_id, selected_port(args.port), args.confirm)
            print("removal committed")
    except (OSError, ValueError, RuntimeError, subprocess.CalledProcessError) as error:
        print(f"error: {error}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
