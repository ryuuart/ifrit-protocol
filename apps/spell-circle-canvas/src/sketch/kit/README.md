# SigilSketchKit — the sheet a sketch stands on

A specimen sheet in this repository is a page with a title, a subtitle
and a footer, ruled off from a run of captioned cells, each cell a
picture in a grounded well with the call over it and the remark under.
Every one of those sheets used to open with the same fifty lines: five
colours, a two-line style helper, a five-line monospaced one, a
seven-line caption value, three calls declaring the canvas, and nine
fields of margins and rules inside the sheet literal. **This library is
those fifty lines, stated once**, so the file someone opens to study
`Border`'s four modes is a file about `Border`'s four modes.

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

`Theme` holds three values and one choice: a `Palette` of five colours, a
`TypeScale` of five `Register`s and two faces, a `Spacing` of the
distances between things, and where a cell's caption lines stand.

It arrives at a component through **`sigil::core::env`**, the
reconciler's inherited value, aliased here as `sketch::kit::Provide`:

```cpp
sketch::kit::Theme sheetTheme() {
  sketch::kit::Theme paper = sketch::kit::houseTheme();
  paper.palette.ground = {0.945f, 0.937f, 0.918f, 1};
  paper.palette.ink = {0.114f, 0.106f, 0.098f, 1};
  return paper;
}

const sketch::kit::Provide look(sheetTheme());   // five lines, not forty
ctx.composer.render(sketch::kit::page({…}, content));
```

A describe phase is an ordinary C++ call tree evaluated bottom-up, so the
describe-time stack *is* the description tree and dynamic scope is the
C++ answer to inheriting down it. `sketch::kit::theme()` is the read;
with nothing bound it answers `houseTheme()`, which is what makes a
component correct on its own.

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

Each takes a props struct, reads the theme, and delegates to
`compose::kit`. Props are the caller's facts; the theme is the look.

| | |
| --- | --- |
| `stage(ctx, Stage)` | the canvas, the ground and the capture moment in one call — the whole `CanvasSpec`, with the ground taken from the theme unless the stage names one |
| `page(Page, content)` | the sheet over the whole canvas: title, subtitle and footer set in the theme's three registers, its margins, its ground and its hairline |
| `well(Well, surface)` | the fixed surface a specimen is shown in, on the theme's cell ground |
| `caption(measure, label, note, body)` | one captioned specimen in the theme's voice; `measure` is the cell's own width, the one distance a caption cannot inherit |
| `passage(ctx, name)` | the prose at `res://passages/<name>`, minus the newlines a file ends with — the two thousand words a sheet about setting a page is SET IN, kept beside the sketch rather than typed into it |

`Page::ruled` is `false` for a sheet that rules neither header nor
footer, and `Page::ground` names a fill for a sheet whose ground is not a
flat colour, because a palette holds colours and a gradient is not one. A
well that must paint nothing passes `Fill::none()`; a well that must
paint something else passes that.

## What is NOT here, and where it is

A leaf may not invent what an ancestor should own.

* The run of cells, the captioned cell's arrangement and the sheet's own
  layout — `compose::kit::cells`, `cell`, `well`, `sheet`. This library
  puts values into those; it does not restate them.
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

It draws nothing, holds no kernel state, links no device and no runtime,
and nothing links it back — `SigilSketches` links it, and no library
below does. It is PIC, because a hot-reloaded sketch's dylib force-loads
it out of the host.

## Build and test

```sh
cmake --build build --config Release --target sketch_kit_test
ctest --test-dir build -C Release -R sketch_kit_test --output-on-failure
```

`sketch_kit_test` asserts the claim a migrated sketch's plate rests on:
that the theme is a comparable value a scope binds and shadows, and that
every component here draws — **in pixels** — exactly what the compose kit
spelled by hand with the same values draws.
