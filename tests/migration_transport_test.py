#!/usr/bin/env python3

import importlib.util
import struct
from pathlib import Path


TOOL = Path(__file__).parents[1] / "tools" / "cpc_game.py"
SPEC = importlib.util.spec_from_file_location("cpc_game", TOOL)
cpc_game = importlib.util.module_from_spec(SPEC)
assert SPEC.loader is not None
SPEC.loader.exec_module(cpc_game)


class FakePort:
    def __init__(self, fragments, timeout=0.001):
        self.fragments = list(fragments)
        self.timeout = timeout
        self.writes = []

    def write(self, data):
        self.writes.append(data)
        return len(data)

    def read_until(self, expected=b"\n", size=None):
        del expected, size
        return self.fragments.pop(0) if self.fragments else b""


def expect_error(fragments, message):
    port = FakePort(fragments)
    try:
        cpc_game.read_migration_range(port, 0x400000, 4)
    except RuntimeError as error:
        if message not in str(error):
            raise SystemExit(f"wrong error: {error!s}")
        return
    raise SystemExit(f"missing error containing {message!r}")


line = b"CPCU1 MIGRATION DATA 4194304 01020304\r\n"
fragmented = FakePort([line[:7], line[7:20], line[20:31], line[31:]])
if cpc_game.read_migration_range(fragmented, 0x400000, 4) != bytes((1, 2, 3, 4)):
    raise SystemExit("fragmented response was not reassembled")
if len(fragmented.writes) != 1:
    raise SystemExit("fragmentation incorrectly issued a second READ command")

expect_error([b"CPCU1 MIGRATION DATA 4194304 0102\n"], "expected=4 actual=2")
expect_error([b"CPCU1 MIGRATION DATA 4194305 01020304\n"], "offset mismatch")
expect_error([b"CPCU1 MIGRATION DATA 4194304 0102GG04\n"], "corrupt hex")
expect_error([line[:20]], "timed out")
expect_error([], "timed out")

telemetry_then_data = FakePort([
    b"CPCPERF slices=1 speed_pct_x10=800\n",
    line[:12], line[12:],
])
if cpc_game.read_migration_range(telemetry_then_data, 0x400000, 4) != bytes((1, 2, 3, 4)):
    raise SystemExit("interleaved telemetry handling regressed")

if cpc_game.MIGRATION_READ_BYTES != 64:
    raise SystemExit("physical migration reads must remain bounded to 64 bytes")
first = bytes(range(64))
second = bytes(range(64, 128))
bounded = FakePort([
    b"CPCU1 MIGRATION DATA 4194304 " + first.hex().encode() + b"\n",
    b"CPCU1 MIGRATION DATA 4194368 " + second.hex().encode() + b"\n",
])
if cpc_game.read_migration_range(bounded, 0x400000, 128) != first + second:
    raise SystemExit("64-byte bounded read assembly failed")
if len(bounded.writes) != 2:
    raise SystemExit("64-byte strategy issued an unexpected command count")
for index, request in enumerate(bounded.writes):
    magic, version, command, payload_length, sequence, payload_crc = struct.unpack(
        "<4sBBHII", request[:16])
    payload = request[16:]
    offset, count = struct.unpack("<II", payload)
    if (magic != b"CPCU" or version != 1 or command != 15 or payload_length != 8 or
            sequence != 0 or payload_crc != cpc_game.zlib.crc32(payload) or
            offset != 0x400000 + index * 64 or count != 64):
        raise SystemExit("64-byte request framing or offset validation failed")

print("PASS: 64-byte bounded reads, fragmentation, short/corrupt frames, timeouts and command count")
