#!/usr/bin/env python3
from pathlib import Path
import sys
import tempfile

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "tools"))

import stage4k8_catalogue_audit as audit


with tempfile.TemporaryDirectory() as temporary:
    root = Path(temporary) / "evidence"
    audit.prepare_output(root, resume=False)
    if not root.is_dir():
        raise SystemExit("evidence directory was not created")

    data = bytes((index & 0xFF for index in range(audit.WINDOW_BYTES)))
    path = root / audit.window_name(0, audit.START)
    digest = audit.save_exact(path, data)
    loaded, loaded_digest = audit.load_existing(path, digest)
    if loaded != data or loaded_digest != digest:
        raise SystemExit("saved window did not round-trip")

    audit.prepare_output(root, resume=True)
    try:
        audit.load_existing(path, "0" * 64)
    except RuntimeError:
        pass
    else:
        raise SystemExit("changed manifest hash did not fail closed")

    try:
        audit.save_exact(root / "short.bin", b"short")
    except RuntimeError:
        pass
    else:
        raise SystemExit("short window did not fail closed")

print("stage4k8 catalogue audit evidence tests: PASS")
