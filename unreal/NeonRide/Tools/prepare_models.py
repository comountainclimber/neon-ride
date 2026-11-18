"""Build smaller GLBs from the film originals. Requires Pillow, numpy and gltfpack."""
import argparse
from io import BytesIO
import json
from pathlib import Path
import struct
import subprocess
from tempfile import TemporaryDirectory

import numpy as np
from PIL import Image


def prepare(source, destination, packer, temporary):
    raw = source.read_bytes()
    json_size = struct.unpack_from("<I", raw, 12)[0]
    doc = json.loads(raw[20:20 + json_size])
    original = raw[28 + json_size:]
    blob = bytearray()
    name = source.stem
    hero = name in {"bike", "glove", "cab", "truck"}
    edge = 2048 if hero else 1024
    images = {item["bufferView"]: item for item in doc["images"]}
    emissive = None
    for index, view in enumerate(doc["bufferViews"]):
        offset, size = view.get("byteOffset", 0), view["byteLength"]
        data = original[offset:offset + size]
        if index in images:
            image = Image.open(BytesIO(data)).convert("RGB")
            image.thumbnail((edge, edge), Image.Resampling.LANCZOS)
            out = BytesIO()
            if images[index]["mimeType"] == "image/png":
                image.save(out, format="PNG", compress_level=3)
            else:
                image.save(out, format="JPEG", quality=91)
            data = out.getvalue()
            if "basecolor" in images[index]["name"].lower():
                # Approximate the film's strip_glow using saturation and brightness.
                # Write black everywhere outside the painted neon accents.
                pixels = np.asarray(image, dtype=np.float32) / 255
                hi = pixels.max(axis=2)
                sat = (hi - pixels.min(axis=2)) / np.maximum(hi, 1e-6)
                mask = np.clip((sat - 0.32) / 0.43, 0, 1) * np.clip((hi - 0.42) / 0.35, 0, 1)
                light = Image.fromarray((pixels * mask[:, :, None] * 255).astype("uint8"))
                stream = BytesIO()
                light.save(stream, format="JPEG", quality=90)
                emissive = stream.getvalue()
        blob.extend(bytes(-len(blob) % 4))
        view.update(byteOffset=len(blob), byteLength=len(data))
        blob.extend(data)
    if emissive:
        blob.extend(bytes(-len(blob) % 4))
        view_index = len(doc["bufferViews"])
        doc["bufferViews"].append({"buffer": 0, "byteOffset": len(blob), "byteLength": len(emissive)})
        blob.extend(emissive)
        image_index = len(doc["images"])
        doc["images"].append({"name": name + "_emissive", "bufferView": view_index, "mimeType": "image/jpeg"})
        texture_index = len(doc["textures"])
        doc["textures"].append({"source": image_index})
        for material in doc["materials"]:
            material.update(emissiveTexture={"index": texture_index}, emissiveFactor=[1, 1, 1])
            material.setdefault("extensions", {})["KHR_materials_emissive_strength"] = {"emissiveStrength": 3.0}
        if "KHR_materials_emissive_strength" not in doc.setdefault("extensionsUsed", []):
            doc["extensionsUsed"].append("KHR_materials_emissive_strength")
    # Use stable names for the generated Unreal assets.
    doc["meshes"][0]["name"] = "SM_" + name.replace("-", "_")
    doc["nodes"][0]["name"] = doc["meshes"][0]["name"]
    doc["buffers"][0]["byteLength"] = len(blob)
    blob.extend(bytes(-len(blob) % 4))
    header = json.dumps(doc, separators=(",", ":")).encode()
    header += b" " * (-len(header) % 4)
    temp = temporary / source.name
    temp.write_bytes(struct.pack("<4sIIII", b"glTF", 2, 28+len(header)+len(blob), len(header), 0x4E4F534A)
                     + header + struct.pack("<II", len(blob), 0x004E4942) + blob)
    ratio = 0.40 if name in {"bike", "glove"} else (0.32 if hero else 0.12)
    subprocess.run([str(packer), "-i", str(temp), "-o", str(destination / source.name),
                    "-si", str(ratio), "-sp", "-noq", "-kn"], check=True)


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("source", type=Path)
    parser.add_argument("destination", type=Path)
    parser.add_argument("--gltfpack", type=Path, required=True)
    args = parser.parse_args()
    args.destination.mkdir(parents=True, exist_ok=True)
    with TemporaryDirectory(dir=args.destination) as folder:
        for model in sorted(args.source.glob("*.glb")):
            prepare(model, args.destination, args.gltfpack.resolve(), Path(folder))
            print(model.name, flush=True)
