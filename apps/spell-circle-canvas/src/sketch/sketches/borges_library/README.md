# Borges library

An original architectural atlas inspired by Jorge Luis Borges's *La
biblioteca de Babel*. Four exploded galleries, a ventilation shaft,
vestibules and a spiral staircase sit between bilingual literary
annotation, a selected hexagonal itinerary, a room plan and a volume
count. The drawing is a reading of the story, not a unique map of it.

The canvas is 1400 × 1000. The headings request Baskerville with Georgia
as an alternative. Small architectural labels request Avenir Next with
Helvetica Neue as an alternative; level labels request Menlo. All
typography is native shaped text. The Spanish and English annotation is
original, and includes no quotation from the story.

## Sources and choices

[The Spanish story](https://www.literatura.us/borges/biblioteca.html)
supplies the hexagonal galleries, central openings, repeated levels,
four shelf walls, two free sides, spherical lamps and spiral stair.
Its book specification supplies five shelves per wall, thirty-two books
per shelf, 410 pages per book, forty lines per page and approximately
eighty characters per line. The bottom grid explicitly counts twenty
shelves and 640 volumes. These numerical facts do not determine a unique
building.

[Gerardo Centenera Tapia's architectural reading](https://www.borges.pitt.edu/sites/default/files/pdfs/La_Biblioteca_de_Babel_pertinencia_de_u.pdf)
explains why a drawing must resolve ambiguities in the story: the
positions of the free sides, the connections and the arrangement of
vestibules are underdetermined. This study makes its circulation an
assumption, chooses a visible route through the small network and uses
an exploded projection to make lower shelves readable. The article's
text was read; its PDF images could not be viewed in the available
browser. No diagram from that article is reproduced.

[Trinity College Dublin's own Long Room page](https://www.visittrinity.ie/venue/the-long-room/)
was inspected visually in the browser. Its photo shows timber shelf
bays, a long gallery and a repeated balustrade. Those physical rhythms
inform the architectural line work and shelf subdivisions. The study
does not copy the Long Room's plan, vault or historical architecture.

The paper stock, muted blue ink, brick annotations, human figures,
exploded spacing, flattened projection and supporting folio system are
original artistic choices. The diagram is an illustration; its
coordinates are not a dimensioned building model.

## Native construction

The retained composition contains four cached native drawing programs:
paper, architecture, supporting diagrams and page ruling. Text leaves
remain retained elements above those programs. The paper uses seeded
sparse fibres and faint edge staining. Room walls contain five rows of
thirty-two generated book spines with varied ink and tiny spine bands.
The open central shaft is assembled from six annular floor quads, so
the hole does not require a sketch-local path winding convention.

The spiral stair uses a sampled helix, projected treads, a continuous
axis and a low outer rail. Lamps, shelf uprights, floor seams, section
hatching and small readers establish several scales of detail. Pale
wire rooms are drawn before the readable cutaway so they recede behind
the main geometry. No bitmap assets or generated raster illustration
are used.

The plate is deliberately still. Repeated architecture and dense type
test retained authoring, projection, Unicode, cap-height anchors and
cached native drawing without requiring frame-driven callbacks.

## Capture

From the application directory:

```sh
UV_CACHE_DIR=/private/tmp/sigil-study-uv-cache \
  build/bin/Release/Sketchbook.app/Contents/MacOS/Sketchbook \
  src/sketch/sketches/borges_library/borges_library.py \
  --frame /private/tmp/borges-library.png --at 0 --scale 2 \
  --state /private/tmp/borges-library-state
```

Add `--gpu` to initialize the device executor on a host with device access.
The file-capture lane still rasterizes this 2D canvas; this checks device
availability rather than Graphite rendering of the plate.

Actual Graphite rendering was separately verified in the live window
using the explicit file path. The bounded run presented this study
successfully and its renderer identified Graphite GPU:

```sh
UV_CACHE_DIR=/private/tmp/sigil-study-uv-cache \
  build/bin/Release/Sketchbook.app/Contents/MacOS/Sketchbook \
  src/sketch/sketches/borges_library/borges_library.py \
  --window-bench 2.5 --window-size 1280x900 --gpu \
  --state /private/tmp/borges-library-window-state
```

The final CPU and device-enabled plates were rendered at 2× and inspected. The refinement
made every repeated room's shelf rhythm visible, moved the wire rooms
behind the cutaway and placed the four architectural callouts on their
stated features. The book grids, Spanish accents, negative level sign,
infinity symbol and dense footer remain visible at their intended scale.
