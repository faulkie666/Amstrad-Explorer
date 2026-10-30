#!/usr/bin/env python3
"""Deterministic fresh-snapshot control response probe for converted CPC games."""

from __future__ import annotations

import argparse
import json
from pathlib import Path

from stage4i4_koncepcja import Emulator, quoted


def probe(args: argparse.Namespace, label: str) -> dict[str, object]:
    emu = Emulator(args.exe, args.cfg, args.cwd, None, True)
    commands: list[dict[str, str]] = []

    def send(command: str, timeout: float = 15.0) -> str:
        response = emu.command(command, timeout)
        commands.append({"command": command, "response": response})
        if response.startswith("ERR"):
            raise RuntimeError(f"{command}: {response}")
        return response

    try:
        send("snapshot load " + quoted(args.snapshot))
        if label != "neutral":
            joystick = label.upper().removeprefix("JOY")
            if label.upper().startswith("JOY"):
                send(f"input joy 0 {joystick}")
                send(f"wait vbl {args.hold_vbl} 10000")
                send(f"input joy 0 -{joystick}")
            else:
                send(f"input key {label} hold={args.hold_vbl}")
        send(f"wait vbl {args.observe_vbl} 30000", 35.0)
        send(f"screenshot {quoted(args.output / (label + '.png'))}")
        vram = send("hash vram").split("crc32=", 1)[-1]
        ram = send("hash mem 0 65536").split("crc32=", 1)[-1]
        regs = send("regs")
        return {"label": label, "vram_crc32": vram, "ram_crc32": ram,
                "registers": regs, "commands": commands}
    finally:
        emu.close()


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--exe", type=Path, required=True)
    parser.add_argument("--cfg", type=Path, required=True)
    parser.add_argument("--cwd", type=Path, required=True)
    parser.add_argument("--snapshot", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--inputs", default="JOYLEFT,JOYRIGHT,JOYUP,JOYDOWN,JOYF1")
    parser.add_argument("--hold-vbl", type=int, default=5)
    parser.add_argument("--observe-vbl", type=int, default=100)
    args = parser.parse_args()
    args.output.mkdir(parents=True, exist_ok=True)
    labels = ["neutral"] + [item.strip() for item in args.inputs.split(",") if item.strip()]
    results = [probe(args, label) for label in labels]
    neutral = results[0]
    for result in results[1:]:
        result["differs_from_neutral"] = (
            result["vram_crc32"] != neutral["vram_crc32"] or
            result["ram_crc32"] != neutral["ram_crc32"]
        )
    (args.output / "results.json").write_text(
        json.dumps({"snapshot": str(args.snapshot), "hold_vbl": args.hold_vbl,
                    "observe_vbl": args.observe_vbl, "results": results}, indent=2) + "\n",
        encoding="utf-8")


if __name__ == "__main__":
    main()
