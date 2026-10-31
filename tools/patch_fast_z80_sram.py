#!/usr/bin/env python3
"""Add opt-in Pico SRAM sections to the pinned cpu-z80 core.

The pinned upstream remains byte-for-byte unchanged unless the build defines
one of the narrowly scoped placement macros. FetchContent runs this
deterministic patch against a fresh, revision-pinned source tree.
"""

from pathlib import Path
import sys


def replace_once(path: Path, old: str, new: str) -> None:
    text = path.read_text(encoding="utf-8")
    if new in text:
        return
    if text.count(old) != 1:
        raise SystemExit(f"unexpected pinned cpu-z80 source at {path}")
    path.write_text(text.replace(old, new), encoding="utf-8", newline="\n")


def main() -> None:
    if len(sys.argv) != 2:
        raise SystemExit("usage: patch_fast_z80_sram.py CPU_Z80_SOURCE_DIR")
    root = Path(sys.argv[1]) / "cpu-z80" / "cpu"
    replace_once(
        root / "z80.c",
        "#define HOT         __attribute__((hot))",
        "#define HOT         __attribute__((hot))\n"
        "#if defined(CPU_Z80_SRAM_DISPATCH_TABLE)\n"
        "#define Z80_DISPATCH_TABLE_ATTR __attribute__((section(\".time_critical.fast_z80_table\")))\n"
        "#else\n"
        "#define Z80_DISPATCH_TABLE_ATTR\n"
        "#endif",
    )
    replace_once(
        root / "z80_execute.inc",
        "static const void* const base_dispatch[256] = {",
        "static const void* const base_dispatch[256] Z80_DISPATCH_TABLE_ATTR = {",
    )
    replace_once(
        root / "z80_execute.inc",
        "HOT int Z80_EXEC_FN(z80_t *cpu, int target_cycles) {",
        "HOT Z80_EXEC_ATTR int Z80_EXEC_FN(z80_t *cpu, int target_cycles) {",
    )
    replace_once(
        root / "z80_execute.inc",
        "        /* HALT - cykly už přičteny v NEXT_SLOW(4) */\n"
        "        if (cpu->halted) goto done;\n\n"
        "        if (cycles_executed >= target_cycles) goto done;",
        "        /* HALT - cykly už přičteny v NEXT_SLOW(4) */\n"
        "        if (cpu->halted) goto done;\n\n"
        "#if Z80_DIRECT_REGS && defined(CPU_Z80_BOUNDED_SPANS)\n"
        "        /* EI, DI, HALT, RETI and RETN use NEXT_SLOW.  End a bounded\n"
        "         * span at each such instruction so their interrupt-visible\n"
        "         * transition remains an external scheduling boundary. */\n"
        "        goto done;\n"
        "#endif\n\n"
        "        if (cycles_executed >= target_cycles) goto done;",
    )
    replace_once(
        root / "z80.c",
        "HOT int z80_execute_batch(z80_t *cpu, int target_cycles);\n"
        "HOT int z80_execute_step(z80_t *cpu, int target_cycles);",
        "HOT int z80_execute_batch(z80_t *cpu, int target_cycles);\n"
        "#if defined(CPU_Z80_SRAM_EXECUTE_STEP)\n"
        "HOT __attribute__((section(\".time_critical.fast_z80_step\")))\n"
        "int z80_execute_step(z80_t *cpu, int target_cycles);\n"
        "#else\n"
        "HOT int z80_execute_step(z80_t *cpu, int target_cycles);\n"
        "#endif",
    )
    replace_once(
        root / "z80.c",
        "#define Z80_EXEC_FN     z80_execute_batch\n#define Z80_DIRECT_REGS 0\n",
        "#define Z80_EXEC_FN     z80_execute_batch\n#define Z80_DIRECT_REGS 0\n#define Z80_EXEC_ATTR\n",
    )
    replace_once(
        root / "z80.c",
        "#include \"cpu/z80_execute.inc\"\n#undef Z80_EXEC_FN\n#undef Z80_DIRECT_REGS\n\n/* 2)",
        "#include \"cpu/z80_execute.inc\"\n#undef Z80_EXEC_ATTR\n#undef Z80_EXEC_FN\n#undef Z80_DIRECT_REGS\n\n/* 2)",
    )
    replace_once(
        root / "z80.c",
        "#define Z80_EXEC_FN     z80_execute_step\n#define Z80_DIRECT_REGS 1\n#include \"cpu/z80_execute.inc\"",
        "#define Z80_EXEC_FN     z80_execute_step\n#define Z80_DIRECT_REGS 1\n"
        "#if defined(CPU_Z80_SRAM_EXECUTE_STEP)\n"
        "#define Z80_EXEC_ATTR __attribute__((section(\".time_critical.fast_z80_step\")))\n"
        "#else\n#define Z80_EXEC_ATTR\n#endif\n"
        "#include \"cpu/z80_execute.inc\"",
    )
    replace_once(
        root / "z80.c",
        "#include \"cpu/z80_execute.inc\"\n#undef Z80_EXEC_FN\n#undef Z80_DIRECT_REGS\n\n/**",
        "#include \"cpu/z80_execute.inc\"\n#undef Z80_EXEC_ATTR\n#undef Z80_EXEC_FN\n#undef Z80_DIRECT_REGS\n\n/**",
    )


if __name__ == "__main__":
    main()
