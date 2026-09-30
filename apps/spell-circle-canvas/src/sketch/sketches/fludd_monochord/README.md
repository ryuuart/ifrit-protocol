# Cosmic monochord

A physical engraving leaf and a native facing folio after Robert Fludd's
*Monochordum mundanum*. The print preserves the tuning hand, cloud hatching,
instrument scroll, planetary signs, stellar belt and curved inscriptions.
The facing leaf connects the historical interval vocabulary to a living
string with fixed endpoints and visible nodes.

## Source and interpretation

The bundled public-domain engraving in `data/monochord.jpeg` comes from the
[Public Domain Review's plate](https://publicdomainreview.org/essay/robert-fludd-and-his-images-of-the-divine/).
The complete image was inspected directly.
[Cornell's object record](https://digital.library.cornell.edu/catalog/ss%3A18168108)
identifies the diagram and publication. The image is a historical print,
not an independently drawn facsimile.

The shader separates the dark ink from the scan's paper. Its retained
linework is printed onto the generated ivory receiving leaf, with coverage
variation, fibre texture, a second cut edge and a contact shadow. This
preserves the source's fine hatching without treating a cropped figure as
a rectangular collage.

The facing folio, curved italic labels, three ratio diagrams, harmonic
traces and moving string are authored additions. The cosmological registers
are presented as historical correspondences. The harmonics illustrate a
fixed string; they do not derive a physical tuning from the engraving's
planetary positions. Frequency varies inversely with sounding length.

## Native construction

- The image loader supplies the engraving to a native material child.
  The ink separator and print filter are local SkSL programs.
- Static print and facing typography use retained texture and picture
  caches. The string and harmonic diagrams are separate live Pen leaves.
- Native shaped TextPath labels follow shared circular interval geometry.
  Baskerville supplies the italic type; Menlo supplies marginal identifiers.
- A pluck excites a fundamental plus a weaker third partial. Its amplitude
  decays, while the selected partial changes every twelve seconds through
  the first three modes. Each mode's nodes stay fixed.

## Capture

The canvas is 2310 × 2190. From `apps/spell-circle-canvas`:

```sh
UV_CACHE_DIR=/tmp/sigil-study-uv-cache \
build/bin/Release/Sketchbook.app/Contents/MacOS/Sketchbook \
  src/sketch/sketches/fludd_monochord/fludd_monochord.py \
  --frame /tmp/fludd-monochord.png --at 1.7 \
  --state /tmp/fludd-state
```

Exact-time file stills use the raster canvas. The separate actual-window
lane supplies Graphite rendering:

```sh
UV_CACHE_DIR=/tmp/sigil-study-uv-cache \
build/bin/Release/Sketchbook.app/Contents/MacOS/Sketchbook \
  src/sketch/sketches/fludd_monochord/fludd_monochord.py \
  --shot /tmp/fludd-window.png --gpu \
  --state /tmp/fludd-window-state
```

The study exercises line-art image separation, shader child inputs, retained
print filtering, paper surfaces, shaped curved type and sparse live motion.
