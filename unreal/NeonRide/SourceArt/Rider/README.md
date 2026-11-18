# Vesper

Vesper is the helmeted test rider for the Midnight Circuit. The selected character reference was used in Tripo Studio to generate the high-resolution sculpt. The two game exports share its retopologized mesh, PBR surface and Mixamo-compatible skin.

| Asset | Purpose |
| --- | --- |
| `vesper-animated.glb` | Native engine import with the riding pose and 3.2-second ejection clip |
| `vesper-brace.glb` | Static brace pose baked from the ejection clip |
| [Browser model](../../../../web/assets/rider/vesper.glb) | Same rider, compressed for delivery to WebGL |

The standing rider is 1.75 m tall. The glTF files use Y-up, +Z forward and a ground-level origin. The runtime mesh has 19,768 triangles and 53 weighted joints. The shared driving code supplies the trajectory and timing of each ejection.
