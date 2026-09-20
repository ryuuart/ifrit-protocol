# SigilDraw — the buffer, the field and the words

The chapter on what stands beside the pen: a second canvas off screen, a
one-shot drawing on somebody else's canvas, the smooth random field p5's
`noise()` is, the colour a string parses to, and the one enumeration
every mode-taking verb reads. `README.md` beside the library is the
front page, and the pen itself has its own page.

## Graphics: an offscreen canvas and the pen that draws on it

p5's `createGraphics(w, h)`, spelled as the value it returns because it
lives across frames: a sketch keeps one as a member and opens a frame on
it whenever it has something to draw there.

```cpp
draw::Graphics buffer{200, 200};       // a member of the sketch

void draw(Pen& pen) override {
  Pen& g = buffer.begin(pen);          // the buffer's own pen
  g.background(0);
  g.fill(255, 120, 80);
  g.circle(100, 100, 40);
  buffer.end();
  pen.image(buffer, 20, 20);           // and onto the frame
}
```

ITS PIXELS ARE THE HOST'S. The buffer is formed at the host pen's own
density, through the host's canvas, so it lives where the host draws — on
the device when the host is on one — and falls back to raster where that
canvas cannot make a surface. It is re-formed when the density changes
and kept otherwise, so what was drawn on it stands until something draws
over it, exactly as p5's does.

A RE-FORM KEEPS THE PICTURE. A surface that has to be replaced — the
density moved, or a resize gave the buffer another canvas size — is not
handed over empty: what the old one held is drawn into it, scaled to the
new extent. Changing how large a buffer is, or how many pixels a unit of
it covers, must not erase what earlier frames accumulated on it.

ITS CLOCK IS THE HOST'S. `Graphics::begin` reads the host pen's frame
count, elapsed time, step and fonts onto the buffer's pen, so a material
resolved there and a shaped line of text there agree with the frame
around them. Its style is its own and holds between frames, as a pen's
does.

Its width and height are in canvas units — the units the buffer's pen
draws in and the units `Pen::image` places it by, whatever density it
ends up formed at.

## on: one drawing on a canvas somebody else holds

`on` is a pen begun over a canvas at a size, a program run with it, and
the frame ended — the ceremony every bake into an offscreen surface
otherwise writes, which is what made raw Skia the cheaper spelling
there.

```cpp
draw::on(*surface->getCanvas(), {120, 80}, [&](draw::Pen& pen) {
  pen.background(kPaper);
  pen.rect(8, 8, 40, 40);
});
```

The pen lives for the call and no longer: nothing is kept between bakes,
which is what separates it from `Graphics`. It has no clock — a picture
drawn once has no time in it — so the elapsed time reads zero and the
frame count is one, so first-frame setup runs on the bake. A drawing that
moves is a node's program and not a bake. The font context is what text
is shaped with, and a bake with none sets none.

## NoiseField: p5's noise as a value

A SMOOTH RANDOM FIELD, sampled anywhere in one, two or three dimensions
and always answering the same number at the same place for the same
seed.

It has p5's shape — octaves layered at doubling frequency and a falloff
between them, a value in [0, 1) — over this repository's own mixer rather
than p5's permutation table: every corner of the base lattice is core's
lattice word squeezed to a unit float, and the value between corners is a
cosine blend of the eight around it, which is the blend p5 uses. So the
pictures a pasted sketch draws with it have p5's character and not p5's
exact pixels.

`geometry::path::valueNoise` is a DIFFERENT FIELD over the same lattice
mixer, not a rounding of this one: it squeezes a corner from the whole
word rather than from the low 24 bits, eases with a smoothstep rather
than a cosine, and answers in [-1, 1]. Either one re-spelled as the other
re-rolls every picture stored from it, which is why they stand side by
side. Reach for that one where a displacement wants a signed field; reach
for this one where a pasted sketch wants p5's.

## parseColor: a CSS colour as p5 accepts one

`#rgb`, `#rgba`, `#rrggbb`, `#rrggbbaa`, and the named colours a sketch
reaches for — the sixteen of HTML's first palette with `grey`, `orange`,
`pink`, `brown`, `gold`, `violet`, `indigo`, `crimson`, `coral`,
`salmon`, `tomato`, `turquoise`, `skyblue`, `steelblue`, `slategray`,
`darkgray`, `lightgray`, `transparent`. Anything else is opaque black,
which is what a canvas gives an unparseable colour too.

## Constant: the words a verb takes

One enumeration for all of them, because p5 keeps one namespace for them
and `CENTER` is the same word to `Pen::rectMode`, `Pen::ellipseMode`,
`Pen::imageMode` and `Pen::textAlign`. A verb handed a word it does not
take ignores it, as p5 does. `POLYGON` is the one word p5 does not spell:
it is what `Pen::beginShape` with no kind means, and nothing needs to
write it.

## See also

The pen's own page, and `brush/README.md` for the natural media over it.
