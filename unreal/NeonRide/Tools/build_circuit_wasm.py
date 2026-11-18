"""Rebuild or verify the browser copy of the native driving simulation (Zig 0.14.1)."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import subprocess
import tempfile

PROJECT = Path(__file__).resolve().parents[1]
CORE = PROJECT / "Source" / "NeonRide"
EXPORTS = ("reset_ride", "loop_length", "course_frame", "traffic_car", "drive", "crash_pose")


def digest(path, normalize=False):
    data = path.read_bytes()
    if normalize:
        data = data.replace(b"\r\n", b"\n")
    return hashlib.sha256(data).hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--web", type=Path, default=PROJECT.parents[1] / "web")
    parser.add_argument("--zig", default="zig")
    parser.add_argument("--check", action="store_true")
    args = parser.parse_args()
    wasm = args.web / "circuit.wasm"
    manifest = args.web / "core-version.json"
    if not args.check:
        with tempfile.TemporaryDirectory(dir=args.web) as work:
            obj = Path(work) / "circuit.o"
            binary = Path(work) / wasm.name
            subprocess.run([args.zig, "c++", "-target", "wasm32-freestanding", "-O3",
                            "-fno-exceptions", "-fno-rtti", "-c", CORE / "CircuitCore.cpp",
                            "-o", obj], check=True)
            subprocess.run([args.zig, "wasm-ld", "--no-entry", "--allow-undefined",
                            *("--export=" + name for name in EXPORTS),
                            "--export-memory", "--strip-all", obj, "-o", binary], check=True)
            os.replace(binary, wasm)
    hashes = {name: digest(CORE / name, True) for name in ("CircuitCore.cpp", "CircuitCore.h")}
    hashes[wasm.name] = digest(wasm)
    if args.check:
        if json.loads(manifest.read_text()) != hashes:
            raise SystemExit("Driving core changed. Rebuild circuit.wasm and its manifest.")
    else:
        manifest.write_text(json.dumps(hashes, indent=2) + "\n", encoding="utf-8")


if __name__ == "__main__":
    main()
