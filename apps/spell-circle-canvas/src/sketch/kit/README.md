# SigilSketchKit — the sheet a sketch stands on

A specimen sheet in this repository is a page with a title, a subtitle
and a footer, ruled off from a run of captioned cells, each cell a
picture in a grounded well with the call over it and the remark under.
**This library is that sheet, stated once.** The colours, the registers,
the margins and the rules come from one theme value, and the arrangement
from one call each, so the file someone opens to study `Border`'s four
modes is a file about `Border`'s four modes and not about furniture.

What it is not is a place a look is decided. `Theme` is a **seam** — a
plain comparable struct — and `houseTheme()` is one **stock value** over
it. A sketch's own theme is indistinguishable from the house one at every
call site, and every component here behaves correctly with no theme bound
at all.

```cpp
#include <sigilsketch/kit/Kit.h>

namespace sketch = sigil::sketch;
using namespace sigil::compose;

struct BorderWeave final : sketch::Sketch {
  void setup(sketch::SketchContext& ctx) override {
    sketch::kit::stage(ctx, {.size = {1100, 424}, .captureAt = 0.05});
    ctx.composer.render(sketch::kit::page(
        {.title = "THE RULE AND THE STRANDS",
         .subtitle = "dials · the width and the inset",
         .footer = "a crossing is discovered, not declared"},
        kit::cells({.cells = {cell(…), cell(…)}, .gap = 10})));
  }
};
```

## The theme, and why it is inherited rather than passed

`Theme` holds three values and one choice: a `Palette` of six colours, a
`TypeScale` of seven `Register`s and two faces, a `Spacing` of the
distances a sheet is set by, and where a cell's caption lines stand.

It arrives at a component through **`sigil::core::environment`**, the
reconciler's inherited value, bound by `sketch::kit::Provide` for the
spacing, the palette and the faces a component reads; its registers
reach the tree as a sheet, `Theme::styleSheet()`, which `page()` states
on its root and a sketch without a page states on its own with
`styleSheet()`:

```cpp
sketch::kit::Theme sheetTheme() {
  sketch::kit::Theme paper = sketch::kit::houseTheme();
  paper.palette.ground = {0.945f, 0.937f, 0.918f, 1};
  paper.palette.ink = {0.114f, 0.106f, 0.098f, 1};
  return paper;
}

const sketch::kit::Provide look(sheetTheme());   // the theme, for this scope
ctx.composer.render(sketch::kit::page({…}, content));  // its root states the sheet
```

A describe phase is an ordinary C++ call tree evaluated bottom-up, so the
describe-time stack *is* the description tree and dynamic scope is the
C++ answer to inheriting down it. `sketch::kit::theme()` is the read;
with nothing bound it answers `houseTheme()`, which is what makes a
component correct on its own.

A `Register` says how one line is set: its size, its tracking, which of
the theme's two faces it takes — and, for the line neither of those is,
`Register::face`, the face that line names for itself. A masthead whose
title is a display cut standing over an eyebrow in a grotesque and a
subtitle in the text face is three faces, and a theme carries two.

**A register is also a partial type for the cascade.** `Theme::font(line)`
is the register as a `weave::Type` — its face, size and track, no colour,
so the ink in force paints it — and `Theme::styleSheet()` is all seven as
classes under their own names: `title`, `subtitle`, `footer`,
`captionLabel`, `captionNote`, `eyebrow`, `section` — with `readout`
beside them, the one class no register of its own answers. A node that
states that sheet with `styleSheet()` puts it in force for everything
under it, so a leaf there is set in a register by name:

```cpp
compose::text(u8"CALL").styleClass("captionLabel")   // the register, by name
compose::text(u8"note").font(look.font(look.type.eyebrow))  // or as a value
```

**A CLASS CARRIES ITS WHOLE LOOK, COLOUR INCLUDED**, as a CSS class does:
`title` and `captionLabel` are registered in `Palette::ink`, `subtitle`,
`footer` and `captionNote` in `Palette::ash`. The two registers a sheet
sets INSIDE its content — `eyebrow` and `section` — name no colour, so
each is painted in the ink in force wherever it is read. So a page's three
lines and a cell's two are one entry each on this sheet and nowhere else:
a theme with two colours moved moves every line that names them.

**One class is not a register of its own.** `readout` is the register a
CALL is set in — the digits of one width — in `Palette::figure`, and it is
where a MEASURED FIGURE stands: a readout's value, a figure column's
cells, a reading pinned over a picture. It is an eighth entry on the
sheet, so a figure reads as one wherever it is written.

**And eight more entries answer no register at all**: the parts a CHART
draws, tabled under "the classes a chart draws" below, so a plot is dressed
by the same sheet a page states and a theme with one colour moved moves
every drawing that names it.

A sketch whose classes go past the registers starts from `styleSheet()`,
adds its own with `weave::StyleSheet::set` or as a literal, and states
the result on its root; a nearer sheet's entries stand over a farther
one's by name, so a panel with registers of its own states them on the
panel.

The face a register takes comes from somewhere, and three fallback runs
recur across this repository's sheets: the book face, the terminal face
and the interface face. `houseFace(Voice, weight, slant)` answers each,
resolved once and held, so two sheets asking for the same voice get the
same face *pointer* — which is what a style, an inherited value and a
memo key compare by. A sheet wanting a face no other sheet asks for
spells its own run through `weave::ports::face`.

```cpp
paper.type.sans = sketch::kit::houseFace(sketch::kit::Voice::Book);
paper.type.mono = sketch::kit::houseFace(sketch::kit::Voice::Terminal);
```

**It costs the prune nothing.** The theme is read during describe and
lands in the reading node's own description, so the reconciler's
structural comparison is already an exact dependency tracker: a node
whose description came out identical prunes whether or not it read a
theme. That is why `Theme` and everything in it compares **exactly** —
never perceptually, never epsilon'd — and why the mono face is resolved
once and held rather than resolved per call: a face is compared by
pointer, and two resolutions of one family never compare equal.

**Bind the theme where the tree is DESCRIBED, not where setup runs.** A
sketch that describes again — from `update()`, when its data changes —
describes outside setup's scope, and a theme bound only there would not
be in it. Put the `Provide` at the top of whatever function builds the
tree, and another in `setup` if `stage()` is to take its ground from the
same theme. The registers need no binding: the sheet is a value on the
tree and goes wherever the tree goes.

**One caveat.** A `custom()` paint program is a callable the kernel
invokes later, and it runs with no scope. Capture the colours such a
lambda needs by value at the call site, where the scope still stands. A
memo's deferred describe is different: it captures the environment
standing where the memo was described, restores it around its own call,
and describes again when what it captured changes — so a memo reads its
theme after the `Provide` that bound it has ended.

## The components

Each takes a props struct, reads the theme, and returns an Element built
by plain composition over `compose::kit`. Props are the caller's facts;
the theme is the look. Children arrive as Elements and slots as props, so
a component nests inside another the way a box does — which is the point:
a sketch is meant to read as its algorithm plus a run of these calls, not
as a thousand lines of furniture.

**A COMPONENT'S TYPE IS THE THEME'S, NEVER A PROP.** No props struct here
carries a face, a size or a colour for the words it sets: the register the
line is set in is fixed by the component, and what that register looks
like is the theme's. A line that must be a step quieter than its register
says so with an ink (`Line::ink`), which is a colour and not a type. **A
text prop is a `compose::Utf8`** — it takes `"…"` and `u8"…"`, a
`std::string` and a `std::u8string` alike — so a sketch writes
`.title = "THE STROKE ATLAS"` and never a conversion around the words.

Every component field that paints an area uses `compose::SurfacePaint`
from `<sigilcompose/core/SurfacePaint.h>`. It accepts a Fill, a live fill
binding, a material paint or a recipe directly. Neutral Compose wells and
sheets accept the same value. Theme wrappers resolve defaults, then pass
that value through unchanged. Pass it to `compose::Element::fill` when
painting an Element directly.

### The surface — `Page.h`, `Cells.h`, `Passage.h`, `Document.h`

| | |
| --- | --- |
| `stage(ctx, Stage)` | the canvas, the ground and the capture moment in one call — the whole `CanvasSpecification`, with the ground taken from the theme unless the stage names one. A SET takes the same value through the `SetContext` overload; the viewpoint is not on it, because a camera is a fact about the scene and `SetContext::camera` is the fallback for the set that states none |
| `page(Page, content)` | the sheet over the whole canvas: title, subtitle and footer set in the classes `title`, `subtitle` and `footer`, its margins, its ground and its hairline |
| `well(Well, surface)` | the fixed surface a specimen is shown in, on the theme's cell ground — with `corners` and a `keyline`, the PLATE a panel stands on; with a `recess`, the hole punched in one; with a `relief`, the piece standing proud of one. THE SURFACE IS THE WELL: the spec is written onto the element handed in, which is what a drawing sized to its plate wants |
| `well({…, .content = Well::Content{}}, picture)` | THE WELL THAT HOLDS: the plate is a surface of its own and the picture stands inside it at its own measure, ranged as `content` says and centred where it says nothing else — the specimen smaller than the plate it is shown on |
| `caption(measure, label, note, body)` | one captioned specimen in the theme's voice — the label set in the class `captionLabel`, the note in `captionNote`; `measure` is the cell's own width, the one distance a caption cannot inherit |
| `cell(Cell, label, note, picture)` | the same specimen with the sheet's plate and measure stated ONCE: `Cell::plate` is the well the picture is HELD by — as a box holds a child, or ranged where `Well::content` says — and `Cell::measure` unset is the plate's own width |
| `cells(Run)` | a run of cells along one axis at the theme's gutter, each at its own width |
| `panelGrid(PanelGrid)` | equal shares of the width, one per cell — what `cells` cannot do, because a fixed width does not know how wide the page is — on one row where `columns` is 0, wrapped every N above that, with a short last row keeping its share; `PanelGrid::measure` is the width the shares are cut from, for the grid whose parent sizes itself from its content and has none to divide |
| `passage(ctx, name)` | the prose in the sketch's own files, `ctx.local(name)` (`"data/manuscript_1.txt"`), minus the newlines a file ends with — the prose a sheet about setting a page is SET IN, kept beside the sketch rather than typed into it |
| `Document(ctx, name)` | the JSON document in the sketch's own files, read as the WORDS THE PLATE SETS: a record at a key, a sentence with its figures written in, a run of lines each in the class the document named, and a passage as one mixed-text value |
| `lineOf(line)` | one of that run as a leaf, in the class the document named for it |

```cpp
sketch::kit::page({.title = "THE STROKE ATLAS"},
                  sketch::kit::panelGrid({.cells = panels, .columns = 4}));
```

`Page::ruled` is `false` for a sheet that rules neither header nor
footer, and `Page::ground` names a fill for a sheet whose ground is not a
flat colour, because a palette holds colours and a gradient is not one. A
well that must paint nothing passes `Fill::none()`; a well that must
paint something else passes that.

**A PAGE IS THE ROOT OF THE CASCADE.** The sheet `page()` returns declares
the font and the ink everything under it inherits: the theme's text face
at the `TypeScale::captionNote` register — the sheet's RUNNING TEXT, the
one line a specimen sheet sets in sentences rather than as a label — in
`Palette::ink`. So a leaf written as `compose::text(utf8)` with no style
of its own is set in the sheet's own voice; `compose::Element::font({.size
= 22})` is that voice at another size and inherits the rest;
`compose::stroke(1.0f)` with no fill named is drawn in the sheet's ink,
which `Fill::currentInk()` also reads back; and a padding written as a
`weave::Length` measures against the type in force. A leaf handed a whole
`weave::TextStyle` — `compose::text(utf8, style)`, which is what
`Theme::style` builds and what most components here pass — inherits
nothing and is set exactly as it was written. The page's own title,
subtitle and footer and a cell's label and note are the exception: each is
set in the CLASS of its own name, a partial over what the page inherits.
`page()` states `Theme::styleSheet()` on its root, so those lines and
every cell under the page are in the theme's voice whether or not the
sketch around it bound a theme, and a sheet the sketch states nearer to
a leaf stands over the root's by name; a sketch that renders no page
states the sheet on its own root. A cell whose call must stand otherwise
hands `Caption::label` its own leaf, and the cells under it keep the
register. A register always states its face: one set in the theme's sans,
which the house theme leaves as the font context's default family, says
so, and a caption under an ancestor that named a face is still set in the
register's own.

**A PLATE IS A WELL WITH TWO MORE FIELDS.** A grounded panel with rounded
corners and one hairline round it is what a page puts a heading, a rack of
pills or a warning strip on, and spelling it by hand is a box, a fill, a
corner radius and a stroke every time:

```cpp
sketch::kit::well({.ground = Fill::color(kPlate), .padding = 13,
                   .paddingY = 10, .clip = false, .corners = 3,
                   .keyline = Fill::color(kRule)})
    .row()
```

`Well::placed` makes the well a `stack` rather than a box, so a plate
holding a drawing rather than a reading is one call: its children keep the
rects they were built with. `keyline` unset draws none — a specimen well
is grounded and unruled, and
that is the common case. Where it is set it is drawn INSIDE the well's own
box: a rule centred on the boundary puts half its width outside, and a
plate that is not the width it was given is the one thing a fixed surface
may not be. `paddingY` is there because a plate is often set tighter down
than across, which one distance cannot say.

`recess` is the well read as a HOLE punched in what holds it rather than
as a patch of ground on it: a shadow cast inside its own edge, and a hard
sunken lip under that shadow — the blur says how deep the surface goes
and the lip says where it breaks. `relief` is the same reading with its
sign turned over — the light one lifted edge catches and the shadow the
opposite one casts, which is what a carved frame, a raised boss and a key
cap all are over a ground. Unset is flush, which is the specimen well.

**THE WORDS ARE A FILE, AND THE FIGURES ARE THE RUN'S.** A plate that
sets sentences keeps them in `data/content.json` beside it and reads them
through `Document`, so the code is the template — the structure, the
classes, the layout — and the file is the content. A record answers at a
key, a sentence comes back with every `{name}` in it replaced by the
figure of that name, a list comes back as the run of lines `each()` walks,
and the same list comes back as one mixed-text value where the sentences
are one paragraph:

```cpp
kit::Document doc{ctx, "data/content.json"};        // in setup
doc.figures({{"rest", compose::kit::formatted("%.2f", measured)}});
…
column().children({text(doc["masthead"]["title"]).styleClass("title"),
                   each(doc.run("notes"), sketch::kit::lineOf)})
```

A LINE NAMES A CLASS AND NOT A LOOK: a bare string in the list is a
sentence in the running voice the tree already carries, and
`{"words": …, "class": …}` is one whose CONTENT decides its colour — a
verdict, a warning, a school of thought — resolved by the sheet in force
where it lands. A document is read and never demanded: a missing file, a
missing key and a key holding the wrong kind all answer nothing, so a
sentence deleted from the file deletes its line and a plate whose file did
not arrive draws its furniture and none of its lettering. A figure is
already formatted when it arrives, because how a number reads is the
measurement's business.

Nothing here reads a theme: a class is a name, and a node is text wherever
text is taken, because `compose::Utf8` accepts any value that reads itself
out with `text()`.

### What announces something — `Heading.h`

| | |
| --- | --- |
| `titleCard(TitleCard)` | an eyebrow over a title over a subtitle, optionally ruled, with the notes ranged at its far edge — the header half of a page, standing on its own |
| `sectionHeader(SectionHeader)` | a name at the left, a remark at the right, and the rule that fills what the two leave between them |

```cpp
sketch::kit::titleCard({.eyebrow = {"SIGIL · COMPOSE"},
                        .title = {"THE STROKE ATLAS"},
                        .subtitle = {"every rail, at one width"}});
```

**EACH LINE IS A `Line`, NOT A STRING**, because a masthead is performed
rather than set: its three lines each enter on their own beat, and one
returned Element has no handle on the lines inside it. A `Line` carries
the words, the ink it is set in where the register's own is not it, and
the beat — an `opacity`, a `lift`, or a whole `compose::Track` over its
glyphs. A line written as words alone rests, which is what a set card is.
`TitleCard::notes` is the stack ranged at the far edge — the issue time,
the bounds a word in a bulletin stands for, the sources a study was read
off — which turns the card into a row with the lines taking the rest of
the width and the two ranged against each other at their ENDS, so the
last note sits on the card's last line. `TitleCard::key` names the parts
(`<key>-eyebrow`, `-title`, `-subtitle`, `-note0`…) the way `Page::key`
and `Row::key` do.

```cpp
sketch::kit::titleCard(
    {.eyebrow = {.words = "MET OFFICE", .opacity = beat(0.05f, 0.55f)},
     .title = {.words = "THE SHIPPING FORECAST",
               .fx = Track{.effect = fx::rise(16.0f), .progress = …}},
     .notes = std::move(slugs),
     .align = Align::Stretch,
     .key = "head"});
```

### A name and the figure that answers it — `Rows.h`

| | |
| --- | --- |
| `labelRow(Reading, Readout)` | the name at the left in the quiet register, the figure at the right in the figure colour and the face a call is set in, with a swatch before the name where the row is also a key |
| `readout(rows, Readout)` | a stack of those, at the theme's row gap, optionally ruled between |
| `table(rows, Table)` | N columns each at its own width, the ones that carry a number in the figure register, with a mark before the first and the word each column carries over it — the reading a pair cannot hold |
| `bars(labels, values, Bars)` | one row per value against the largest of them: the label at the left, the bar in the theme's figure colour on a track of the same dimmed, and the figure after it — with the overload that reads the two columns off a table; `Bars::inks` is one colour per row, over the bar's paint and the row's two lines, for the sheet where WHICH row is lit is the data's business |

```cpp
sketch::kit::readout({{u8"nodes", nodes}, {u8"instances", live}},
                     {.measure = 220, .nameMeasure = 168});
```

`Readout` is HOW a row is set and the rows are the data, so one value
sets a whole stack. `nameMeasure` is what ranges the figures of unequal
names.

A READOUT and a TABLE are different readings, and neither is the other
with a field set. A readout is a PAIR ranged to opposite edges of one
measure; a table is N columns each at its own width, which is what a
reading of more than a name and a figure needs — a key, a cost, the tier
it took and the condition that refused it.

```cpp
sketch::kit::table(rows, {.columns = {{.width = 126, .figure = true}, {.width = 46, .figure = true}, {.width = 66}, {}},
                          .swatch = 9});
```

A ROW STATES THE COLOUR IT IS SET IN, and a column the word over it.
`Reading::ink` and `Row::ink` stand over the theme's own and keep the
registers the row was set in — the foot a table's own reading is, a
reading in the colour of the thing it reads — because WHICH rows are lit
is what the data says and a theme cannot. `Column::head` is the word over
one column, in the theme's section register, so a headed table is one call
and the head and the reading under it are ranged by one arrangement.

A figure a sketch measured about its own execution goes through
`ctx.measured` **before** it reaches here. These components arrange a
row; what the number is, and whether it is pinned, is the sketch's.

**BARS ARE NOT A ROW OF METERS.** A meter is one fraction of a known
whole; `bars` is N rows against an extent DERIVED from the values, which
is what a plot of a column is and why no scale is stated anywhere.
`Bars::largest` states the extent where a plot must share one with the
plot beside it, and `Bars::rest` is the track behind each bar —
`Fill::none()` draws none.

```cpp
sketch::kit::bars(*cities, "city", "population", {.length = 150})
```

### Colour, named — `Legend.h`

| | |
| --- | --- |
| `legend(Legend)` | swatch-and-label rows, stacked or run along a line |
| `swatchStrip(SwatchStrip)` | a ramp's steps in order at one size, with words under the ones that have them, each in the ink the strip names for it and each riding the entrance the strip carries |
| `chip(Chip)` | one word on its own ground, in the theme's eyebrow register |

```cpp
sketch::kit::legend({.entries = {{Fill::color(kWarm), u8"lit"},
                                 {Fill::color(kCool), u8"shaded"}}});
```

An entry may carry its own `keyline` and its own `ink`. A key to a
ladder of tiers is not a run of flat patches under one ink: its mark is
a dim body inside a bright edge, and its word is set in the colour it
names — which is how the reader tells the key from a caption. The
entry's `note` stays in the quiet ash either way, because a gloss is
not part of the naming.

`SwatchStrip::inks` is what says which numbers under a ramp are the
MEASUREMENT — the steps a reading is taken at are lit and the rest stand
in the quiet ash — and `SwatchStrip::appear` is the beat each step rides
in on, so a strip that is DEALT rather than printed says
`.staggerChildren(26ms)` on what comes back, exactly as a legend does.

`LegendEntry::mark` is the other half: where a patch of colour is not
what the key shows — a quarried sample at its own two dimensions, a live
figure, a sprite — the caller hands over the drawing itself, and it
stands exactly where the swatch would while keeping whatever size,
corners and edge it was built with. An entry that is DEALT rather than
printed carries its own `opacity` and `slide`; what tells one entry from
the next is the run's own `staggerChildren`, chained onto what `legend`
returns.

### A fraction drawn — `Meter.h`

| | |
| --- | --- |
| `meter(Meter)` | a fraction along a bar, with a label over it at the left and its reading at the right; `level` is the bound spelling, scaled rather than sized, for a bar that moves every frame; `keyline` and `inset` set it in a bezel |
| `gauge(Gauge)` | the same reading around a dial, over `geometry::shapes::sector` |

```cpp
sketch::kit::meter({.fraction = load, .label = "cache",
                    .reading = "74%", .width = Dimension(220)});
```

A live fraction is a re-describe rather than a binding: the filled part
is a width, and a width is layout.

`keyline` and `inset` make the rail a BEZELLED gauge — a line drawn
inside its own box and a bar held off that line — where the same reading
on a sheet is a bare bar.
### A frame with scales, and layers as functions of it — `Chart.h`

| | |
| --- | --- |
| `plot(key, frame, layers)` | the frame, and the layers over it in the order they were written, each filling the plot's own box |
| `axis(Ruler)` | one of the frame's two scales drawn: its line, its ticks, and the numbers under them |
| `rules(Rules)` | hairlines across the field at the domain values a curve is read against, stroked with the same kind of pen |
| `trace(f, Trace)` | a function of one variable walked across the x domain and stroked with `Trace::pen` — a width, a dash and a cap — and gated along its own length by `Trace::along`, which is the curve drawing itself on |
| `area(f, Area)` | the band between that curve and a base, filled |
| `marks(rows, mark, Marks)` | one element per row, placed where the frame maps its datum |
| `segments(rows, mark, Segments)` | one element per row at the bounds of its OWN TWO ENDS, shaped as the line between them — a chord, a residual vector, a whisker, a link |
| `bands(rows, Bands)` | the band each row owns drawn out to its value — a bar on a Cartesian frame, a wedge on a polar one; `along` says which scale hands out the bands and `base` where they grow from |
| `label(words, x, y, Anchor)` | a word at a point of the field |

```cpp
sketch::kit::plot("s", {.x = {.domain = {0.5, 2}},
                        .y = {.domain = {-0.55, 0.35}}, .pad = 2},
                  {sketch::kit::axis({.of = sketch::kit::Axis::Y,
                                      .ticks = {0}}),
                   sketch::kit::rules({.x = {1.0}}),
                   sketch::kit::trace(exact, {.width = 1.4f}),
                   sketch::kit::label("s_exact", 1.6, 0.2)})
    .width(324)
    .height(64)
```

**THE FRAME IS WHAT THE TWO AXES MEAN, AND NOTHING ABOUT PIXELS.** A
`sketch::kit::Plot` carries two `data::Scale`s, the room it keeps inside its
box and, where it is a wheel rather than a plane, a `sketch::kit::Polar`.
Its RANGES are the box's: x runs across and y runs UP, inside the pad on
every side, and on a polar frame x runs over the sweep and y from the inner
radius out to the rim. **So a sketch states domains, transforms, steps and
band paddings, and never computes a pixel from a datum** — which is the
whole reason this lives here rather than in the compose kit: it is the one
component in this library that needs `data::Scale`, and the compose kit does
not link SigilData.

**A LAYER IS A FUNCTION OF THAT FRAME.** `sketch::kit::Layer` is called with
the frame, the plot's key and the layer's own index in the run, and takes the
parameters it names, so `[](const Plot& f) { return … ; }` and `[] { return
… ; }` are layers too: a drawing of the sketch's own goes into a plot beside
the library's and reads the same mapping. `Plot::at` is the whole of what
such a layer needs — the point a datum lands on, band-centred where the
transform has bands — with `Plot::scale` for the band's own extent,
`Plot::centre`, `Plot::radius`, `Plot::angle` and `Plot::radiusFraction` for
a wheel's.

**CARTESIAN AND POLAR ARE ONE `plot`.** A band angle scale with a
square-root radius scale is the coxcomb; the wedge, the ring, the spoke and
the label on the rim are the same layers reading the same two scales. That
is why the coordinates are a property of the FRAME: a second component would
be a second arithmetic, and the two would drift.

**A LAYER EITHER DRAWS A PATH OR PLACES ELEMENTS**, and which one it is
decides what it can do. `axis` and `area` are keyed recordings and `rules`
and `trace` are keyed SHAPES — one node each, however many samples — and
all four prune on the plot's key, because a callable compares to nothing
and the key is the caller's statement that this is the same drawing. A
SHAPE is what a curve that draws itself on has to be: a span gate runs
along a node's outline, so `Trace::along` is `spans::upTo(…)` and the same
key still prunes it. The dots a reference publishes on such a curve are a
`marks` layer beside it — one path and N elements are two readings, and a
run of dots that is a layer of its own can be keyed, staggered and hit.
`marks`, `bands` and `label` are containers whose LAYOUT
SCHEME places each child at its datum's position, so a mark is a real
element: keyed, animatable, hit-testable, and free to be anything the sketch
can build. A plot does not size itself — every layer is absolute against its
box — so the width and the height are the caller's, as a console's placement
is.

**A BAND IS HANDED OUT BY ONE SCALE AND GROWN BY THE OTHER.** The band
layer's `along` is `Axis::X` for the column chart — a band across, grown up to the y its
reader answers — and `Axis::Y` for the row reading, where the band runs down
and the bar across and the name at the left is the y axis's own tick.
Its `base` is the value on the growing scale that a band starts from: 0
for a column standing on its axis, a centre value for a reading of
DEVIATIONS, which puts a shortfall on one side of that rule and a surplus on
the other. A polar frame reads its bands along the ANGLE whichever `along`
says, because a wedge is a band of the sweep and the radius is the reading.

**A WEDGE STANDS IN THE BOUNDS OF ITS OWN SECTOR**, not in the square of its
disc: a 30° wedge inscribed in its whole disc is a bake nine parts
transparent, and a wheel of seventy-two of them pays that per wedge per
entrance frame. Its PIVOT is still the hub, wherever in or out of its own
box that falls, so a wedge that grows in grows out of the centre of the
wheel. Both are the layer's, so a part that states a shape or a transform
origin of its own is overruled on a polar frame.

#### The classes a chart draws

Every part names a class and reads its look from the `weave::StyleSheet` in
force where it lands. `Theme::styleSheet()` registers all eight, so a plot
under a `page()` is dressed without the sketch saying anything, and a sketch
that wants otherwise states a sheet of its own on the plot or on its root —
never a prop, because a colour is not content.

| | | |
| --- | --- | --- |
| `plotAxis` | an axis line | `Palette::ash` |
| `plotTick` | a tick mark, and the number under it — one mark, one colour | `TypeScale::captionLabel` in `Palette::ash` |
| `plotRule` | a hairline across the field | `Palette::rule` |
| `plotTrace` | a curve | `Palette::figure` |
| `plotArea` | the band under a curve | `Palette::figure`, dimmed |
| `plotMark` | a datum's own element | `Palette::figure` |
| `plotBar` | the band a datum is drawn as | `Palette::figure` |
| `plotLabel` | a word placed in the field | `TypeScale::captionLabel` in `Palette::ink` |

A RECORDING READS ITS COLOUR THROUGH THE INK IN FORCE, which is what the
class resolves to, exactly as text's colour is — `Fill::currentInk()` is the
spelling where a fill is demanded — so the six classes that dress a drawing
name a colour and nothing else, and only `plotTick` and `plotLabel` carry type,
because only they set words. What stays a prop is a stroke width, a radius,
a sampling count and a distance: geometry, never look.

**A PLOT OF SEVERAL SERIES NAMES A CLASS PER LAYER.** `styleClass` on a
layer's props is the class it reads instead of the one its part is named
for — the same kind of part under a different entry on the sheet, which is
what a class attribute is for — so a sketch with three curves registers
three names on the sheet it states and writes one of them at each `trace`.
The axis is the exception and carries no override: there is one axis per
scale, and its line and its ticks are two classes already.

```cpp
weave::StyleSheet look = sketch::kit::houseTheme().styleSheet();
look.set("second", {.color = kCool});
…
sketch::kit::plot("decay", frame,
                  {sketch::kit::trace(fast),
                   sketch::kit::trace(slow, {.styleClass = "second"})})
```

**A TICK'S NUMBER IS A `Part`.** `Ruler::tickLine` is a function of the
VALUE, because how a number reads is the data's business and not the kit's;
empty is the value to three significant figures in the class `plotTick`, which
`tickLabel` is the leaf of. The band layer's own `part` is the same door for
the element a datum is drawn as, and `marks`'s mark is the caller's
outright — a bar that must be a gradient, a sprite or a stack of two is that
element, and the layer still places it.

### A window's share of what it scrolls — `Scrollbar.h`

| | |
| --- | --- |
| `Scrolled::thumb()` | the length a thumb has to be, and the travel that leaves it, from the window, the whole strip and the track |
| `scrollbar(Scrollbar)` | a stepper at each end, a track between them, and the thumb standing somewhere along it |

```cpp
sketch::kit::scrollbar({.leading = stepper(true), .trailing = stepper(false),
                        .thumb = slider(),
                        .scrolled = {.view = shown, .content = whole,
                                     .track = trackH},
                        .position = envelope().target(0.0f, travel)})
    .width(Dimension(19))
    .padding(2)
```

A THUMB IS A READING. Its length is the window's share of the strip, so
a bar drawn at one length while the content is scrolled over another is a
thumb that slides off its own track, or one that stops short of the end
and says the last rows are not there. `Scrolled::thumb()` is that
arithmetic, and it is a value on its own because a caller that binds a
live position needs the travel to map its envelope onto.

Every part is the caller's Element — a bevelled Motif slider, a styled IE
thumb, a pixel sprite — and what is returned is the bar's own box, so the
shell's fill, its keyline, its padding and its gutter are chained onto it
in the caller's own words. What the component keeps is the arithmetic and
the placement: a thumb that positioned itself would be the second opinion
this exists to remove. A thumb measured off an original rather than
computed from anything states its `thumbLength` instead.

### What stands behind and around — `Panel.h`

| | |
| --- | --- |
| `backdrop(Backdrop)` | the theme's ground over the whole surface, shaded toward the corners and grained, over `compose::kit::vignette` and `grained` |
| `panel(Panel, content)` | a titled region of a page: an eyebrow over a title, a note ranged at the far edge of the head, and the content on the plate under it |
| `frame(Frame, screen)` | a device's chrome: an outer shell, a screen inset into it by the bezel on every side, and the plate its word is engraved on |

```cpp
sketch::kit::frame({.width = Dimension(275), .height = Dimension(116), .bezel = 6,
                    .plate = "MAIN WINDOW"}, tape);
```

`Backdrop::over` is the canvas — a vignette is a fact about an extent,
which is the one thing here a theme cannot carry. `Frame::keyline` unset
is the theme's rule and `Fill::none()` draws none, which is what a shell
whose only rule runs round its OUTER edge asks for.

**A PANEL IS A PLATE, NOT A SPECIMEN WELL.** `Panel::body` is a `Well`
whose unset padding is the theme's PANEL padding, whose unset radius is
its panel corner and whose unset keyline is its rule — a piece of
furniture is padded, rounded and ruled, where a picture's surface is
grounded and flush. Its three lines are set in the classes `eyebrow`,
`title` and `captionNote`, and the two gaps of its head are the theme's
caption gaps, so a region and the cell beside it breathe alike.

### A log, and things along an axis — `Console.h`, `Ticker.h`

| | |
| --- | --- |
| `console(Console)` | N feeds of one monospaced voice on one bordered plate, over `compose::kit::console` |
| `ticker(Ticker)` | a strip crawling past a window, over `compose::kit::marquee` |
| `timeline(Timeline)` | a rail marked off with ticks and the words that name them |

```cpp
sketch::kit::console({.feeds = {&checks}, .levels = {{"fail", kAlarm}}})
    .rect({64, 1420, kW - 64, 1576});
```

A console does not place itself: a component that decides where it goes
cannot be reused.

A timeline's `ink` is the whole mark's — a tick and the word under it are
one mark and take one colour. A mark that states its own `ink` keeps its
word out of the scale's, the way a legend entry does.

## What is NOT here, and where it is

A leaf may not invent what an ancestor should own.

* The run of cells, the captioned cell's arrangement, a titled region's
  head and the sheet's own layout — `compose::kit::cells`, `panelGrid`,
  `cell`, `well`, `panel`, `sheet`. This library
  puts values into those; it does not restate them.
* A ground's vignette and its grain as fills — `compose::kit::vignette`
  and `compose::kit::grained` (`kit/Ground.h`). `backdrop` puts the
  theme's values into those; it does not build a shader.
* Ring and grid arithmetic — `geometry::arrange`. Do not respell it with
  `std::cos` and `std::sin`; the two round differently.
* Entrances, loops and the stagger cascade — `compose::kit::fx`, spelled
  in compose's own types.
* A memoised typeface — `weave::ports::face()`. This library holds no
  font cache; it holds the one face its own theme is set in.
* A resource that is not the sketch's own words — an image, a video, a
  blob, a probe, a table, a database — `ctx.assets`. `passage` and
  `Document` are the two readers here, and they are here because the words
  are the only resource whose exact bytes decide a plate.
* The mapping a plot's two axes are — `data::Scale`, with its domain, its
  range, its transform and its tick ladder. `Chart.h` puts a box's own size
  into two of those and reads `Scale::apply`; it holds no arithmetic of its
  own, and it is the one component here that links SigilData.
* Numbers a sketch measured about its own execution — `ctx.measured`,
  before they reach any component here. A sketch that draws its own
  timings into its own plate differs from itself between runs.

## Boundaries

It draws nothing and holds no kernel state, and nothing links it back —
`SigilSketches` links it, and no library below does. It LINKS THE SKETCH
ARCHIVE, because `stage()` writes a sketch's `CanvasSpecification` through the
canvas runtime's own context: no device backend and no window come with
that, but the reload engine and the headless renderer stand in the same
archive and do. It is PIC, because a hot-reloaded sketch's dylib force-loads
it out of the host.

## Build and test

```sh
cmake --build build --config Release --target sketch_test
ctest --test-dir build -C Release -R '^SketchKit' --output-on-failure
```

`kit/test/` asserts the claim a migrated sketch's plate rests on:
that the theme is a comparable value a scope binds and shadows, and that
every component here draws — **in pixels** — exactly what the compose kit
spelled by hand with the same values draws.
