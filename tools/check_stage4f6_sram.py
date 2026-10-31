#!/usr/bin/env python3
"""Fail closed unless the Stage 4F.6 linker map matches the SRAM experiment."""

from pathlib import Path
import re
import sys


STEP_BYTES = 76_212
MIN_HEADROOM = 116 * 1024
MIN_HEAP = 2_048


def symbol(text: str, name: str) -> int:
    matches = re.findall(rf"0x([0-9a-fA-F]+)\s+{re.escape(name)}\b", text)
    if len(matches) != 1:
        raise SystemExit(f"expected exactly one {name}, found {len(matches)}")
    return int(matches[0], 16)


def main() -> None:
    if len(sys.argv) not in (2, 3):
        raise SystemExit("usage: check_stage4f6_sram.py LINKER_MAP [EXPECTED_STEP_BYTES]")
    expected_step_bytes = int(sys.argv[2], 0) if len(sys.argv) == 3 else STEP_BYTES
    text = Path(sys.argv[1]).read_text(encoding="utf-8", errors="replace")

    match = re.search(
        r"\.time_critical\.fast_z80_step\s*\n\s*"
        r"0x([0-9a-fA-F]+)\s+0x([0-9a-fA-F]+)\s+libcpc_core\.a\(z80\.c\.obj\)",
        text,
    )
    if not match:
        raise SystemExit("z80_execute_step SRAM section missing")
    step_addr, step_size = (int(value, 16) for value in match.groups())
    if not 0x20000000 <= step_addr < 0x20080000:
        raise SystemExit(f"z80_execute_step is not in main SRAM: 0x{step_addr:08x}")
    if step_size != expected_step_bytes:
        raise SystemExit(
            f"z80_execute_step changed size: {step_size} != {expected_step_bytes}"
        )
    if text.count(".time_critical.fast_z80_step") != 1:
        raise SystemExit("unexpected duplicate z80_execute_step SRAM section")
    if ".time_critical.fast_z80_table" in text:
        raise SystemExit("rejected SRAM dispatch-table path is present")

    bss_end = symbol(text, "__bss_end__")
    heap_limit = symbol(text, "__HeapLimit")
    stack_bottom = symbol(text, "__StackBottom")
    stack_top = symbol(text, "__StackTop")
    if stack_top - stack_bottom != 2_048 or stack_bottom < heap_limit:
        raise SystemExit("core-0 stack reservation or placement changed")

    heap_match = re.search(r"^\.heap\s+0x[0-9a-fA-F]+\s+0x([0-9a-fA-F]+)\b", text, re.MULTILINE)
    if not heap_match:
        raise SystemExit("minimum heap section missing")
    heap_size = int(heap_match.group(1), 16)
    if heap_size < MIN_HEAP:
        raise SystemExit(f"minimum heap too small: {heap_size} < {MIN_HEAP}")

    headroom = heap_limit - (bss_end + heap_size)
    if headroom < MIN_HEADROOM:
        raise SystemExit(f"SRAM headroom gate failed: {headroom} < {MIN_HEADROOM}")

    print(
        "stage4f6 SRAM gate: "
        f"step=0x{step_addr:08x}+{step_size} heap={heap_size} "
        f"stack={stack_top - stack_bottom} headroom={headroom} "
        f"minimum={MIN_HEADROOM}"
    )


if __name__ == "__main__":
    main()
