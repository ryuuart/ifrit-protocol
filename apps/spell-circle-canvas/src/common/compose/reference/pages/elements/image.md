---
kind: element
library: SigilCompose
name: image
qualified: sigil::compose::image
header: sigilcompose/core/Factories.h
group: Leaves
python: sigil.compose.image
status: stable
example: image_element
common_verbs: [sampling, region, width, height, corners, clip, boundary]
---

# image

A picture as a leaf: an asset the resource layer decoded, or a picture
already rendered — a bake taken on an intermediate surface, a frame out
of a file, a texture a device handed back.

<!-- example: image_element -->

## Syntax

```cpp
Element image(std::shared_ptr<const sigil::image::ImageAsset> asset);
Element image(sk_sp<SkImage> picture,
              material::skia::Fit fit = material::skia::Fit::Contain);
```

```python
def image(image: skia.Image,
          fit: material.Fit = material.Fit.Contain) -> Element: ...
```

## Parameters

| Parameter | What it is | Where one comes from |
|---|---|---|
| `asset` | A decoded image with its own identity. | SigilImage's asset vocabulary, through the sketch's assets |
| `picture` | A picture already rendered. | A snapshot, a decode, a device texture |
| `fit` | How the picture meets the box it is given. | [`material::skia::Fit`](../../ELEMENTS.md#the-kernel): `Stretch`, `Contain`, `Cover`, `Native` |

## Description

**The asset leaf takes its intrinsic size from the asset** and is sized
like any other node from there.

**The picture leaf takes the box it stands in** under every fit, and
under `Contain` and `Cover` the node itself carries the picture's
proportions, so what is painted is the whole of the node. `Stretch`
scales both axes independently; `Contain` keeps the proportions and
leaves the slack on the long axis; `Cover` leaves no slack and lets
whatever clips the node crop the overflow. A null picture is an empty
box.

**Sampling is inherited, and the default is wrong for pixels.** Linear
is right for a photograph and wrong for every grid of pixels — art,
tilemaps, fonts baked as sprites, simulation buffers. State
`sampling(SkSamplingOptions(SkFilterMode::kNearest))` once on the panel
that holds them and every image leaf under it takes it, as CSS inherits
`image-rendering`.

**`region` draws one sub-rect of the source** — an atlas cell, a sprite
frame — strictly constrained, so neighbouring cells never bleed in.

A node whose `boundary` is `Boundary::Coverage` hands its decorations
the silhouette of what it drew rather than its box, which is what dresses
a cut-out and what text flows around.

## Examples

- `reference/examples/image_element.cpp` — one checker, built rather
  than loaded, under each of the three fits with nearest sampling.
- `reference/examples/image_element.py` — the same picture in Python.

## See also

`picture` for a recorded `SkPicture` as a leaf, `Slice` for an image
laid onto a box through a lattice, [`custom`](custom.md) for content
drawn rather than sampled, and the *Content* group on [the verb
index](../../VERBS.md).
