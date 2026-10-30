#!/usr/bin/env python3
"""Benchmark deterministic lossless codecs on validated CPC snapshots."""

from __future__ import annotations

import argparse
import hashlib
import json
import statistics
import sys
import time
import zlib
from pathlib import Path


def median_decode_us(decode, encoded: bytes, expected: bytes, rounds: int) -> float:
    timings = []
    for _ in range(rounds):
        started = time.perf_counter_ns()
        result = decode(encoded)
        timings.append((time.perf_counter_ns() - started) / 1000.0)
        if result != expected:
            raise ValueError("decoder output differs from source snapshot")
    return statistics.median(timings)


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--packages", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("snapshots", nargs="+", type=Path)
    parser.add_argument("--rounds", type=int, default=100)
    args = parser.parse_args()

    sys.path.insert(0, str(args.packages.resolve()))
    import heatshrink2  # type: ignore
    import lz4  # type: ignore
    import lz4.block  # type: ignore
    import zstandard  # type: ignore

    zstd_compressor = zstandard.ZstdCompressor(level=3)
    zstd_decompressor = zstandard.ZstdDecompressor()
    codecs = {
        "lz4-hc-block": (
            lambda b: lz4.block.compress(
                b, mode="high_compression", compression=12, store_size=False
            ),
            lambda b: lz4.block.decompress(b, uncompressed_size=65792),
        ),
        "heatshrink-11-4": (
            lambda b: heatshrink2.compress(b, window_sz2=11, lookahead_sz2=4),
            lambda b: heatshrink2.decompress(
                b, input_buffer_size=2048, window_sz2=11, lookahead_sz2=4
            ),
        ),
        "deflate-9": (lambda b: zlib.compress(b, 9), zlib.decompress),
        "zstd-3": (zstd_compressor.compress, zstd_decompressor.decompress),
    }

    records = []
    for path in args.snapshots:
        source = path.read_bytes()
        if len(source) != 65792 or source[:8] != b"MV - SNA":
            raise ValueError(f"not an exact CPC SNA: {path}")
        for name, (encode, decode) in codecs.items():
            encoded = encode(source)
            if encode(source) != encoded:
                raise ValueError(f"non-deterministic encoder output: {name}")
            decode_us = median_decode_us(decode, encoded, source, args.rounds)
            records.append(
                {
                    "snapshot": path.name,
                    "source_path": str(path.resolve()),
                    "source_sha256": hashlib.sha256(source).hexdigest(),
                    "codec": name,
                    "compressed_bytes": len(encoded),
                    "ratio_percent": round(100.0 * len(encoded) / len(source), 3),
                    "desktop_decode_median_us": round(decode_us, 3),
                    "encoded_sha256": hashlib.sha256(encoded).hexdigest(),
                    "deterministic": True,
                    "roundtrip": True,
                }
            )

    summaries = []
    for codec in codecs:
        selected = [row for row in records if row["codec"] == codec]
        summaries.append(
            {
                "codec": codec,
                "mean_bytes": round(
                    statistics.mean(row["compressed_bytes"] for row in selected), 2
                ),
                "median_bytes": statistics.median(
                    row["compressed_bytes"] for row in selected
                ),
                "minimum_bytes": min(row["compressed_bytes"] for row in selected),
                "maximum_bytes": max(row["compressed_bytes"] for row in selected),
                "mean_ratio_percent": round(
                    statistics.mean(row["ratio_percent"] for row in selected), 3
                ),
                "desktop_decode_median_us": round(
                    statistics.median(
                        row["desktop_decode_median_us"] for row in selected
                    ),
                    3,
                ),
            }
        )

    report = {
        "tool_versions": {
            "python": sys.version,
            "heatshrink2": "0.15.0",
            "lz4": lz4.__version__,
            "zstandard": zstandard.__version__,
            "zlib": zlib.ZLIB_VERSION,
        },
        "rounds": args.rounds,
        "records": records,
        "summary": summaries,
    }
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(report, indent=2), encoding="utf-8")
    print(json.dumps(report["summary"], indent=2))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
