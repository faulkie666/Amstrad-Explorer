#!/usr/bin/env python3
"""Read-only Stage 4H.5 PSG trace and producer/consumer capacity analysis."""

from __future__ import annotations

import argparse
import csv
import math
from collections import Counter
from pathlib import Path


CPC_HZ = 4_000_000
AY_HZ = 1_000_000
SAMPLE_HZ = 22_050
DMA_SAMPLES = 128


def inspect(label: str, path: Path, ratio: float) -> None:
    writes: list[tuple[int, int, int]] = []
    with path.open(newline="", encoding="utf-8") as stream:
        for row in csv.DictReader(stream):
            if row["event"] != "write":
                continue
            reg = int(row["register"])
            if 0 <= reg <= 13:
                writes.append((int(row["cycle"]), reg, int(row["value"])))

    blocks: Counter[int] = Counter()
    seconds: Counter[int] = Counter()
    for cycle, _, _ in writes:
        wall_sample = math.floor((cycle / CPC_HZ) * SAMPLE_HZ / ratio)
        blocks[wall_sample // DMA_SAMPLES] += 1
        seconds[wall_sample // SAMPLE_HZ] += 1

    last_emu_s = writes[-1][0] / CPC_HZ if writes else 0.0
    wall_s = last_emu_s / ratio if ratio else math.inf
    distinct = len({(reg, value) for _, reg, value in writes})
    print(
        f"{label}: sound_writes={len(writes)} distinct_reg_values={distinct} "
        f"last_event_emu_s={last_emu_s:.3f} projected_wall_s={wall_s:.3f} "
        f"max_writes_per_128_sample_block={max(blocks.values(), default=0)} "
        f"max_writes_per_wall_second={max(seconds.values(), default=0)} "
        f"envelope_restarts={sum(reg == 13 for _, reg, _ in writes)}"
    )


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--ratio", type=float, default=0.68)
    parser.add_argument("trace", nargs="+", help="LABEL=CSV")
    args = parser.parse_args()
    if not 0.0 < args.ratio <= 1.0:
        parser.error("--ratio must be in (0, 1]")

    block_ms = DMA_SAMPLES * 1000.0 / SAMPLE_HZ
    production_ms = block_ms / args.ratio
    print(
        f"ratio={args.ratio:.6f} block_play_ms={block_ms:.3f} "
        f"block_production_ms={production_ms:.3f} recurring_gap_ms={production_ms - block_ms:.3f} "
        f"sample_deficit_per_s={SAMPLE_HZ * (1.0 - args.ratio):.1f}"
    )
    for item in args.trace:
        if "=" not in item:
            parser.error(f"expected LABEL=CSV: {item}")
        label, filename = item.split("=", 1)
        inspect(label, Path(filename), args.ratio)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
