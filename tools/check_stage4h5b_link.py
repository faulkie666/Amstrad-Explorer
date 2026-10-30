#!/usr/bin/env python3
"""Fail closed unless the final Stage 4H.5B SRAM layout is exact."""

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
        rf"{re.escape(name)}\s*\n?\s*0x([0-9a-fA-F]+)\s+0x([0-9a-fA-F]+)", text
    )
    if not match:
        raise SystemExit(f"missing {name}")
    return tuple(int(value, 16) for value in match.groups())


def main() -> None:
    if len(sys.argv) != 2:
        raise SystemExit("usage: check_stage4h5b_link.py LINKER_MAP")
    text = Path(sys.argv[1]).read_text(encoding="utf-8", errors="replace")

    control_addr, control_size = section(text, ".scratch_x.stage4h5b_control")
    dc_addr, dc_size = section(text, ".scratch_y.stage4h5b_dc")
    stack_one_bottom = symbol(text, "__StackOneBottom")
    stack_one_top = symbol(text, "__StackOneTop")
    stack_bottom = symbol(text, "__StackBottom")
    stack_top = symbol(text, "__StackTop")
    if (control_addr, control_size) != (0x20080000, 1_216):
        raise SystemExit(f"unexpected control layout 0x{control_addr:08x}+{control_size}")
    if (dc_addr, dc_size) != (0x20081000, 2_048):
        raise SystemExit(f"unexpected DC layout 0x{dc_addr:08x}+{dc_size}")
    if (stack_one_bottom, stack_one_top) != (0x20080800, 0x20081000):
        raise SystemExit("core-1 live stack is not the intended upper scratch-X half")
    if (stack_bottom, stack_top) != (0x20081800, 0x20082000):
        raise SystemExit("core-0 live stack changed")
    if control_addr + control_size > stack_one_bottom or dc_addr + dc_size > stack_bottom:
        raise SystemExit("scratch allocation overlaps a live stack")
    stack_inputs = re.findall(r"^ \.stack1[^\n]*", text, re.MULTILINE)
    if len(stack_inputs) != 1 or ".stack1.stage4h5b" not in stack_inputs[0]:
        raise SystemExit("custom core-1 stack reservation missing or duplicated")
    if "pico_multicore/multicore.c.obj\n" in "\n".join(stack_inputs):
        raise SystemExit("default SDK core-1 stack was retained")
    bss_end = symbol(text, "__bss_end__")
    heap_limit = symbol(text, "__HeapLimit")
    heap_match = re.search(r"^\.heap\s+0x[0-9a-fA-F]+\s+0x([0-9a-fA-F]+)",
                           text, re.MULTILINE)
    if not heap_match:
        raise SystemExit("heap section missing")
    heap_size = int(heap_match.group(1), 16)
    headroom = heap_limit - (bss_end + heap_size)
    if headroom < MIN_MAIN_HEADROOM:
        raise SystemExit(f"main SRAM gate failed: {headroom} < {MIN_MAIN_HEADROOM}")

    print(
        "stage4h5b link gate: "
        f"control=0x{control_addr:08x}+{control_size} "
        f"core1_stack=0x{stack_one_bottom:08x}+{stack_one_top-stack_one_bottom} "
        f"dc=0x{dc_addr:08x}+{dc_size} "
        f"core0_stack=0x{stack_bottom:08x}+{stack_top-stack_bottom} "
        f"scratch_x_gap={stack_one_bottom-(control_addr+control_size)} "
        f"main_headroom={headroom} minimum={MIN_MAIN_HEADROOM}"
    )


if __name__ == "__main__":
    main()
