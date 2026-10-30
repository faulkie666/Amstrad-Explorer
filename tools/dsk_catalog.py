#!/usr/bin/env python3
"""Read-only catalogue probe for standard/extended CPC DSK or DSK.GZ images."""

from __future__ import annotations

import argparse
import gzip
import json
from pathlib import Path


def load(path: Path) -> bytes:
    raw = path.read_bytes()
    return gzip.decompress(raw) if path.name.lower().endswith(".gz") else raw


def sectors(image: bytes) -> list[tuple[int, int, int, bytes]]:
    if len(image) < 256:
        raise ValueError("short DSK")
    standard = image.startswith(b"MV - CPCEMU Disk-File")
    extended = image.startswith(b"EXTENDED CPC DSK File")
    if not (standard or extended):
        raise ValueError("unknown DSK header")
    tracks, sides = image[0x30], image[0x31]
    if not tracks or not sides:
        raise ValueError("invalid DSK geometry")
    fixed_track_size = int.from_bytes(image[0x32:0x34], "little")
    offset = 256
    result: list[tuple[int, int, int, bytes]] = []
    for physical in range(tracks * sides):
        track_size = (image[0x34 + physical] * 256) if extended else fixed_track_size
        if track_size == 0:
            continue
        block = image[offset:offset + track_size]
        if len(block) != track_size or not block.startswith(b"Track-Info"):
            raise ValueError(f"invalid track {physical}")
        count = block[0x15]
        data_at = 256
        for index in range(count):
            info = 0x18 + index * 8
            c, h, r, n = block[info:info + 4]
            declared = int.from_bytes(block[info + 6:info + 8], "little")
            length = declared if extended and declared else 128 << n
            payload = block[data_at:data_at + length]
            if len(payload) != length:
                raise ValueError(f"short sector C={c} H={h} R={r}")
            result.append((c, h, r, payload))
            data_at += length
        offset += track_size
    return result


def catalogue(path: Path) -> dict[str, object]:
    image = load(path)
    all_sectors = sectors(image)
    track_zero = [(r, data) for c, _h, r, data in all_sectors if c == 0]
    if not track_zero:
        raise ValueError("track zero has no sectors")
    # CPC data disks use C1..C4; system disks commonly use 41..44.
    base = 0xC1 if any(r == 0xC1 for r, _ in track_zero) else 0x41
    directory = b"".join(data for r, data in sorted(track_zero) if base <= r < base + 4)
    if len(directory) < 2048:
        raise ValueError("directory sectors are incomplete")
    files: dict[tuple[int, bytes, bytes], dict[str, object]] = {}
    for at in range(0, 2048, 32):
        entry = directory[at:at + 32]
        user = entry[0]
        if user == 0xE5 or user > 15:
            continue
        name = bytes(value & 0x7F for value in entry[1:9]).rstrip(b" ")
        ext = bytes(value & 0x7F for value in entry[9:12]).rstrip(b" ")
        if not name:
            continue
        key = (user, name, ext)
        item = files.setdefault(key, {"user": user, "name": name.decode("ascii", "replace"),
                                      "ext": ext.decode("ascii", "replace"), "records": 0,
                                      "extents": 0})
        item["records"] = int(item["records"]) + entry[15]
        item["extents"] = int(item["extents"]) + 1
    entries = sorted(files.values(), key=lambda item: (item["user"], item["name"], item["ext"]))
    return {"path": str(path), "compressed_bytes": path.stat().st_size,
            "image_bytes": len(image), "files": entries}


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("paths", nargs="+", type=Path)
    parser.add_argument("--json", action="store_true")
    parser.add_argument("--output", type=Path)
    parser.add_argument("--extract-dir", type=Path,
                        help="write decompressed DSK copies under this artifact directory")
    args = parser.parse_args()
    output = []
    failed = []
    for path in args.paths:
        try:
            item = catalogue(path)
            if args.extract_dir:
                args.extract_dir.mkdir(parents=True, exist_ok=True)
                target = args.extract_dir / path.name.removesuffix(".gz")
                target.write_bytes(load(path))
                item["artifact_dsk"] = str(target)
            output.append(item)
        except Exception as exc:  # fail each candidate independently
            failed.append({"path": str(path), "error": str(exc)})
    if args.json:
        rendered = json.dumps({"images": output, "failed": failed}, indent=2)
        if args.output:
            args.output.parent.mkdir(parents=True, exist_ok=True)
            args.output.write_text(rendered + "\n", encoding="utf-8")
        else:
            print(rendered)
    else:
        for item in output:
            print(item["path"])
            for entry in item["files"]:
                suffix = f".{entry['ext']}" if entry["ext"] else ""
                print(f"  {entry['name']}{suffix} records={entry['records']}")
        for item in failed:
            print(f"ERROR {item['path']}: {item['error']}")


if __name__ == "__main__":
    main()
