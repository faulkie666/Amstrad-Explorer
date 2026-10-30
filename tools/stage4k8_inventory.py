#!/usr/bin/env python3
"""Read-only, deterministic Stage 4K.8 source inventory."""

from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path


def digest(path: Path) -> str:
    value = hashlib.sha256()
    with path.open("rb") as source:
        for chunk in iter(lambda: source.read(1024 * 1024), b""):
            value.update(chunk)
    return value.hexdigest()


def kind(path: Path) -> str:
    name = path.name.lower()
    for suffix in (".sna.gz", ".dsk.gz", ".cdt.gz"):
        if name.endswith(suffix):
            return suffix
    return path.suffix.lower() or "(none)"


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--arcade", type=Path, required=True)
    parser.add_argument("--simulation", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()

    records = []
    counts: dict[str, dict[str, int]] = {}
    for category, root in (("Arcade", args.arcade), ("Simulation", args.simulation)):
        counts[category] = {}
        for path in sorted((p for p in root.rglob("*") if p.is_file()),
                           key=lambda p: str(p).casefold()):
            media = kind(path)
            counts[category][media] = counts[category].get(media, 0) + 1
            records.append({
                "category": category,
                "title_directory": path.parent.name,
                "path": str(path),
                "bytes": path.stat().st_size,
                "format": media,
                "sha256": digest(path),
            })

    payload = {
        "schema": "cpcexplorer-stage4k8-source-inventory-v1",
        "roots": {"Arcade": str(args.arcade), "Simulation": str(args.simulation)},
        "files": len(records),
        "counts_by_category_and_format": counts,
        "records": records,
    }
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(payload, indent=2) + "\n", encoding="utf-8")
    print(f"wrote {args.output} files={len(records)}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
