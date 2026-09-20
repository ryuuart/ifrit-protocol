# The values

What a picture MEANS, for a caller that already has its bytes: the
decoded document and its frames, the named channel planes underneath
them, and the two rasters a silhouette question is answered with. Where
those bytes come from — a path, a URI, a mount, a cache — belongs to
whoever fetched them; nothing here opens a file.

Every value page answers the same three questions in the same order:
**Make one** — every spelling that produces the value; **Pass it to** —
every slot that takes it; and **Also returned by** — what hands one
back.

## The document

| Value | What it is | Header |
|---|---|---|
| [`ImageAsset`](pages/types/ImageAsset.md) | A decoded image document, still or animated, with `Frame` for one composited frame and its duration and `ImageProbe` for the metadata read without a pixel decode. | `asset/ImageAsset.h` |
| [`ChannelData`](pages/types/ChannelData.md) | Every channel the source carries as named interleaved float planes, and the compositing that turns a channel group back into an image. | `decode/ChannelData.h` |

## The silhouette

| Value | What it is | Header |
|---|---|---|
| [`DistanceField`](pages/types/DistanceField.md) | The exact Euclidean distance from every pixel to the nearest covered one, with `Mask` as the coverage it is measured from. | `field/DistanceField.h` |

## The doors

| Function | What it does | Header |
|---|---|---|
| [`embeddedPngs`](pages/functions/embeddedPngs.md) | Every PNG carried inside another file's bytes, found by its own signature — the last resort where a container has no parser here. | `asset/Embedded.h` |

`README.md` beside this file is the library: the two layers and why they
are not interchangeable, the routing order, and the gotchas.

## The headers these come from

- `asset/ImageAsset.h` — `ImageAsset`, `Frame`, `ImageProbe`
- `asset/Embedded.h` — `embeddedPngs`, `EmbeddedImage`, `EmbeddedScan`
- `decode/ChannelData.h` — `ChannelData`
- `decode/Decode.h` — `DecodeOptions`, `decodeImage`, `probeImage`,
  `decodeChannels`, `probeResource`
- `encode/Encode.h` — `Format`, `EncodeOptions`, `encodeImage`,
  `canEncode`, `formatForPath`, `extensionFor`
- `field/DistanceField.h` — `Mask`, `DistanceField`, `coverageMask`,
  `distanceField`
