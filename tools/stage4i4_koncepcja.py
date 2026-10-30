#!/usr/bin/env python3
"""Deterministic localhost-only konCePCja automation for Stage 4I.4.

This helper deliberately operates on caller-supplied scratch media.  It never
opens the CPC Explorer USB protocol and has no device-access functionality.
"""

from __future__ import annotations

import argparse
import ctypes
import json
import queue
import socket
import subprocess
import threading
import time
from pathlib import Path


class Emulator:
    def __init__(
        self, exe: Path, cfg: Path, cwd: Path, media: Path | None,
        gui_surface: bool = False,
    ):
        self.lines: queue.Queue[str] = queue.Queue()
        self.output: list[str] = []
        args = [str(exe.resolve())]
        if not gui_surface:
            args.append("--headless")
        args += [
            f"--cfg_file={cfg.resolve()}",
            "--override=system.model=0",
            "--override=system.ram_size=64",
            "--override=rom.slot07=amsdos.rom",
        ]
        if media is not None:
            args.append(str(media.resolve()))
        startupinfo = None
        if gui_surface and hasattr(subprocess, "STARTUPINFO"):
            startupinfo = subprocess.STARTUPINFO()
            startupinfo.dwFlags |= subprocess.STARTF_USESHOWWINDOW
            startupinfo.wShowWindow = 0
        self.process = subprocess.Popen(
            args,
            cwd=str(cwd.resolve()),
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
            text=True,
            encoding="utf-8",
            errors="replace",
            bufsize=1,
            startupinfo=startupinfo,
        )
        assert self.process.stdout is not None
        self.reader = threading.Thread(target=self._read_output, daemon=True)
        self.reader.start()
        self.port, self.manifest = self._wait_manifest()

    def _read_output(self) -> None:
        assert self.process.stdout is not None
        for raw in self.process.stdout:
            line = raw.rstrip("\r\n")
            self.output.append(line)
            self.lines.put(line)

    def _wait_manifest(self, timeout: float = 15.0) -> tuple[int, list[str]]:
        deadline = time.monotonic() + timeout
        manifest: list[str] = []
        collecting = False
        while time.monotonic() < deadline:
            if self.process.poll() is not None:
                raise RuntimeError(
                    f"konCePCja exited before manifest: {self.process.returncode}"
                )
            try:
                line = self.lines.get(timeout=0.1)
            except queue.Empty:
                continue
            if line == "--- # koncepcja":
                collecting = True
            if collecting:
                manifest.append(line)
                if line == "...":
                    break
        else:
            raise TimeoutError("timed out waiting for konCePCja manifest")

        port = None
        in_ports = False
        for line in manifest:
            if line == "ports:":
                in_ports = True
            elif in_ports and line.startswith("  ipc:"):
                port = int(line.split(":", 1)[1].strip())
                break
        if port is None:
            raise RuntimeError("manifest did not contain an IPC port")
        return port, manifest

    def command(self, command: str, timeout: float = 10.0) -> str:
        started = time.monotonic()
        with socket.create_connection(("127.0.0.1", self.port), timeout=timeout) as sock:
            sock.settimeout(min(timeout, 1.0))
            sock.sendall(command.encode("utf-8") + b"\n")
            sock.shutdown(socket.SHUT_WR)
            chunks: list[bytes] = []
            while time.monotonic() - started < timeout:
                try:
                    chunk = sock.recv(65536)
                except socket.timeout:
                    if chunks:
                        break
                    continue
                if not chunk:
                    break
                chunks.append(chunk)
        response = b"".join(chunks).decode("utf-8", errors="replace").strip()
        if not response:
            raise RuntimeError(f"empty IPC response for {command!r}")
        return response

    def close(self) -> None:
        if self.process.poll() is None:
            try:
                self.command("quit 0", timeout=3.0)
            except Exception:
                self.process.terminate()
        try:
            self.process.wait(timeout=5.0)
        except subprocess.TimeoutExpired:
            self.process.kill()
            self.process.wait(timeout=5.0)


def quoted(path: Path) -> str:
    resolved = str(path.resolve())
    if hasattr(ctypes, "windll"):
        target = resolved
        suffix = ""
        if not path.exists():
            target = str(path.resolve().parent)
            suffix = path.name
        buffer = ctypes.create_unicode_buffer(32768)
        length = ctypes.windll.kernel32.GetShortPathNameW(
            target, buffer, len(buffer)
        )
        if 0 < length < len(buffer):
            resolved = str(Path(buffer.value) / suffix) if suffix else buffer.value
    # konCePCja's path-taking IPC commands do not unquote Windows paths.  Use
    # the short form so the command remains a single token without quotes.
    return resolved.replace("\\", "/")


def run_probe(args: argparse.Namespace) -> int:
    emu = Emulator(args.exe, args.cfg, args.cwd, args.media, args.gui_surface)
    try:
        results = {
            "manifest": emu.manifest,
            "version": emu.command("version"),
            "model": emu.command("config get model"),
            "ram": emu.command("config get ram_size"),
            "roms": emu.command("rom list"),
            "drive": emu.command("disk status A"),
        }
        if args.media is not None:
            results["files"] = emu.command("disk ls A")
        print(json.dumps(results, indent=2))
    finally:
        emu.close()
    return 0


def run_classic(args: argparse.Namespace) -> int:
    if args.media is None:
        raise SystemExit("classic requires --media pointing at a scratch DSK")
    args.output.mkdir(parents=True, exist_ok=True)
    emu = Emulator(args.exe, args.cfg, args.cwd, args.media, args.gui_surface)
    transcript: list[dict[str, str | float]] = []

    def send(command: str, timeout: float = 10.0) -> str:
        response = emu.command(command, timeout)
        transcript.append(
            {"time": time.time(), "command": command, "response": response}
        )
        if response.startswith("ERR"):
            raise RuntimeError(f"{command}: {response}")
        return response

    try:
        send("wait vbl 100 10000", 15.0)
        send("config get model")
        send("config get ram_size")
        send("rom list")
        send("disk status A")
        send("disk ls A")
        send(f"screenshot {quoted(args.output / '00-basic.png')}")

        send('autotype RUN"INVADERS"~ENTER~')
        send("wait vbl 500 30000", 35.0)
        send(f"screenshot {quoted(args.output / '01-infinite-lives.png')}")
        send("hash vram")

        send("input key N hold=5", 10.0)
        send("wait vbl 50 10000", 15.0)
        send(f"screenshot {quoted(args.output / '02-start-level.png')}")
        send("hash vram")

        # Keep the loader's firmware key scan on the faithful execution tier.
        # Auto-tier breakpoint insertion changes the execution mode at exactly
        # this prompt, so first prove the unmodified loader transition without
        # a debugger breakpoint in the keyboard path.
        send("input key 1 hold=10", 15.0)
        # The tap advances only ten frames; arm the entry breakpoint after the
        # prompt has consumed the key but while AMSDOS is still loading BIN.
        send("bp add 0x4200")
        hit = send("wait bp 30000", 35.0)
        if "PC=4200" not in hit.upper():
            raise RuntimeError(f"loader did not reach game entry: {hit}")
        send("bp del 0x4200")
        send("wait vbl 250 20000", 25.0)
        send(f"screenshot {quoted(args.output / '03-post-level-choice.png')}")
        send("record ym start " + quoted(args.output / "classic-invaders.ym"))
        send("wait vbl 750 30000", 35.0)
        send(f"screenshot {quoted(args.output / '03-gameplay.png')}")
        send("hash vram")
        send("hash mem 0 65536")
        send("regs")
        send("regs psg")
        send("record ym stop")
        send("snapshot save " + quoted(args.output / "classic-invaders.sna"))

        # Prove that the captured state does not need the scratch disk for the
        # following emulated 20 seconds.
        send("disk eject A")
        send("wait vbl 1000 40000", 45.0)
        send(f"screenshot {quoted(args.output / '04-diskless.png')}")
        send("hash vram")
        send("hash mem 0 65536")
    finally:
        (args.output / "ipc-transcript.json").write_text(
            json.dumps(
                {
                    "manifest": emu.manifest,
                    "commands": transcript,
                    "emulator_output": emu.output,
                },
                indent=2,
            ),
            encoding="utf-8",
        )
        emu.close()
    return 0


def run_classic_start_probe(args: argparse.Namespace) -> int:
    if args.media is None:
        raise SystemExit("classic-start-probe requires --media pointing at the saved SNA")
    args.output.mkdir(parents=True, exist_ok=True)
    emu = Emulator(args.exe, args.cfg, args.cwd, None, args.gui_surface)
    transcript: list[dict[str, str | float]] = []

    def send(command: str, timeout: float = 10.0) -> str:
        response = emu.command(command, timeout)
        transcript.append(
            {"time": time.time(), "command": command, "response": response}
        )
        if response.startswith("ERR"):
            raise RuntimeError(f"{command}: {response}")
        return response

    try:
        send("snapshot load " + quoted(args.media))
        send("wait vbl 100 10000", 15.0)
        send(f"screenshot {quoted(args.output / '00-restored-title.png')}")
        send("hash vram")
        send("input key SPACE hold=5", 10.0)
        send("wait vbl 250 20000", 25.0)
        send(f"screenshot {quoted(args.output / '01-after-space.png')}")
        send("hash vram")
        send("input joy 0 F1")
        send("wait vbl 5 10000", 15.0)
        send("input joy 0 -F1")
        send("wait vbl 250 20000", 25.0)
        send(f"screenshot {quoted(args.output / '02-after-joy-fire.png')}")
        send("hash vram")
        send("record ym start " + quoted(args.output / "classic-gameplay.ym"))
        send("wait vbl 1000 40000", 45.0)
        send("record ym stop")
        send(f"screenshot {quoted(args.output / '03-gameplay-run.png')}")
        send("hash vram")
        send("hash mem 0 65536")
        send("regs")
        send("regs psg")
        send("snapshot save " + quoted(args.output / "classic-invaders-gameplay.sna"))
    finally:
        (args.output / "ipc-transcript.json").write_text(
            json.dumps(
                {
                    "manifest": emu.manifest,
                    "commands": transcript,
                    "emulator_output": emu.output,
                },
                indent=2,
            ),
            encoding="utf-8",
        )
        emu.close()
    return 0


def run_game_capture(args: argparse.Namespace) -> int:
    if args.media is None or not args.run_name:
        raise SystemExit("game-capture requires --media and --run-name")
    args.output.mkdir(parents=True, exist_ok=True)
    emu = Emulator(args.exe, args.cfg, args.cwd, args.media, args.gui_surface)
    transcript: list[dict[str, str | float]] = []

    def send(command: str, timeout: float = 10.0) -> str:
        response = emu.command(command, timeout)
        transcript.append(
            {"time": time.time(), "command": command, "response": response}
        )
        if response.startswith("ERR"):
            raise RuntimeError(f"{command}: {response}")
        return response

    try:
        send("wait vbl 100 10000", 15.0)
        send(f"screenshot {quoted(args.output / '00-basic.png')}")
        send(f'autotype RUN"{args.run_name}"~ENTER~')
        send("wait vbl 1500 60000", 65.0)
        send(f"screenshot {quoted(args.output / '01-loaded-30s.png')}")
        send("hash vram")
        send("hash mem 0 65536")
        send("regs")
        send("regs psg")

        send("record ym start " + quoted(args.output / f"{args.run_name.lower()}-60s.ym"))
        send("wait vbl 3000 90000", 95.0)
        send("record ym stop")
        send(f"screenshot {quoted(args.output / '02-loaded-90s.png')}")
        send("hash vram")
        send("hash mem 0 65536")
        send("regs psg")

        if args.capture_keys:
            joystick_inputs = {
                "JOYF": "F1", "JOYL": "LEFT", "JOYR": "RIGHT",
                "JOYU": "UP", "JOYD": "DOWN",
            }
            for index, raw_key in enumerate(args.capture_keys.split(","), start=3):
                key = raw_key.strip()
                if not key:
                    continue
                joystick_input = joystick_inputs.get(key.upper())
                if joystick_input is not None:
                    send(f"input joy 0 {joystick_input}")
                    send(f"wait vbl {args.key_hold_vbl} 10000", 15.0)
                    send(f"input joy 0 -{joystick_input}")
                else:
                    send(f"input key {key} hold={args.key_hold_vbl}", 10.0)
                send(f"wait vbl {args.observe_vbl} 30000", 35.0)
                send(f"screenshot {quoted(args.output / f'{index:02d}-after-{key}.png')}")
                send("hash vram")
                send("hash mem 0 65536")
        else:
            send("input key SPACE hold=5", 10.0)
            send("wait vbl 250 20000", 25.0)
            send(f"screenshot {quoted(args.output / '03-after-space.png')}")
            send("hash vram")

            send("input joy 0 F1")
            send("wait vbl 5 10000", 15.0)
            send("input joy 0 -F1")
            send("wait vbl 500 30000", 35.0)
            send(f"screenshot {quoted(args.output / '04-after-joy-fire.png')}")
            send("hash vram")
            send("hash mem 0 65536")

        send("hash vram")
        send("hash mem 0 65536")
        send("snapshot save " + quoted(args.output / f"{args.run_name.lower()}-candidate.sna"))

        send("disk eject A")
        send("wait vbl 1000 40000", 45.0)
        send(f"screenshot {quoted(args.output / '05-diskless-20s.png')}")
        send("hash vram")
        send("hash mem 0 65536")
        send("snapshot save " + quoted(args.output / f"{args.run_name.lower()}-diskless.sna"))
    finally:
        (args.output / "ipc-transcript.json").write_text(
            json.dumps(
                {
                    "manifest": emu.manifest,
                    "commands": transcript,
                    "emulator_output": emu.output,
                },
                indent=2,
            ),
            encoding="utf-8",
        )
        emu.close()
    return 0


def run_snapshot_probe(args: argparse.Namespace) -> int:
    if args.media is None or not args.keys:
        raise SystemExit("snapshot-probe requires --media and --keys")
    args.output.mkdir(parents=True, exist_ok=True)
    emu = Emulator(
        args.exe, args.cfg, args.cwd, args.disk_media, args.gui_surface
    )
    transcript: list[dict[str, str | float]] = []

    def send(command: str, timeout: float = 10.0) -> str:
        response = emu.command(command, timeout)
        transcript.append(
            {"time": time.time(), "command": command, "response": response}
        )
        if response.startswith("ERR"):
            raise RuntimeError(f"{command}: {response}")
        return response

    try:
        send("snapshot load " + quoted(args.media))
        send("wait vbl 100 10000", 15.0)
        send(f"screenshot {quoted(args.output / '00-restored.png')}")
        send("record ym start " + quoted(args.output / "probe.ym"))
        joystick_inputs = {
            "JOYF": "F1",
            "JOYL": "LEFT",
            "JOYR": "RIGHT",
            "JOYU": "UP",
            "JOYD": "DOWN",
        }
        for index, key in enumerate(args.keys.split(","), start=1):
            key = key.strip()
            if not key:
                continue
            joystick_input = joystick_inputs.get(key.upper())
            if joystick_input is not None:
                send(f"input joy 0 {joystick_input}")
                send("wait vbl 5 10000", 15.0)
                send(f"input joy 0 -{joystick_input}")
            else:
                send(f"input key {key} hold={args.key_hold_vbl}", 10.0)
            send(f"wait vbl {args.observe_vbl} 30000", 35.0)
            send(f"screenshot {quoted(args.output / f'{index:02d}-after-{key}.png')}")
            send("hash vram")
            send("hash mem 0 65536")
            send("regs psg")
        send("record ym stop")
        send("snapshot save " + quoted(args.output / "probe-final.sna"))
    finally:
        (args.output / "ipc-transcript.json").write_text(
            json.dumps(
                {
                    "manifest": emu.manifest,
                    "commands": transcript,
                    "emulator_output": emu.output,
                },
                indent=2,
            ),
            encoding="utf-8",
        )
        emu.close()
    return 0


def parser() -> argparse.ArgumentParser:
    result = argparse.ArgumentParser()
    result.add_argument(
        "action",
        choices=(
            "probe", "classic", "classic-start-probe", "game-capture",
            "snapshot-probe",
        ),
    )
    result.add_argument("--exe", type=Path, required=True)
    result.add_argument("--cfg", type=Path, required=True)
    result.add_argument("--cwd", type=Path, required=True)
    result.add_argument("--media", type=Path)
    result.add_argument(
        "--disk-media", type=Path,
        help="optional DSK to keep mounted while probing a restored snapshot",
    )
    result.add_argument("--output", type=Path, default=Path("stage4i4-output"))
    result.add_argument("--run-name")
    result.add_argument("--keys")
    result.add_argument(
        "--capture-keys",
        help="optional comma-separated bounded inputs applied during game-capture while the DSK remains mounted",
    )
    result.add_argument(
        "--observe-vbl", type=int, default=500,
        help="emulated VBL intervals observed after each deterministic input",
    )
    result.add_argument(
        "--key-hold-vbl", type=int, default=5,
        help="VBL intervals to hold each snapshot-probe keyboard input (default: 5)",
    )
    result.add_argument(
        "--gui-surface", action="store_true",
        help="create a hidden render surface for IPC screenshots; all input remains IPC",
    )
    return result


if __name__ == "__main__":
    parsed = parser().parse_args()
    raise SystemExit(
        {
            "probe": run_probe,
            "classic": run_classic,
            "classic-start-probe": run_classic_start_probe,
            "game-capture": run_game_capture,
            "snapshot-probe": run_snapshot_probe,
        }[parsed.action](parsed)
    )
