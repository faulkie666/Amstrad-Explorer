#!/usr/bin/env python3
"""Wait for a CPC Explorer USB reconnect and capture its initial CDC output."""

from pathlib import Path
import sys
import time

import serial
from serial.tools import list_ports


VID = 0x2E8A
PID = 0x0009


def find_port():
    for port in list_ports.comports():
        if port.vid == VID and port.pid == PID:
            return port.device
    return None


def wait_until(predicate, timeout, message):
    deadline = time.monotonic() + timeout
    while time.monotonic() < deadline:
        value = predicate()
        if value:
            return value
        time.sleep(0.1)
    raise TimeoutError(message)


def main():
    output = Path(sys.argv[1] if len(sys.argv) > 1 else "first-hardware-boot.log")
    print("Waiting for current CPC Explorer USB device to disconnect...", flush=True)
    wait_until(lambda: find_port() is None, 90, "device did not disconnect")
    print("Disconnected; waiting for firmware USB to enumerate...", flush=True)
    port = wait_until(find_port, 30, "firmware USB did not re-enumerate")
    print(f"Opening {port}", flush=True)
    chunks = []
    with serial.Serial(port, 115200, timeout=0.05) as stream:
        stream.dtr = True
        deadline = time.monotonic() + 15
        while time.monotonic() < deadline:
            data = stream.read(4096)
            if data:
                chunks.append(data)
                print(data.decode("utf-8", "replace"), end="", flush=True)
    payload = b"".join(chunks)
    output.write_bytes(payload)
    print(f"\nCaptured {len(payload)} bytes to {output}", flush=True)
    return 0 if payload else 2


if __name__ == "__main__":
    raise SystemExit(main())
