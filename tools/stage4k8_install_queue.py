#!/usr/bin/env python3
"""Fail-closed Stage 4K.8 queue runner using the existing CPCU1 uploader."""

from __future__ import annotations

import argparse
import contextlib
import hashlib
import io
import json
from pathlib import Path
import re
import sys
from datetime import datetime, timezone

sys.path.insert(0, str(Path(__file__).resolve().parent))

import cpc_game


EXPECTED_READY = (
    b"fw=0.4.28-stage4k7migrationrecoverfix",
    b"flash=EF-40-18",
    b"capacity=16777216",
)


def fetch_state(port_name: str) -> dict[str, object]:
    serial, _ = cpc_game.serial_module()
    with serial.Serial(port_name, 115200, timeout=3, write_timeout=2) as port:
        port.reset_input_buffer()
        port.write(cpc_game.frame(1, 0))
        ready = cpc_game.read_protocol_line(port, (b"CPCU1 READY",))
        if not all(token in ready for token in EXPECTED_READY):
            raise RuntimeError(f"unexpected device identity: {ready.decode(errors='replace')}")
        port.write(cpc_game.frame(6, 0))
        status = None
        games: list[dict[str, str]] = []
        while True:
            line = cpc_game.read_protocol_line(
                port, (b"CPCU1 STATUS", b"CPCU1 GAME", b"CPCU1 END"))
            if line.startswith(b"CPCU1 STATUS"):
                status = line.decode("ascii")
            elif line.startswith(b"CPCU1 GAME"):
                match = re.fullmatch(
                    rb"CPCU1 GAME (\d+) id=([^ ]+) title=(.*)", line)
                if match is None:
                    raise RuntimeError(f"malformed game entry: {line!r}")
                games.append({"index": match.group(1).decode(),
                              "id": match.group(2).decode(),
                              "title": match.group(3).decode()})
            elif line == b"CPCU1 END":
                break
        if status is None or not all(token in status for token in
                                     ("upload=idle", "migration=active", "flash=EF-40-18",
                                      "capacity=16777216")):
            raise RuntimeError(f"device is not ready for upload: {status}")
        if len({game["id"] for game in games}) != len(games):
            raise RuntimeError("visible catalogue contains duplicate IDs")
        return {"ready": ready.decode(), "status": status, "games": games}


def load_queue(manifest_path: Path) -> list[dict[str, object]]:
    manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
    queue = []
    for item in manifest["processed"]:
        if item.get("conversion") != "validated_package" or \
                item.get("installation") == "committed_visible":
            continue
        package = manifest_path.parent / item["package"]
        data = package.read_bytes()
        digest = hashlib.sha256(data).hexdigest()
        if digest != item["package_sha256"]:
            raise RuntimeError(f"package SHA-256 mismatch: {package}")
        info = cpc_game.validate_launcher_visibility(data)
        queue.append({"title": item["title"], "category": item["category"],
                      "id": info["id"], "package": str(package),
                      "package_sha256": digest, "bytes": len(data)})
    return queue


def save_progress(path: Path, progress: dict[str, object]) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    temporary = path.with_suffix(path.suffix + ".tmp")
    temporary.write_text(json.dumps(progress, indent=2) + "\n", encoding="utf-8")
    temporary.replace(path)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--port", required=True)
    parser.add_argument("--manifest", required=True, type=Path)
    parser.add_argument("--progress", required=True, type=Path)
    parser.add_argument("--confirm", required=True)
    args = parser.parse_args()
    if args.confirm != "INSTALL_VALIDATED_QUEUE":
        raise RuntimeError("queue installation requires --confirm INSTALL_VALIDATED_QUEUE")
    if args.progress.exists():
        raise RuntimeError("progress file already exists; ambiguous/resumed campaigns require review")

    queue = load_queue(args.manifest)
    if not queue:
        raise RuntimeError("validated waiting queue is empty")
    initial = fetch_state(args.port)
    visible = {game["id"] for game in initial["games"]}
    if len(visible) != 15 or "4kroid" in visible:
        raise RuntimeError(f"unexpected initial visible catalogue: {sorted(visible)}")
    progress: dict[str, object] = {
        "started_utc": datetime.now(timezone.utc).isoformat(),
        "port": args.port, "initial": initial, "queue": queue,
        "successes": [], "state": "running",
    }
    save_progress(args.progress, progress)

    for ordinal, item in enumerate(queue, start=1):
        before = fetch_state(args.port)
        before_ids = {game["id"] for game in before["games"]}
        if before_ids != visible or item["id"] in before_ids:
            raise RuntimeError(f"pre-upload catalogue mismatch before {item['id']}")
        package_path = Path(item["package"])
        package = package_path.read_bytes()
        capture = io.StringIO()
        try:
            with contextlib.redirect_stdout(capture):
                cpc_game.upload(package, args.port, "UPLOAD")
        except Exception as error:
            progress["state"] = "hold"
            progress["failure"] = {"ordinal": ordinal, "id": item["id"],
                                   "error": str(error), "output": capture.getvalue()}
            save_progress(args.progress, progress)
            raise RuntimeError(
                f"upload outcome HOLD at ordinal={ordinal} id={item['id']}: {error}") from error
        output = capture.getvalue()
        print(output, end="")
        after = fetch_state(args.port)
        after_ids = [game["id"] for game in after["games"]]
        if len(after_ids) != len(before_ids) + 1 or set(after_ids) != before_ids | {item["id"]}:
            progress["state"] = "hold"
            progress["failure"] = {"ordinal": ordinal, "id": item["id"],
                                   "error": "post-commit visibility mismatch",
                                   "after": after}
            save_progress(args.progress, progress)
            raise RuntimeError(f"post-commit visibility mismatch for {item['id']}")
        visible = set(after_ids)
        success = dict(item)
        success.update({"ordinal": ordinal, "upload_output": output.strip(),
                        "visible_count": len(after_ids), "status": after["status"]})
        progress["successes"].append(success)
        progress["last_state"] = after
        save_progress(args.progress, progress)
        print(f"VERIFIED {ordinal}/{len(queue)} id={item['id']} visible={len(after_ids)}")

    progress["state"] = "complete"
    progress["completed_utc"] = datetime.now(timezone.utc).isoformat()
    save_progress(args.progress, progress)
    print(f"QUEUE COMPLETE installed={len(queue)} visible={len(visible)}")
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except Exception as error:
        print(f"error: {error}", file=sys.stderr)
        raise SystemExit(1)
