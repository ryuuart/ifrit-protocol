# Findings

Defects found while working, stated as what the code does, what it was
evidently intended to do, and what a test should assert once intent is
restored. A work queue: delete an entry when it is fixed, and delete this
file when it is empty.

## rota_convocationis is over its budget after its opening

`--bench --sketch rota_convocationis` passes on the sketch's own declared
moment (p99 11.3 ms of a 16.6 ms budget), and the twelve sub-seals no
longer add to the per-frame cost — the seal cycle grows the frame by 3 ms
where it grew it by 18. What the entry that stood here did not cover:
from about six seconds in the plate is over budget anyway, and by the
seal cycle's end it sits near 35 ms with no single node responsible.
`--bench --at 9`, `--at 12` and `--at 15` report p50 31.5 / 34.0 /
35.3 ms.

The cost is a long tail of full-canvas live paints — the emissive stacks
(`stella`, `arcus`, `star-lit`, `inner-lit`, the `rim-lit` and `nom-lit`
grades), each a `Baked` path filled at `kPlus` over the whole 1280 px
canvas, four per glow — plus three text rings that replay a picture every
frame (`vox`, `registrum`, `textura`) because their placement is driven
by a live path phase. The names' band was the fourth and is fixed: it is
turned as a body and baked. The same conversion on the other three is NOT
a win as things stand — a ring turned by a bound rotation is
`transformLive`, so its bake is held in local space and the blit resamples
a 650–1000 px image through the rotation, which costs more than the
replay it replaced.

Intended: either the emissive grades are cheap enough that twenty of them
fit in a frame, or a ring turned by a declared rotation can hold a bake
the blit does not resample. The second is the library half and the more
useful one: a device-space bake is refused to a node whose transform is
live, and yet a rotation about the node's own centre moves no pixel of
the bake's CONTENT — only where it lands.

Assert once fixed: `--bench --at 9`, `--at 12` and `--at 15` all verdict
PASS at 1280x1280, and the per-frame report shows no full-canvas live
paint in the emissive stacks.
