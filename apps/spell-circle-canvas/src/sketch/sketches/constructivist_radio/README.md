# Constructivist radio

An original typographic poster about the city as a transmitter. It is a
study of a visual language, with a fabricated receiver, city, broadcast
schedule and wording. It does not reproduce a historical poster or claim
that its fictional programme existed.

The page is 900 × 1260 canvas units. A massive Cyrillic headline rides an
oblique rule; a red wedge carries a second, steeper pair of headlines.
The loudspeaker is an enlarged cut-out in front of that wedge. A sloping
city montage, rings of sound, dense secondary captions, a vertical spine
and a wide red programme band give the composition several reading
distances. The typography is real shaped Unicode text, including the
combining breve in Й. The display face requests Arial Black with Arial
and Helvetica Neue as its alternatives; captions request Arial Narrow
with Avenir Next Condensed as its alternative.

## Sources and interpretation

The [MoMA record for Rodchenko's *Handbill “Listen to Radio”*](https://www.moma.org/collection/works/102381)
identifies a circular lithograph from about 1931. Its image was inspected
in the museum's image viewer. The dark disc, highly elongated red Cyrillic
lettering, pale secondary captions and overlapping ellipses behind the
title are concrete reference techniques. This study adapts the disc into
a receiver and distributes the sound ellipses across a rectangular page.
It does not copy the handbill's wording, proportions or arrangement.

The [Cleveland Museum of Art record for Lissitzky's *For the Voice*](https://www.clevelandart.org/art/2002.60)
identifies the 1923 book's red and black letterpress on paper. The museum
describes mixed fonts and letter sizes, directional typography,
geometrical marks and a stepped index for Mayakovsky's poems, which were
intended for reading aloud. Its cover was inspected in the museum's
expanded image viewer. Its asymmetric type, bars and concentric circle
inform this study's hierarchy and directional changes. The radio subject
also treats letters as the visual counterpart of a voice.

[MoMA's Rodchenko chronology](https://www.moma.org/interactives/exhibitions/1998/rodchenko/table.html)
documents his LEF covers, photographic work and photocollage for
Mayakovsky. The montage in this study is a generated analogy: thirteen
building silhouettes with punched windows and aerials, combined with a
radially screened receiver. It contains no photographs and makes no claim
to reproduce a particular building, machine or archival image.

The aged cream stock, pigment dropout, faint handling marks, slight red
registration offset and halftone receiver are original reconstruction
choices. The simulated ink is an artistic coverage model; it does not
model chemical absorption or reproduce a measured historical print.

## Native construction

The substrate is a SigilMaterial shader. Fine random grain, sparse long
fibres, a broad edge stain and a quiet central crease give the paper
texture at several scales. A separate pigment shader makes red and black
coverage uneven without changing the outlines. Its alpha is premultiplied.
The red plate multiplies over the underlying page with a fractional
registration offset; cream lettering stands over it as a knockout.

The receiver combines concentric geometric rings, a radial metal lattice,
a screened field whose dot diameters follow a synthetic light, an
off-centre cone and rim screws. All of those marks are native drawing.
The city uses oblique roof polygons, repeated window holes and thin
aerials. Registration targets and seeded handling marks finish the page.
No bitmap assets or generated raster images are used.

Static drawing programmes use `compose::pen` with `compose::Cache::Picture`.
The individual type leaves remain retained text inside
`compose::positioned`. Their first baselines use
`weave::FrameOptions::FirstBaseline::kCapHeight`, making each stated y
coordinate a cap-height anchor. Their ink uses `compose::PaintBox::Canvas`,
so the paper-facing grain field is shared across transformed passages.
The page uses `compose::Overflow::Clip` at its outer edge. The study is
static: temporal texture drift would imply moving pigment rather than a
printed sheet.

## Capture

From `apps/spell-circle-canvas`:

```sh
build/bin/Release/Sketchbook.app/Contents/MacOS/Sketchbook \
  src/sketch/sketches/constructivist_radio/constructivist_radio.cpp \
  --frame /tmp/constructivist-radio.png --scale 2 \
  --state /tmp/constructivist-radio-state
```

The capture moment is 0.4 seconds. A fresh state directory makes live
compilation independent of another sketch's cache. The host must match the
framework headers it compiles the sketch against.

The file entry compiled and rendered at 2×. The resulting 1800 × 2520
raster plate was inspected: the Cyrillic headline, combining breve in
СЛУШАЙ, lower-band Й, registration targets and rotated spine captions
remain inside the page. The schedule sits in clear paper beneath the
dark city base, and the lower band leaves room above its accented caps.
The receiver's screen, radial ribs and rim screws remain distinguishable.
The page was compared with the museum images' disc, directional type,
two-ink hierarchy and geometric rhythm as an original adaptation.

File captures use the raster path. Actual Graphite rendering was verified
through an explicit file window run, independent of the compiled registry.
The bounded run presented this study successfully, its renderer identified
Graphite GPU, and its own window capture was inspected. The window showed
the printed ink, Cyrillic hierarchy, receiver screen and city geometry.

Run a bounded Graphite window check with:

```sh
build/bin/Release/Sketchbook.app/Contents/MacOS/Sketchbook \
  src/sketch/sketches/constructivist_radio/constructivist_radio.cpp \
  --window-bench 2.5 --window-size 1280x900 --gpu \
  --state /tmp/constructivist-radio-window-state
```

To photograph that study through the live window instead, use the same
file path with `--shot /tmp/constructivist-radio-window.png --gpu` and a
fresh state directory. The output is the app window with its fitted
canvas, not the full-resolution still that `--frame` writes. The
single-stem headless GPU lane requires the study in the compiled registry.
