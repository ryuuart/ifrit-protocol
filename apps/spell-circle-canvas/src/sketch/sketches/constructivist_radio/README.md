# Constructivist radio

An original 900 × 1260 radio broadside. Large shaped Cyrillic rides a
slanted transmission axis. A ruled steel lattice, a dark-throated metal
horn and a receding industrial street form a fabricated printed montage.
Two ink colours, paper pressure, a page fold, a slight plate offset and
registration marks join the image and type into one physical print.

The programme, issue number, schedule and wording are authored. This is
neither an archival radio advertisement nor a reconstruction of a specific
historical poster. The imagery is native geometry and procedural tonal
rendering, not archival photography or a bitmap collage.

## References and adaptation

[Rodchenko's *Handbill “Listen to Radio”*, about 1931, at MoMA](https://www.moma.org/collection/works/102381)
was inspected visually in the browser. Its small circular lithograph
combines a dark disc, elongated red Cyrillic, cream captions and turquoise
elliptical signs. It supplies a radio subject and the force of type as an
image. The rectangular format, diagonal steel tower, metal horn, street
and cream/red/black ink system here are authored rather than copied from
that circular handbill.

[El Lissitzky's *For the Voice*, 1923, at the Cleveland Museum of Art](https://www.clevelandart.org/art/2002.60)
was inspected visually with its image expanded. Its letterpress book
makes reading aloud spatial: changes of type size and direction, geometric
bars, a strongly constructed cover and a stepped index provide visual
rhythm. This study applies that relation between voice, directional type
and geometric structure. It does not reproduce the poems, cover letters
or index design.

[Margarita Tupitsyn's MoMA essay, *Colorless Field: Notes on the Paths of Modern Photography*](https://www.moma.org/interactives/objectphoto/assets/essays/Tupitsyn.pdf)
was read for its account of photographic material, non-objective collage,
film titles and oblique architectural views in Constructivist practice.
The essay's figures are not claimed as visually inspected references.
The fabricated montage here places industrial objects and a street at
contrasting scales and lets their geometry share the type's axis. The
screened horn is an authored way to evoke a reproduced tonal image;
its halftone, metal light and machining field are not documented claims
about the printing process of the two museum objects.

The lattice consists of six ruled hyperboloids joined at shrinking rings.
That is an engineering form associated with early broadcasting towers,
not a surveyed reconstruction of a named tower. The foreground apparatus
and buildings are invented. Their joinery, lamp direction, beam contrast,
window recesses and tram wires provide physical scale within the poster.

## Native print craft

The metal horn starts as a continuous tonal field: oblique cone normals,
a rolled edge, an offset throat with deep shadow, radial machining lines
and a correlated tarnish field. An angled screen converts that tone into
ink area. Light tones use positive dots; dark tones use negative holes,
so deep shadows merge into solid-looking ink rather than remaining a
uniform polka-dot pattern. The native shader uses the page's coordinates
and device content scale. Its edges cover a device pixel; when a cell
becomes too small to resolve, its mean ink coverage replaces the screen.
This keeps the metal tone legible in the fitted live view while larger
captures reveal the printed dots.

The tower's members join rotated end rings. Opposite handed generators,
front/back tonal separation, bright narrow steel edges, rivets, ring bolts
and a maintenance ladder make the lattice spatial. The street shares a
receding diagonal and has roof planes, wall returns, brick courses, window
reveals, ledges, aerials and suspended tram wires. Static complex geometry
uses native texture caches; simple plate and finishing marks use retained
picture caches.

The paper shader combines smoothly interpolated pressure variation,
long fibres, an edge stain and a wandering central crease. Printed ink
varies coverage under the same correlated pressure scale rather than
sprinkling unrelated defects across the composition. The stock's light
changes almost imperceptibly over time, like viewing a print under a slow
moving lamp. The print itself holds still.

All text is natively shaped, including the decomposed breve in `СЛУШАЙ`.
The large type requests Arial Black; narrow captions request Arial Narrow
with Avenir Next Condensed as an alternative; small text requests Arial.
Each line states its width, first cap-height baseline, tracking, condensed
width and transform origin. The authored hierarchy leaves the Cyrillic
accent and lower caption visible inside the print area. The artwork uses
no bitmap assets or image-generated media.

## Running and inspecting

From the application directory:

```sh
UV_CACHE_DIR=/private/tmp/sigil-study-uv-cache \
  build/bin/Release/Sketchbook.app/Contents/MacOS/Sketchbook \
  src/sketch/sketches/constructivist_radio/constructivist_radio.py \
  --frame /private/tmp/constructivist-radio.png --at 4 --scale 2 \
  --state /private/tmp/constructivist-radio-state
```

The exact 2D file-frame lane is raster. Adding `--gpu` initializes the
device but does not establish Graphite rendering of this plate. A bounded
live-window check uses the explicit Python file path:

```sh
UV_CACHE_DIR=/private/tmp/sigil-study-uv-cache \
  build/bin/Release/Sketchbook.app/Contents/MacOS/Sketchbook \
  src/sketch/sketches/constructivist_radio/constructivist_radio.py \
  --window-bench 2.5 --window-size 1280x900 --gpu \
  --state /private/tmp/constructivist-radio-window-state
```

For an application-window image, replace the benchmark flags with
`--shot /private/tmp/constructivist-radio-window.png --gpu`. The image
contains the app frame and fitted page. The renderer log supplies the
backend evidence.
