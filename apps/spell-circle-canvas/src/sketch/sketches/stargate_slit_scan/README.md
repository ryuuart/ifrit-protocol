# Passage

An original 28-second optical film study inspired by the slit-scan
passages in *2001: A Space Odyssey*. It develops light as architecture:
vertical walls, a horizontal passage and a luminous doorway. The title,
source motifs, camera trajectory, colors and sequence are authored for
the study, rather than a reconstruction of a specific shot.

The film occupies a 2:1 canvas. Its opening title recedes as the image
arrives; the passage itself has no explanatory interface. A short end
card follows the exposure's fade.

## Construction

The Python sketch retains one shader-filled native Compose box and two
typographic layers. One elapsed clock drives travel, the image's exposure
envelope and the title cards. The optical field is procedural SkSL.
Native bloom contributes near halation and a wider tail. The emulsion
filter modulates luminance with fine frame-stepped grain while preserving
the black void.

The source artwork combines three fiber scales with unequal widths,
warped spacing, dropout, scumbled brightness and broader veils. The fine
traces approach average coverage as their projected footprint becomes
unresolved. Three neighboring source samples soften the slit aperture;
they are spatial samples, not samples of previous rendered frames.
Different source offsets and exposure gains give the two sides distinct
structure. A quarter turn changes the vertical field into horizontal
rails, followed by the larger light masses of the final opening.

The image uses an analytic projection of procedural source strips. It
can evaluate its picture at a requested scene time without first
rendering every preceding frame. It does not photograph physical artwork,
store previous rendered frames, or simulate film chemistry. Exposure
trails are part of the authored spatial construction, not an assertion
that the renderer has temporal feedback.

## Render

From the application directory:

```sh
build/bin/Release/Sketchbook.app/Contents/MacOS/Sketchbook \
  src/sketch/sketches/stargate_slit_scan/stargate_slit_scan.py \
  --frame /tmp/passage.png --at 8
```

Scene times 1.8, 8, 16 and 23 show the title and three principal optical
compositions. The file capture lane draws 2D canvases on Raster and adds
the host's still step to the requested timestamp. A held native Graphite
window is separate backend evidence.

The loop lasts 28 seconds. Its opening and closing black frames make the
clock reset explicit; the underlying trajectory is not periodic.

## Reference and interpretation

Douglas Trumbull describes moving the camera and artwork relative to one
another during an open-shutter exposure. That relationship motivates the
study's stretched source strips and separate travel clock. The shader's
analytic image is an interpretation of the resulting spatial language,
not a calibrated model of the physical apparatus.
[First-person production account](https://www.amc.com/blogs/james-camerons-story-of-science-fiction-qa-douglas-trumbull-visual-effects-director--1005555)

The inspected film frame has unequal magenta and orange structures,
brilliant pale cores, a small cyan cluster, broken fine traces and large
black regions. These inform the contrast and asymmetry. A still does not
establish the original shot's speed or transitions; this study's sequence
and typography are original.
[Inspected film frame](https://images.ctfassets.net/1aemqu6a6t65/6X5DqgtjX6dqdxLRbXQAol/5294ff2d03825189203eafff500a5760/stanley-kubrick-2001-space-odyssey-astoria-queens-nyc-3785d92085f003bad76dca834cc3ed07)

The adjacent `slitscan_2001` sketch already constructs weighted exposure
stamps with native instancing and presents its rig and exposure analysis.
Those seams are available in Python too. This study uses a continuous
procedural optical field for an uninterrupted cinematic composition;
it does not introduce a second instancing or accumulation abstraction.
