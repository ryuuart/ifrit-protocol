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

#include <include/core/SkCanvas.h>
#include <sigilcompose/core/Core.h>
#include <sigilcompose/kit/Specimen.h>
#include <sigilmaterial/pattern/Patterns.h>
#include <sigilmaterial/pattern/Tile.h>
#include <sigilsketch/canvas/Sketch.h>
#include <sigilsketch/kit/Kit.h>

#include <functional>
#include <string>
#include <utility>
#include <vector>

namespace sketch = sigil::sketch;
namespace material = sigil::material;
namespace pattern = sigil::material::pattern;

using namespace sigil::compose;
using material::Color;
using sigil::compose::toU8;

namespace {

constexpr SkSize kCanvas = {1100, 636};
constexpr float kCell = 252;
constexpr float kPicture = 194;

constexpr float kPhase = 17;  // how far the sett slides along +x, px
constexpr float kPan = 21;    // the mapping's pan, px

/** The house sheet, in this one's own look. */
sketch::kit::Theme sheetTheme() {
  sketch::kit::Theme look = sketch::kit::houseTheme();
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

/** The bake every bottom-row cell remaps. Held as a function-local
 *  static, because the bake IS the identity: a Tile re-minted per draw
 *  would carry fresh shared state and re-render every frame. */
const pattern::Tile& banked() {
  static const pattern::Tile tile = pattern::sequence(sett());
  return tile;
}

/** A pixel-grid tile, for the pair that differ only in their filter. */
const pattern::Tile& squares() {
  static const pattern::Tile tile =
      pattern::checker(4, {0.16f, 0.19f, 0.24f, 1}, {0.80f, 0.72f, 0.46f, 1});
  return tile;
}

void paintTile(SkCanvas& canvas, const pattern::Tile& tile, SkSize size) {
  SkPaint paint;
  paint.setShader(tile.texture().shader());
  canvas.drawRect(SkRect::MakeWH(size.width(), size.height()), paint);
}

Element cell(const char* call, const std::string& note,
             std::function<void(SkCanvas&, SkSize)> draw) {
  return sketch::kit::caption(
      kCell, toU8(call), toU8(note),
      sketch::kit::well(
          {.width = kCell, .height = kPicture},
          custom(call, [draw = std::move(draw)](SkCanvas& canvas,
                                                const PaintContext& pc) {
            draw(canvas, pc.size);
          })));
}

Element swatch(const char* call, const std::string& note, pattern::Tile tile) {
  return cell(call, note,
              [tile = std::move(tile)](SkCanvas& canvas, SkSize size) {
                paintTile(canvas, tile, size);
              });
}

}  // namespace

struct PatternSequence final : sketch::Sketch {
  void setup(sketch::SketchContext& ctx) override {
    const sketch::kit::Provide look(sheetTheme());
    // nothing moves; the sheet is complete at once
    sketch::kit::stage(ctx, {.size = kCanvas, .captureAt = 0.05});

    ctx.composer.render(sketch::kit::page(
        {.title = toU8("PATTERN SEQUENCE \xc2\xb7 pattern::sequence and "
                       "Tile's mapping"),
         .subtitle = toU8("dials \xc2\xb7 the runs and their period "
                          "\xc2\xb7 the phase (17 px) \xc2\xb7 the pan "
                          "(21 px) \xc2\xb7 the scale, the rotation and "
                          "the filter"),
         .footer = toU8("the top row rebakes and the bottom row does "
                        "not \xe2\x80\x94 scale, rotation, pan and "
                        "filter act on the sampling matrix, so one "
                        "bake serves every cell under the rule")},
        kit::cells(
            {.cells =
                 {kit::cells(
                      {.cells =
                           {swatch(
                                "pattern::sequence(runs)",
                                kit::format("four runs along +x \xc2\xb7 the "
                                            "period is their sum, %.0f px",
                                            (double)period(sett())),
                                pattern::sequence(sett())),
                            swatch("sequence(runs, 17)",
                                   "the phase slides the whole sequence "
                                   "along +x, wrapped \xc2\xb7 a "
                                   "different PROGRAM, so a different "
                                   "bake",
                                   pattern::sequence(sett(), kPhase)),
                            swatch("sequence({{18, ink}, {6, gold}})",
                                   "as many colours as there are runs "
                                   "\xc2\xb7 two of them is an awning, "
                                   "four is a sett",
                                   pattern::sequence(
                                       {{18, {0.13f, 0.20f, 0.24f, 1}},
                                        {6, {0.86f, 0.76f, 0.44f, 1}}})),
                            swatch("pattern::stripes(6, 12, gold)",
                                   "the one-colour case has its own "
                                   "name \xc2\xb7 the painted width and "
                                   "the gap, rather than a run list",
                                   pattern::stripes(
                                       6, 12, {0.86f, 0.76f, 0.44f, 1}))},
                       .gap = 14}),
                  kit::cells(
                      {.cells =
                           {swatch("banked.offset({21, 0})",
                                   "the mapping pans the repeat in the "
                                   "SAMPLED space's px \xc2\xb7 no "
                                   "rebake, and the seam never shows",
                                   pattern::Tile(banked()).offset({kPan, 0})),
                            swatch(
                                "banked.rotate(90).scale(1.4)",
                                "rotate, then scale, then translate "
                                "\xc2\xb7 a rotated repeat stays "
                                "seamless because the bake never "
                                "turned",
                                pattern::Tile(banked()).rotate(90).scale(1.4f)),
                            cell("one bake, drawn crossed",
                                 "the sett along +x and the same bake "
                                 "turned a right angle over it \xc2\xb7 "
                                 "which is what a tartan is",
                                 [](SkCanvas& canvas, SkSize size) {
                                   paintTile(canvas, banked(), size);
                                   canvas.saveLayerAlphaf(nullptr, 0.55f);
                                   paintTile(canvas,
                                             pattern::Tile(banked()).rotate(90),
                                             size);
                                   canvas.restore();
                                 }),
                            cell("filter(kNearest) | filter(kLinear)",
                                 "linear is the default and is right "
                                 "for an organic tile \xc2\xb7 on a "
                                 "pixel grid it is wrong, which the "
                                 "seam down the middle says",
                                 [](SkCanvas& canvas, SkSize size) {
                                   canvas.save();
                                   canvas.clipRect(SkRect::MakeWH(
                                       size.width() * 0.5f, size.height()));
                                   paintTile(
                                       canvas,
                                       pattern::Tile(squares()).scale(5).filter(
                                           SkFilterMode::kNearest),
                                       size);
                                   canvas.restore();
                                   canvas.save();
                                   canvas.clipRect(SkRect::MakeLTRB(
                                       size.width() * 0.5f, 0, size.width(),
                                       size.height()));
                                   paintTile(
                                       canvas,
                                       pattern::Tile(squares()).scale(5).filter(
                                           SkFilterMode::kLinear),
                                       size);
                                   canvas.restore();
                                 })},
                       .gap = 14})},
             .column = true,
             .gap = 18})));
  }
};

SIGIL_SKETCH(PatternSequence, "Kit \xc2\xb7 API",
             "a coloured sett as a baked tile, then one bake panned, "
             "turned, crossed with itself and sampled two ways \xe2\x80\x94 "
             "none of which rebakes it")
