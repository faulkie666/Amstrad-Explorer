#!/usr/bin/env python3

import importlib.util
import struct
from pathlib import Path

TOOL = Path(__file__).parents[1] / "tools" / "validate_uf2.py"
spec = importlib.util.spec_from_file_location("validate_uf2", TOOL)
module = importlib.util.module_from_spec(spec)
assert spec.loader is not None
spec.loader.exec_module(module)


def block(flags: int, target: int, number: int, total: int, family: int,
          payload: bytes, extension: int | None = None) -> bytes:
    output = bytearray(512)
    struct.pack_into("<IIIIIIII", output, 0, module.MAGIC0, module.MAGIC1, flags,
                     target, len(payload), number, total, family)
    output[32:32 + len(payload)] = payload
    if extension is not None:
        struct.pack_into("<I", output, 32 + len(payload), extension)
    struct.pack_into("<I", output, 508, module.MAGIC_END)
    return bytes(output)


application = block(module.FAMILY_FLAG, module.XIP_BASE, 0, 1,
                    module.ARM_S_FAMILY, bytes(256))
e10 = block(module.E10_FLAGS, module.E10_TARGET, 0, 2,
            module.ABSOLUTE_FAMILY, bytes((0xEF,)) * 256, module.E10_EXTENSION)
report = module.validate_bytes(e10 + application)
assert report["application_blocks"] == 1
assert report["e10_blocks"] == 1
assert report["e10_treated_as_potentially_actionable"] is True


def rejected(data: bytes) -> bool:
    try:
        module.validate_bytes(data)
    except module.ValidationError:
        return True
    return False


assert rejected(block(module.E10_FLAGS, 0x10FEFF00, 0, 2,
                      module.ABSOLUTE_FAMILY, bytes((0xEF,)) * 256,
                      module.E10_EXTENSION) + application)
assert rejected(block(module.E10_FLAGS, module.E10_TARGET, 0, 2,
                      module.ABSOLUTE_FAMILY, bytes((0xEE,)) * 256,
                      module.E10_EXTENSION) + application)
assert rejected(e10 + e10 + application)
assert rejected(block(module.FAMILY_FLAG, 0x10400000, 0, 1,
                      module.ARM_S_FAMILY, bytes(256)))
assert rejected(block(module.FAMILY_FLAG, 0xFFFFFFFF, 0, 1,
                      module.ARM_S_FAMILY, bytes(256)))
assert rejected(block(module.FAMILY_FLAG, module.XIP_BASE, 0, 1,
                      0xDEADBEEF, bytes(256)))

print("PASS: exact E10 exception and fail-closed UF2 target policy")
