# Borges library

A reader stands at the open side of a hexagonal gallery. Five shelf rows
and varied leather spines flank a central ventilation well; low rails,
pendant lamps and further vestibules repeat above, below and toward the
vanishing point. A spiral stair occupies the near vestibule. An open
traveller's ledger in the foreground records the room rule and a search
for a catalogue. The canvas is 1600 × 1100.

This is an original spatial reading of Jorge Luis Borges's *La biblioteca
de Babel*. The Spanish and English annotations are authored paraphrases,
not quotations. The diagram is a finite visible portion of an imagined
library; it does not claim a uniquely determined plan or a historically
existing building.

## Reference structure

[The Spanish story](https://www.literatura.us/borges/biblioteca.html)
supplies hexagonal galleries, a central well, low rails, four shelf walls,
two unshelved sides, two spherical lamps, repeated floors, narrow
vestibules and a spiral staircase. The shelf specification is five rows
per wall and thirty-two volumes per row: twenty shelves and 640 books per
room. Each book has 410 pages, with forty lines per page and approximately
eighty characters per line. The finite alphabet has twenty-five symbols.
These constraints drive the geometry and ledger rather than decorate it.

[Gerardo Centenera Tapia's architectural reading](https://www.borges.pitt.edu/sites/default/files/pdfs/La_Biblioteca_de_Babel_pertinencia_de_u.pdf)
was read for its discussion of ambiguities in the connections and free
sides. The PDF figures were not available for visual inspection. No
figure from that article is reproduced. This study chooses opposite free
sides and a straight chain of vestibules. The ledger itinerary follows
that authored chain. The central wells remain open; the passage floor
joins neighbouring rooms rather than bridging their wells.

[Trinity College Dublin's Long Room page](https://www.visittrinity.ie/venue/the-long-room/)
was inspected visually in the browser. Its repeated timber shelf bays,
balustrade and deep gallery informed the sense of physical rhythm and
scale. Its plan, vault and ornament are not reproduced. The honey-coloured
timber, turned-looking rails, leather palette, brass mounts, invented
spine inscriptions, readers and ledger are authored choices.

## Native construction and craft

A world-coordinate model is projected into native polygons and strokes.
Six room depths and ten floor levels provide bounded geometry; perspective
and atmospheric attenuation allow their continuation to become unreadable
in the distance. Surface pieces, rails, shelf boards, fittings, shaped
spine text and spherical pendants share one depth order. Lamps disappear
behind floors and framing members. Four small readers establish a body
scale against the floor spacing.

Every intact book wall has five rows with exactly thirty-two generated
bindings. Unequal widths share one shelf extent. Heights, leather colours,
projecting spines, light edges, dark contact edges, gilt bands and selected
native shaped inscriptions vary within that constraint. The inscriptions
are invented, not transcripts of books in the story. Tiny lettering uses
an affine tangent approximation to each projected spine. The retained
large headings and paragraphs remain shaped text with explicit cap-height
anchors and leading.

The floor planes are subdivided for local falloff from each room's two
lamps. Boards carry perspective seams, longitudinal grain that curves
within a board, occasional knot contours and contact shade beside the
rail. Lintels carry longitudinal grain, a bright bevel and a shaded lower
molding; pilasters have narrow carved highlights and dark return edges.
All these marks inherit their surface direction rather than being
uncorrelated screen noise. The helix has individually projected treads,
a continuous spindle and a rail following its rise.

The foreground ledger has a layered page block, a bowed dark gutter,
paper fibres, ruled annotations and a selected itinerary. Its red marker
travels down the authored chain over twenty-four seconds and repeats.
The lamp atmosphere breathes slightly. Live material and transform
bindings drive that motion; the static architecture and ledger use native
texture caches. No reference bitmap, raster illustration or image-generation
asset appears in the artwork.

## Running and inspecting

From the application directory:

```sh
UV_CACHE_DIR=/private/tmp/sigil-study-uv-cache \
  build/bin/Release/Grimoire.app/Contents/MacOS/Grimoire \
  ../grimoire/sketches/borges_library/borges_library.py \
  --frame /private/tmp/borges-library.png --at 4 --scale 2 \
  --state /private/tmp/borges-library-state
```

The exact file-frame capture rasterizes the native 2D composition. Adding
`--gpu` initializes the device but does not turn this lane into Graphite
evidence. For a bounded live-window rendering check, use the explicit
file path:

```sh
UV_CACHE_DIR=/private/tmp/sigil-study-uv-cache \
  build/bin/Release/Grimoire.app/Contents/MacOS/Grimoire \
  ../grimoire/sketches/borges_library/borges_library.py \
  --window-bench 2.5 --window-size 1280x900 --gpu \
  --state /private/tmp/borges-library-window-state
```

A live window capture uses `--shot /private/tmp/borges-window.png --gpu`
instead of the benchmark flags. It includes the application frame and
its fitted canvas. Renderer logs establish the backend; an image filename
or device initialization alone does not.
