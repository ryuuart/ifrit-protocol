# The values

What a picture MEANS, for a caller that already has its bytes: the
decoded documents and their frames, the named channel planes underneath
an image, and the two rasters a silhouette question is answered with.
Where those bytes come from — a path, a URI, a mount, a cache — belongs
to whoever fetched them; nothing here opens a file.

Every value page answers the same three questions in the same order:
**Make one** — every spelling that produces the value; **Pass it to** —
every slot that takes it; and **Also returned by** — what hands one
back.

## The documents

| Value | What it is | Header |
|---|---|---|
| [`Image`](pages/types/Image.md) | A decoded image document, still or animated, with `Frame` for one composited frame and where it stands, and `Metadata` for what it says without a pixel decode. | `core/Image.h` |
| [`Channels`](pages/types/Channels.md) | Every channel the source carries as named interleaved float planes, and the compositing that turns a channel group back into a picture. | `image/Channels.h` |

`Video` is the streaming document, a clip whose frames decode around a
playhead; `VIDEO.md` beside the library is its chapter.

## The silhouette

| Value | What it is | Header |
|---|---|---|
| [`DistanceField`](pages/types/DistanceField.md) | The exact Euclidean distance from every pixel to the nearest covered one, with `Mask` as the coverage it is measured from. | `field/DistanceField.h` |

## The doors

| Function | What it does | Header |
|---|---|---|
| [`embeddedImages`](pages/functions/embeddedImages.md) | Every image carried inside another file's bytes, found by its own signature — the last resort where a container has no parser here. | `advanced/Embedded.h` |

`README.md` beside this directory is the library: the common cases, the
options, the control, and the boundary; `IMAGE.md` and `VIDEO.md` are
its chapters.

## The headers these come from

- `core/Image.h` — `Image`, `ImageOptions`
- `core/Frame.h` — `Frame`, `Timing`, `Loop`
- `core/Metadata.h` — `Metadata`
- `advanced/Embedded.h` — `embeddedImages`, `EmbeddedImage`, `EmbeddedScan`
- `image/Channels.h` — `Channels`, `ChannelPick`
- `field/DistanceField.h` — `Mask`, `DistanceField`, `FieldOptions`,
  `coverageMask`, `distanceField`
