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
        {.title = toU8("THE RULE AND THE STRANDS"),
         .subtitle = toU8("dials · the width and the inset"),
         .footer = toU8("a crossing is discovered, not declared")},
        kit::cells({.cells = {cell(…), cell(…)}, .gap = 10})));
  }
};
```

## The theme, and why it is inherited rather than passed

`Theme` holds three values and one choice: a `Palette` of six colours, a
`TypeScale` of seven `Register`s and two faces, a `Spacing` of the
distances a sheet is set by, and where a cell's caption lines stand.

It arrives at a component through **`sigil::core::env`**, the
reconciler's inherited value, aliased here as `sketch::kit::Provide`:

```cpp
sketch::kit::Theme sheetTheme() {
  sketch::kit::Theme paper = sketch::kit::houseTheme();
  paper.palette.ground = {0.945f, 0.937f, 0.918f, 1};
  paper.palette.ink = {0.114f, 0.106f, 0.098f, 1};
  return paper;
}

const sketch::kit::Provide look(sheetTheme());   // bound for this scope
ctx.composer.render(sketch::kit::page({…}, content));
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

**Bind it where the tree is DESCRIBED, not where setup runs.** A sketch
that describes again — from `update()`, when its data changes — describes
outside setup's scope, and a theme bound only there would not be in it.
Put the `Provide` at the top of whatever function builds the tree, and
another in `setup` if `stage()` is to take its ground from the same
theme.

**One caveat.** A callable the kernel invokes later — a `custom()` paint
program, a memo's deferred describe — runs with no scope. Capture the
colours such a lambda needs by value at the call site, where the scope
still stands.

## The components

Each takes a props struct, reads the theme, and returns an Element built
by plain composition over `compose::kit`. Props are the caller's facts;
the theme is the look. Children arrive as Elements and slots as props, so
a component nests inside another the way a box does — which is the point:
a sketch is meant to read as its algorithm plus a run of these calls, not
as a thousand lines of furniture.

**A GROUND IS EITHER OF TWO THINGS.** Every field here that paints an
area — `Well::ground`, `Frame::shell` and `Frame::screen`, `Page::ground`,
`Console::ground`, `Backdrop::ground`, a meter's track and its bar, a
scrollbar's track, a ticker's rail, a chip's ground, a legend's swatches —
is a `Ground`: one value holding a `compose::Fill` **or** a material,
converting from either, so
`Fill::color(kPlate)`, a `material::skia::Paint` and a bare
`material::Material` are each written where the ground is asked for and
none of them is wrapped. A fill goes onto the node as it always did; a
material goes on the way `Element::fill` puts one there, so a static one
collapses to a Fill and rides the same caching and prune path a colour
does, while a live or geometry-dependent one stays whole and is resolved
against the frame it is drawn at. A reconstruction whose ground is
quarried stone is the reason for the second form: the picture holds a
recipe, and a component that took only a `Fill` would turn that half of
the tree away.

### The surface — `Page.h`, `Cells.h`, `Passage.h`

| | |
| --- | --- |
| `stage(ctx, Stage)` | the canvas, the ground and the capture moment in one call — the whole `CanvasSpec`, with the ground taken from the theme unless the stage names one |
| `page(Page, content)` | the sheet over the whole canvas: title, subtitle and footer set in the theme's three registers, its margins, its ground and its hairline |
| `well(Well, surface)` | the fixed surface a specimen is shown in, on the theme's cell ground — with `corners` and a `keyline`, the PLATE a panel stands on, and with a `recess`, the hole punched in one |
| `caption(measure, label, note, body)` | one captioned specimen in the theme's voice; `measure` is the cell's own width, the one distance a caption cannot inherit |
| `cells(Run)` | a run of cells along one axis at the theme's gutter, each at its own width |
| `columns(Columns)` | equal shares of the width, one per cell — what `cells` cannot do, because a fixed width does not know how wide the page is |
| `panelGrid(PanelGrid)` | the same, wrapped every N, with a short last row keeping its share |
| `passage(ctx, name)` | the prose at `res://passages/<name>`, minus the newlines a file ends with — the prose a sheet about setting a page is SET IN, kept beside the sketch rather than typed into it |

```cpp
sketch::kit::page({.title = toU8("THE STROKE ATLAS")},
                  sketch::kit::panelGrid({.cells = panels, .columns = 4}));
```

`Page::ruled` is `false` for a sheet that rules neither header nor
footer, and `Page::ground` names a fill for a sheet whose ground is not a
flat colour, because a palette holds colours and a gradient is not one. A
well that must paint nothing passes `Fill::none()`; a well that must
paint something else passes that.

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

`keyline` unset draws none — a specimen well is grounded and unruled, and
that is the common case. Where it is set it is drawn INSIDE the well's own
box: a rule centred on the boundary puts half its width outside, and a
plate that is not the width it was given is the one thing a fixed surface
may not be. `paddingY` is there because a plate is often set tighter down
than across, which one distance cannot say.

`recess` is the well read as a HOLE punched in what holds it rather than
as a patch of ground on it: a shadow cast inside its own edge, and a hard
sunken lip under that shadow — the blur says how deep the surface goes
and the lip says where it breaks. Unset is flush, which is the specimen
well.

### What announces something — `Heading.h`

| | |
| --- | --- |
| `titleCard(TitleCard)` | an eyebrow over a title over a subtitle, optionally ruled, with the notes ranged at its far edge — the header half of a page, standing on its own |
| `sectionHeader(SectionHeader)` | a name at the left, a remark at the right, and the rule that fills what the two leave between them |

```cpp
sketch::kit::titleCard({.eyebrow = {toU8("SIGIL · COMPOSE")},
                        .title = {toU8("THE STROKE ATLAS")},
                        .subtitle = {toU8("every rail, at one width")}});
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
    {.eyebrow = {.words = toU8("MET OFFICE"), .opacity = beat(0.05f, 0.55f)},
     .title = {.words = toU8("THE SHIPPING FORECAST"),
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
| `table(rows, Table)` | N columns each at its own width, the ones that carry a number in the figure register, with a mark before the first — the reading a pair cannot hold |

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
sketch::kit::table(rows, {.columns = {{126, true}, {46, true}, {66}, {}},
                          .swatch = 9});
```

A figure a sketch measured about its own execution goes through
`ctx.measured` **before** it reaches here. These components arrange a
row; what the number is, and whether it is pinned, is the sketch's.

### Colour, named — `Legend.h`

| | |
| --- | --- |
| `legend(Legend)` | swatch-and-label rows, stacked or run along a line |
| `swatchStrip(SwatchStrip)` | a ramp's steps in order at one size, with words under the ones that have them |
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
sketch::kit::meter({.fraction = load, .label = toU8("cache"),
                    .reading = toU8("74%"), .width = Dim(220)});
```

A live fraction is a re-describe rather than a binding: the filled part
is a width, and a width is layout.

`keyline` and `inset` make the rail a BEZELLED gauge — a line drawn
inside its own box and a bar held off that line — where the same reading
on a sheet is a bare bar.

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
    .width(Dim(19))
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
| `frame(Frame, screen)` | a device's chrome: an outer shell, a screen inset into it by the bezel on every side, and the plate its word is engraved on |

```cpp
sketch::kit::frame({.width = Dim(275), .height = Dim(116), .bezel = 6,
                    .plate = toU8("MAIN WINDOW")}, tape);
```

`Backdrop::over` is the canvas — a vignette is a fact about an extent,
which is the one thing here a theme cannot carry. `Frame::keyline` unset
is the theme's rule and `Fill::none()` draws none, which is what a shell
whose only rule runs round its OUTER edge asks for.

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

* The run of cells, the captioned cell's arrangement and the sheet's own
  layout — `compose::kit::cells`, `cell`, `well`, `sheet`. This library
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
* A resource that is not prose — an image, a video, a blob, a probe —
  `ctx.assets`. `passage` is the one reader here, and it is here because
  a passage is the only resource whose exact bytes decide a plate.
* Numbers a sketch measured about its own execution — `ctx.measured`,
  before they reach any component here. A sketch that draws its own
  timings into its own plate differs from itself between runs.

## Boundaries

It draws nothing and holds no kernel state, and nothing links it back —
`SigilSketches` links it, and no library below does. It LINKS THE SKETCH
ARCHIVE, because `stage()` writes a sketch's `CanvasSpec` through the
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
