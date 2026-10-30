#!/usr/bin/env python3
"""Focused filesystem regression for Stage 4I.12V.1 evidence persistence."""

from pathlib import Path
import sys
import tempfile

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "tools"))

from stage4i12v_readback import prepare_evidence_dir, save_binary


with tempfile.TemporaryDirectory() as temporary:
    evidence = Path(temporary) / "nested" / "evidence"
    prepare_evidence_dir(evidence)
    assert evidence.is_dir()
    assert list(evidence.iterdir()) == []
    payload = bytes(range(256)) * 16
    digest = save_binary(evidence / "window.bin", payload)
    assert (evidence / "window.bin").read_bytes() == payload
    assert digest == "c8f5d0341d54d951a71b136e6e2afcb14d11ed8489a7ae126a8fee0df6ecf193"
    prepare_evidence_dir(evidence, resume=True)
    assert (evidence / "window.bin").read_bytes() == payload

print("PASS: evidence directory creation, probe cleanup and binary save/readback")
