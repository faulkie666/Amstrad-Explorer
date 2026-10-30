#!/usr/bin/env python3
"""Resumable, read-only CPCLIB1 catalogue audit for Stage 4K.8.

Each 4 KiB window is attempted once in its own serial session and persisted only
after exact offset/length framing succeeds.  A failed window is never retried by
the same invocation; rerun with --resume after a separately approved recovery.
"""

from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path
import sys

sys.path.insert(0, str(Path(__file__).resolve().parent))

import cpc_game


START = 0x400000
BYTES = 0x8000
WINDOW_BYTES = 0x1000
EXPECTED_SHA256 = "e006aedc68aaa1ddb2a92f0dd298c934d77af090c1d648612ed6d7ed26a3ec27"
EXPECTED_READY = (
    b"fw=0.4.28-stage4k7migrationrecoverfix",
    b"flash=EF-40-18",
    b"capacity=16777216",
)


def sha256(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def prepare_output(path: Path, resume: bool) -> None:
    path.mkdir(parents=True, exist_ok=True)
    if not path.is_dir():
        raise RuntimeError(f"audit evidence path is not a directory: {path}")
    existing = list(path.iterdir())
    if existing and not resume:
        raise RuntimeError(f"audit evidence directory is not empty: {path}")
    probe = path / ".write-test.tmp"
    probe.write_bytes(b"stage4k8 catalogue audit\0")
    if probe.read_bytes() != b"stage4k8 catalogue audit\0":
        raise RuntimeError("audit evidence path readback failed")
    probe.unlink()


def window_name(index: int, offset: int) -> str:
    return f"catalog-{index:02d}-{offset:06x}-{offset + WINDOW_BYTES - 1:06x}.bin"


def save_exact(path: Path, data: bytes) -> str:
    if len(data) != WINDOW_BYTES:
        raise RuntimeError(f"catalogue window length mismatch: {len(data)}")
    path.write_bytes(data)
    saved = path.read_bytes()
    if saved != data:
        raise RuntimeError(f"saved catalogue evidence mismatch: {path}")
    return sha256(saved)


def load_existing(path: Path, expected_hash: str | None) -> tuple[bytes, str]:
    data = path.read_bytes()
    digest = sha256(data)
    if len(data) != WINDOW_BYTES or expected_hash is None or digest != expected_hash:
        raise RuntimeError(f"unmanifested or changed catalogue evidence: {path.name}")
    return data, digest


def write_manifest(output: Path, ranges: list[dict[str, object]]) -> None:
    temporary = output / ".catalogue-audit-manifest.tmp"
    temporary.write_text(json.dumps({"ranges": ranges}, indent=2) + "\n",
                         encoding="utf-8")
    temporary.replace(output / "catalogue-audit-manifest.json")


def read_one_window(port_name: str, offset: int) -> tuple[bytes, str]:
    serial, _ = cpc_game.serial_module()
    with serial.Serial(port_name, 115200, timeout=4, write_timeout=2) as port:
        port.reset_input_buffer()
        port.write(cpc_game.frame(1, 0))
        ready = cpc_game.read_protocol_line(port, (b"CPCU1 READY",))
        if not all(token in ready for token in EXPECTED_READY):
            raise RuntimeError(f"unexpected device identity: {ready.decode(errors='replace')}")
        data = cpc_game.read_migration_range(port, offset, WINDOW_BYTES)
    return data, ready.decode()


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--port", required=True)
    parser.add_argument("--evidence-dir", required=True, type=Path)
    parser.add_argument("--resume", action="store_true")
    args = parser.parse_args()

    prepare_output(args.evidence_dir, args.resume)
    manifest_path = args.evidence_dir / "catalogue-audit-manifest.json"
    known: dict[str, str] = {}
    if manifest_path.exists():
        prior = json.loads(manifest_path.read_text(encoding="utf-8"))
        known = {item["file"]: item["sha256"] for item in prior.get("ranges", [])}

    ranges: list[dict[str, object]] = []
    blocks: list[bytes] = []
    ready_lines: list[str] = []
    for index, offset in enumerate(range(START, START + BYTES, WINDOW_BYTES)):
        filename = window_name(index, offset)
        path = args.evidence_dir / filename
        if path.exists():
            data, digest = load_existing(path, known.get(filename))
            source = "saved"
        else:
            try:
                data, ready = read_one_window(args.port, offset)
            except Exception as error:
                raise RuntimeError(
                    f"catalogue audit stopped at window={index} offset=0x{offset:06x}; "
                    f"next_unread=0x{offset:06x}: {error}") from error
            digest = save_exact(path, data)
            ready_lines.append(ready)
            source = "device"
        ranges.append({"index": index, "offset": offset, "bytes": len(data),
                       "sha256": digest, "file": filename, "source": source})
        write_manifest(args.evidence_dir, ranges)
        blocks.append(data)

    combined = b"".join(blocks)
    digest = sha256(combined)
    if len(combined) != BYTES or digest != EXPECTED_SHA256:
        raise RuntimeError(
            f"complete catalogue audit identity mismatch: bytes={len(combined)} "
            f"sha256={digest} expected={EXPECTED_SHA256}")
    slots = cpc_game.parse_catalogue_slots(combined)
    combined_path = args.evidence_dir / "catalogues-400000-407fff.bin"
    combined_path.write_bytes(combined)
    report = {
        "bytes": len(combined), "sha256": digest,
        "expected_sha256": EXPECTED_SHA256, "windows": len(ranges),
        "window_bytes": WINDOW_BYTES, "protocol_read_bytes": cpc_game.MIGRATION_READ_BYTES,
        "ready": ready_lines[-1] if ready_lines else "validated saved evidence",
        "slots": slots,
    }
    (args.evidence_dir / "catalogue-audit.json").write_text(
        json.dumps(report, indent=2) + "\n", encoding="utf-8")
    print(json.dumps(report, indent=2))
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except Exception as error:
        print(f"error: {error}", file=sys.stderr)
        raise SystemExit(1)
