#!/usr/bin/env python3
"""Validate the non-deployable Stage 4H.5A scratch-SRAM link probe."""

from pathlib import Path
import re
import sys


MIN_MAIN_HEADROOM = 116 * 1024


def symbol(text: str, name: str) -> int:
    matches = re.findall(rf"0x([0-9a-fA-F]+)\s+{re.escape(name)}\b", text)
    if len(matches) != 1:
        raise SystemExit(f"expected exactly one {name}, found {len(matches)}")
    return int(matches[0], 16)


def section(text: str, name: str) -> tuple[int, int]:
    match = re.search(
        rf"{re.escape(name)}\s*\n?\s*0x([0-9a-fA-F]+)\s+0x([0-9a-fA-F]+)",
        text,
    )
    if not match:
        raise SystemExit(f"missing {name}")
    return tuple(int(value, 16) for value in match.groups())


def main() -> None:
    if len(sys.argv) != 2:
        raise SystemExit("usage: check_stage4h5a_sram_probe.py LINKER_MAP")
    text = Path(sys.argv[1]).read_text(encoding="utf-8", errors="replace")

    control_addr, control_size = section(text, ".scratch_x.stage4h5a_control")
    dc_addr, dc_size = section(text, ".scratch_y.stage4h5a_dc")
    stack_addr, stack_size = section(text, ".stack1.stage4h5a")
    stack_one_bottom = symbol(text, "__StackOneBottom")
    stack_one_top = symbol(text, "__StackOneTop")
    stack_bottom = symbol(text, "__StackBottom")
    stack_top = symbol(text, "__StackTop")

    if control_addr != 0x20080000 or control_size > 1_224:
        raise SystemExit(f"unexpected control placement 0x{control_addr:08x}+{control_size}")
    if dc_addr != 0x20081000 or dc_size != 2_048:
        raise SystemExit(f"unexpected DC placement 0x{dc_addr:08x}+{dc_size}")
    if stack_size != 2_048 or stack_one_top - stack_one_bottom != 2_048:
        raise SystemExit("core-1 stack is not exactly 2048 bytes")
    # .stack1_dummy is only a sizing input. The SDK places the live core-1
    # stack at __StackOneBottom..__StackOneTop, at the top of scratch X.
    if control_addr + control_size > stack_one_bottom:
        raise SystemExit("audio control overlaps core-1 stack")
    if dc_addr + dc_size > stack_bottom:
        raise SystemExit("DC history overlaps core-0 stack")
    if stack_top - stack_bottom != 2_048:
        raise SystemExit("core-0 stack changed")

    bss_end = symbol(text, "__bss_end__")
    heap_limit = symbol(text, "__HeapLimit")
    heap_match = re.search(r"^\.heap\s+0x[0-9a-fA-F]+\s+0x([0-9a-fA-F]+)", text, re.MULTILINE)
    if not heap_match:
        raise SystemExit("heap section missing")
    heap_size = int(heap_match.group(1), 16)
    headroom = heap_limit - (bss_end + heap_size)
    if headroom < MIN_MAIN_HEADROOM:
        raise SystemExit(f"main SRAM gate failed: {headroom} < {MIN_MAIN_HEADROOM}")

    print(
        "stage4h5a SRAM probe: "
        f"control=0x{control_addr:08x}+{control_size} "
        f"core1_stack=0x{stack_addr:08x}+{stack_size} "
        f"dc=0x{dc_addr:08x}+{dc_size} "
        f"scratch_x_free={stack_one_bottom - (control_addr + control_size)} "
        f"scratch_y_free={stack_bottom - (dc_addr + dc_size)} "
        f"main_headroom={headroom} minimum={MIN_MAIN_HEADROOM}"
    )


if __name__ == "__main__":
    main()
