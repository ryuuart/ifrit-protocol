/** @file
 * pattern_sequence — a coloured sett as a tile, and what the mapping can
 * change without touching the bake.
 *
 * A `Tile` is a repeating texture baked ONCE from a program. The bake is
 * memoised on shared state and regeneration is explicit: `seed(n)` or a
 * new program drops it and the next `image()` re-renders. Everything
 * else — `scale`, `rotate`, `offset`, `filter` — acts on the SAMPLING
 * matrix only, so a rotated repeat stays seamless and costs no rebake.
 *
 * That split is the whole reason a Tile is a value and not a picture.
 * The top row here changes the PROGRAM (a different sett, a different
 * phase) and pays for a bake each time; the bottom row changes only the
 * mapping of one bake, and a tartan — the same sett crossed with itself
 * at a right angle — is two draws of one bake.
 *
 * The bake IS the identity, which decides where a Tile is stored: hold
 * one where assets are held. Re-minting a Tile every frame mints fresh
 * shared state with no bake in it, so every frame re-renders it.
 *
 * EDIT THESE FIRST
 *   kSett   — the runs, {width px, colour}. The period is their sum.
 *   kPhase  — how far the sequence slides along +x, px.
 *   kPan    — the offset the mapping pans the repeat by, px.
 */

// TAGS: Patterns/Tiling

#include <sigilcompose/core/Core.h>
#include <sigilcompose/kit/Document.h>
#include <sigilcompose/kit/Specimen.h>
#include <sigilmaterial/pattern/Patterns.h>
#include <sigilmaterial/pattern/Tile.h>
#include <sigilmaterial/skia/Paint.h>
#include <sigilsketch/canvas/Sketch.h>
#include <sigilsketch/kit/Kit.h>

#include <string>
#include <utility>
#include <vector>

namespace sketch = sigil::sketch;
namespace material = sigil::material;
namespace pattern = sigil::material::pattern;
namespace mskia = sigil::material::skia;

using namespace sigil::compose;
using material::Color;

namespace {

constexpr SkSize kCanvas = {1200, 810};
constexpr float kCell = 268;
constexpr float kPicture = 190;

constexpr float kPhase = 17;  // how far the sett slides along +x, px
constexpr float kPan = 21;    // the mapping's pan, px

/** The specimen sheet, in this one's own look. */
sketch::kit::Theme sheetTheme() {
  sketch::kit::Theme look = sketch::kit::studyTheme();
  look.palette.cellGround = {0.09f, 0.095f, 0.11f, 1};
  return look;
}

/** The sett: four runs whose widths sum to the period. */
std::vector<std::pair<float, Color>> sett() {
  return {{26, {0.13f, 0.20f, 0.24f, 1}},
          {7, {0.86f, 0.76f, 0.44f, 1}},
          {14, {0.36f, 0.14f, 0.16f, 1}},
          {5, {0.66f, 0.72f, 0.70f, 1}}};
}

float period(const std::vector<std::pair<float, Color>>& runs) {
  float sum = 0;
  for (const auto& run : runs) sum += run.first;
  return sum;
}

/** The bake every bottom-row cell remaps. THE BAKE IS THE IDENTITY: a Tile
 *  re-minted per draw carries fresh shared state and re-renders every frame,
 *  where a COPY of one shares it. So the sketch holds one and every cell
 *  copies from it — held there rather than in a static, since this file is a
 *  dylib a reload unloads. */
pattern::Tile bankedTile() { return pattern::sequence(sett()); }

/** A pixel-grid tile, for the pair that differ only in their filter. */
pattern::Tile squaresTile() {
  return pattern::checker(4, {0.16f, 0.19f, 0.24f, 1},
                          {0.80f, 0.72f, 0.46f, 1});
}

/** A tile as a paint: what a node is grounded in. */
mskia::Paint painted(const pattern::Tile& tile) {
  return mskia::Paint::shader(tile.texture().shader());
}

/** One cell: the tile as the ground of the well the specimen stands in. */
Element swatch(const pattern::Tile& tile) {
  return sketch::kit::well(
      {.width = kCell, .height = kPicture, .ground = painted(tile)});
}

}  // namespace

struct PatternSequence {
  pattern::Tile banked = bankedTile();
  pattern::Tile squares = squaresTile();

  void setup(sketch::SketchContext& ctx) {
    const sketch::kit::Provide look(sheetTheme());
    // nothing moves; the sheet is complete at once
    sketch::kit::stage(ctx, {.size = kCanvas, .captureAt = 0.05});

    Element crossed = swatch(banked).children(
        {box()
             .inset(0)
             .fill(painted(pattern::Tile(banked).rotate(90)))
             .opacity(0.55f)});
    Element filters =
        sketch::kit::well({.width = kCell, .height = kPicture})
            .row()
            .children({box().flexGrow(1).fill(
                           painted(pattern::Tile(squares).scale(5).filter(
                               SkFilterMode::kNearest))),
                       box().flexGrow(1).fill(
                           painted(pattern::Tile(squares).scale(5).filter(
                               SkFilterMode::kLinear)))});
    ctx.composer.render(sketch::kit::page(
        {.title = "Bake the pattern, move the sampling",
         .subtitle = kit::formatted(
             "Four coloured runs make one %.0f px repeat · program edits and "
             "sampling edits have different costs",
             (double)period(sett())),
         .footer =
             "Keep the Tile with your assets. Copies share its bake; scale, "
             "rotation, offset and filtering alter how that bake is sampled."},
        box().column().gap(22).children(
            {document::h2(
                 "01 / CHANGE THE PROGRAM · A DISTINCT BAKE FOR EACH CASE"),
             sketch::kit::comparison(
                 {.cases = {{.title = "FOUR RUNS",
                             .control = "sequence · 26 + 7 + 14 + 5 px",
                             .figure = swatch(pattern::sequence(sett())),
                             .note = "The reference sett. Each run carries its "
                                     "own width and colour."},
                            {.title = "SHIFT THE PHASE",
                             .control = "sequence · phase +17 px",
                             .figure =
                                 swatch(pattern::sequence(sett(), kPhase)),
                             .note = "Phase is part of the program, so the "
                                     "shifted repeat gets its own bake."},
                            {.title = "SIMPLIFY THE SETT",
                             .control = "sequence · 18 + 6 px",
                             .figure = swatch(pattern::sequence(
                                 {{18, {0.13f, 0.20f, 0.24f, 1}},
                                  {6, {0.86f, 0.76f, 0.44f, 1}}})),
                             .note = "Two colours describe an awning instead "
                                     "of a four-colour sett."},
                            {.title = "LEAVE A GAP",
                             .control = "stripes · 6 px ink / 12 px gap",
                             .figure = swatch(pattern::stripes(
                                 6, 12, {0.86f, 0.76f, 0.44f, 1})),
                             .note = "A single colour over transparency has a "
                                     "simpler spelling."}},
                  .measure = 1120,
                  .gap = 16}),
             document::h2("02 / CHANGE THE SAMPLING · REUSE THE EXISTING BAKE"),
             sketch::kit::comparison(
                 {.cases =
                      {{.title = "PAN",
                        .control = "offset({21, 0})",
                        .figure =
                            swatch(pattern::Tile(banked).offset({kPan, 0})),
                        .note = "Move the reference tile through sampled "
                                "space, without repainting it."},
                       {.title = "TURN + SCALE",
                        .control = "rotate(90°) · scale(1.4)",
                        .figure = swatch(
                            pattern::Tile(banked).rotate(90).scale(1.4f)),
                        .note = "A continuous repeat after rotation. The baked "
                                "image never turned."},
                       {.title = "CROSS THE SAME TILE",
                        .control = "0° + 90° · upper opacity 55%",
                        .figure = std::move(crossed),
                        .note = "Two mappings of one sett make a tartan-like "
                                "intersection."},
                       {.title = "CHOOSE THE FILTER",
                        .control = "nearest / linear · both scale 5",
                        .figure = std::move(filters),
                        .note =
                            "A checker isolates the difference: hard pixels on "
                            "the left, interpolation on the right."}},
                  .measure = 1120,
                  .gap = 16})})));
  }
};

SIGIL_SKETCH(PatternSequence, "Kit · API",
             "a coloured sett as a baked tile, then one bake panned, "
             "turned, crossed with itself and sampled two ways — "
             "none of which rebakes it")
