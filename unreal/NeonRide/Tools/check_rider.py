"""Check that compression retained the native Vesper rig and its animation tracks."""
import argparse
import json
from pathlib import Path
import struct


def signature(path):
    with path.open("rb") as stream:
        magic, version, size, chunk_size, chunk = struct.unpack("<4sIIII", stream.read(20))
        if (magic, version, chunk, size) != (b"glTF", 2, 0x4E4F534A, path.stat().st_size):
            raise ValueError(f"Invalid glTF container: {path}")
        doc = json.loads(stream.read(chunk_size))
    nodes, accessors = doc["nodes"], doc["accessors"]
    joints = sorted(nodes[i]["name"] for i in doc["skins"][0]["joints"])
    triangles = sum(accessors[p["indices"]]["count"] // 3
                    for mesh in doc["meshes"] for p in mesh["primitives"])
    clips = {}
    for clip in doc["animations"]:
        duration = max(accessors[s["input"]]["max"][0] for s in clip["samplers"])
        targets = [(nodes[c["target"]["node"]]["name"], c["target"]["path"]) for c in clip["channels"]]
        clips[clip["name"]] = (round(duration, 4), sorted(targets))
    return joints, triangles, clips


if __name__ == "__main__":
    project = Path(__file__).resolve().parents[1]
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--web", type=Path, default=project.parents[1] / "web")
    args = parser.parse_args()
    native = signature(project / "SourceArt/Rider/vesper-animated.glb")
    web = signature(args.web / "assets/rider/vesper.glb")
    if native != web or "Riding" not in native[2] or native[2].get("CrashEject", (0,))[0] != 3.2:
        raise SystemExit("Vesper rig, geometry or animation differs between the two builds.")
    print(f"{len(web[0])} joints, {web[1]} triangles, matching riding and ejection tracks")
