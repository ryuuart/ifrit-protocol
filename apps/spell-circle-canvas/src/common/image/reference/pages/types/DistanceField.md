---
kind: type
library: SigilImage
name: DistanceField
qualified: sigil::image::DistanceField
group: Distance fields
status: stable
---

# DistanceField

How far every pixel is from the nearest covered one, in pixels. Zero on
a covered pixel, so comparing the field with a margin is the mask
dilated by a disc of that radius.

## Description

A coverage mask is an image's alpha compared with a tolerance — the
threshold above which a pixel counts as ink. A distance field is the
EXACT Euclidean distance from every pixel to the nearest covered one,
which is what makes "everything within m of this shape" a real disc
offset rather than a square one: corners rounded, a diagonal edge
standing off by m and not by m times the root of two.

It is an image-domain primitive and not a shader: it answers for an
arbitrary raster, which an analytic distance function for a
parameterised shape cannot do. Skia's own field generator is private to
its sources and answers a different question — an 8-bit signed field
around a glyph, quantised for a texture atlas — so it serves neither the
exact distances a margin is measured in nor a raster that is not a
glyph.

## Exact, not approximate

`distanceField` runs two separable passes — a parabola-envelope
transform down each row, then down each column — which answer the true
squared distance for every pixel in time proportional to the raster,
where a chamfer pass would answer an integer approximation of it. A mask
that covers nothing answers every pixel with `DistanceField::kOutside`.

`DistanceField::at` is the distance in pixels at a point, or
`DistanceField::kOutside` off the raster: what a pixel off the raster
answers, since nothing covers it and nothing ever will, so it is further
away than any distance the raster holds. `DistanceField::empty` is
whether the field covers no raster at all, and
`DistanceField::distance` is the row-major plane itself.

## The mask underneath

`Mask` is which pixels a picture covers: one byte a pixel, 1 covered and
0 not, in row-major order — `Mask::covered`, with `Mask::width` and
`Mask::height`. `Mask::at` is whether a pixel is covered, and a pixel
off the raster is not; `Mask::empty` is whether the mask covers no
raster at all.

`coverageMask` thresholds an alpha into one: a pixel is covered when its
alpha is GREATER than the threshold, a fraction of full opacity. A
threshold of 0 admits every pixel the paint touched at all; one of 0.5
admits the pixels an unantialiased rasteriser would have filled, which
puts the mask's edge where the drawn edge is. It reads the alpha channel
alone, so an 8-bit alpha raster and a full-colour one answer alike, and
the image overload reads back to the CPU when the pixels are not already
there — an image that cannot be read answers an empty mask.

## See also

- `field/DistanceField.h` — the header: `DistanceField`, `Mask`,
  `distanceField`, `coverageMask`
- [ImageAsset](ImageAsset.md) — the decoded document a mask is taken
  from
