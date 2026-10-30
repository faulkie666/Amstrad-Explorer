#!/usr/bin/env python3

import subprocess
import sys
import tempfile
from pathlib import Path


def run(*args: str) -> subprocess.CompletedProcess[str]:
    return subprocess.run([sys.executable, str(TOOL), *args], text=True,
                          stdout=subprocess.PIPE, stderr=subprocess.PIPE)


TOOL = Path(__file__).parents[1] / "tools" / "cpc_game.py"
KNIGHT_SNAPSHOT = Path(__file__).parents[1] / "snapshots" / "knight-tyme.sna"

with tempfile.TemporaryDirectory() as directory:
    root = Path(directory)
    snapshot = bytearray(256 + 65536)
    snapshot[:8] = b"MV - SNA"
    snapshot[16] = 1
    snapshot[107:109] = bytes((64, 0))
    source = root / "valid.sna"
    source.write_bytes(snapshot)
    first = root / "first.cpcgame"
    second = root / "second.cpcgame"
    arguments = ("import-sna", str(source), "--id", "test_game", "--title", "Test Game")
    one = run(*arguments, "--output", str(first))
    two = run(*arguments, "--output", str(second))
    if one.returncode or two.returncode or first.read_bytes() != second.read_bytes():
        raise SystemExit("deterministic valid import failed")
    if run("inspect", str(first)).returncode:
        raise SystemExit("package inspection failed")
    controlled = root / "controlled.cpcgame"
    controlled_result = run(*arguments, "--controls", "zx-space", "--output", str(controlled))
    controlled_inspect = run("inspect", str(controlled))
    if controlled_result.returncode or "controls: zx-space" not in controlled_inspect.stdout:
        raise SystemExit("generic control metadata was not packaged and inspected")
    corrupt = bytearray(controlled.read_bytes())
    corrupt[16 + 59] = 0xff
    corrupt[16 + 72:16 + 76] = bytes(4)
    import zlib
    corrupt[16 + 72:16 + 76] = zlib.crc32(corrupt[16:96]).to_bytes(4, "little")
    invalid_controls = root / "invalid-controls.cpcgame"
    invalid_controls.write_bytes(corrupt)
    rejected_controls = run("inspect", str(invalid_controls))
    if rejected_controls.returncode == 0 or "unsupported control profile" not in rejected_controls.stderr:
        raise SystemExit("invalid control metadata did not fail closed")

    # The deployed launcher exposes incremental games through an A-Z hierarchy.
    # Reject incompatible titles before opening the requested serial port.
    for game_id, title in (("4kroid", "4kroid"), ("digit", "9 Lives"),
                           ("punct", "! Invaders")):
        hidden = root / f"{game_id}.cpcgame"
        made = run("import-sna", str(source), "--id", game_id, "--title", title,
                   "--output", str(hidden))
        rejected = run("upload", str(hidden), "--port", "COM-NOT-REAL",
                       "--confirm", "UPLOAD")
        if made.returncode or rejected.returncode == 0 or \
                "launcher visibility requires an alphabetic title" not in rejected.stderr or \
                "could not open port" in rejected.stderr:
            raise SystemExit(f"hidden launcher title {title!r} was not rejected before serial access")
    alphabetic = run("upload", str(first), "--port", "COM-NOT-REAL", "--confirm", "UPLOAD")
    if "launcher visibility requires" in alphabetic.stderr:
        raise SystemExit("valid alphabetic launcher title was rejected")

    # Version 2 and 3 snapshots identify the source machine at offset 0x6D.
    # Only a native CPC464 snapshot may be described as CPC464 by the package.
    snapshot[16] = 2
    snapshot[0x6D] = 0
    cpc464_v2 = root / "cpc464-v2.sna"
    cpc464_v2.write_bytes(snapshot)
    if run("import-sna", str(cpc464_v2), "--id", "cpc464_v2", "--title", "CPC464 V2",
           "--output", str(root / "cpc464-v2.cpcgame")).returncode:
        raise SystemExit("native CPC464 v2 snapshot was rejected")

    for machine, expected in ((1, "CPC664"), (2, "CPC6128"), (3, "unknown CPC"),
                              (4, "CPC6128 Plus"), (5, "CPC464 Plus"),
                              (6, "GX4000"), (0x7f, "machine identity 127")):
        snapshot[0x6D] = machine
        incompatible = root / f"machine-{machine}.sna"
        incompatible.write_bytes(snapshot)
        rejected_machine = run("import-sna", str(incompatible), "--id", "bad_machine",
                               "--title", "Bad Machine",
                               "--output", str(root / f"machine-{machine}.cpcgame"))
        if rejected_machine.returncode == 0 or expected not in rejected_machine.stderr:
            raise SystemExit(f"SNA machine identity {machine} was not rejected clearly")

    # Version 1 predates the machine-identity field. Keep the proven workflow,
    # without treating the otherwise unused byte as CPC model evidence.
    snapshot[16] = 1
    snapshot[0x6D] = 0xff
    v1 = root / "v1.sna"
    v1.write_bytes(snapshot)
    if run("import-sna", str(v1), "--id", "legacy_v1", "--title", "Legacy V1",
           "--output", str(root / "legacy-v1.cpcgame")).returncode:
        raise SystemExit("legacy v1 snapshot workflow regressed")

    if not KNIGHT_SNAPSHOT.is_file():
        raise SystemExit("existing Knight Tyme snapshot is missing")
    knight_package = root / "knight.cpcgame"
    knight = run("import-sna", str(KNIGHT_SNAPSHOT), "--id", "knight_tyme",
                 "--title", "Knight Tyme", "--output", str(knight_package))
    if knight.returncode or run("inspect", str(knight_package)).returncode:
        raise SystemExit("existing Knight Tyme snapshot/package workflow regressed")

    snapshot = bytearray(v1.read_bytes())
    snapshot[107:109] = bytes((128, 0))
    invalid = root / "invalid.sna"
    invalid.write_bytes(snapshot + bytes(65536))
    rejected = run("import-sna", str(invalid), "--id", "bad", "--title", "Bad",
                   "--output", str(root / "bad.cpcgame"))
    if rejected.returncode == 0 or "64 KiB" not in rejected.stderr:
        raise SystemExit("128 KiB snapshot was not rejected")
    unconfirmed_remove = run("remove", "test_game", "--confirm", "NO", "--port", "COM1")
    if unconfirmed_remove.returncode == 0 or "--confirm REMOVE" not in unconfirmed_remove.stderr:
        raise SystemExit("unconfirmed removal was not rejected")
    builtin_remove = run("remove", "knight_tyme", "--confirm", "REMOVE", "--port", "COM1")
    if builtin_remove.returncode == 0 or "cannot be removed" not in builtin_remove.stderr:
        raise SystemExit("built-in removal was not rejected")

print("PASS: deterministic importer, CPC464 machine policy, Knight package, removal confirmation")
