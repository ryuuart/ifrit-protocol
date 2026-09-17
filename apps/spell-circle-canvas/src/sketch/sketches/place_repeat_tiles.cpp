/** @file
 * place_repeat_tiles — one motif many times: the copy chain in a pool,
 * and one long picture sliced into tiles.
 *
 * `instancing::place::repeat` fills a `Pool` with a repeated copy chain:
 * the translate and the rotation are LINEAR in the copy index and the
 * scale is EXPONENTIAL (`pow(scaleStep, i)`), which is why a chain that
 * shrinks keeps shrinking by the same fraction rather than by the same
 * number of pixels. The opacity ramp writes the `alphas()` lane, which
 * COMPOSES with an authored tint instead of overwriting it, and it is
 * written at all only when the two opacity arguments say something.
 *
 * `tiles::` is the other repetition: a strip longer than any texture is
 * authored as ONE element tree, baked with `snapshot()` — a picture has
 * no size limit because it is vector — and then handed to a consumer as
 * N tile-sized rasters. The slice is a clip and a translate and nothing
 * else: `tiles::window` returns that transform, and neighbouring tiles
 * share their boundary texels, so the seams vanish. `Facing::Mirrored`
 * is a statement about the CONSUMER, not the picture — a surface whose u
 * runs backwards needs the tile baked reversed to read the right way
 * round, and getting it wrong is invisible in a PNG of the tile.
 *
 * EDIT THESE FIRST
 *   kCopies — how many copies each chain lays down.
 *   kStep — the per-copy translate, px.
 *   kTiles — how many tiles the strip is cut into.
 */

// TAGS: Geometry/Layout, Patterns/Tiling

#include <include/core/SkCanvas.h>
#include <include/core/SkPicture.h>
#include <sigilcompose/core/Core.h>
#include <sigilcompose/core/Instances.h>
#include <sigilcompose/core/Tiles.h>
#include <sigilcompose/kit/Document.h>
#include <sigilcompose/kit/Placers.h>
#include <sigilcompose/kit/Specimen.h>
#include <sigilgeometry/kit/Generators.h>
#include <sigilsketch/canvas/Sketch.h>
#include <sigilsketch/kit/Kit.h>

#include <cmath>
#include <memory>
#include <string>
#include <utility>

namespace sketch = sigil::sketch;
namespace shapes = sigil::geometry::shapes;

using namespace sigil::compose;

namespace {

constexpr SkSize kCanvas = {1100, 890};
constexpr float kCell = 328;
constexpr float kPicture = 206;

constexpr size_t kCopies = 9;  // copies in each chain
constexpr float kStep = 26;    // the per-copy translate, px
constexpr int kTiles = 4;      // slices the strip is cut into
constexpr SkSize kMotif = {34, 34};
constexpr SkISize kTile = {44, 128};

constexpr SkColor4f kWarm{0.86f, 0.52f, 0.34f, 1};

/** THE ONE MOTIF every chain repeats and the strip is built from, in the
 *  ink it is stamped in — the theme's figure for a chain's cell, and the
 *  strip's own warm for every third star of the run. */
Element motif(SkColor4f ink) {
  return box()
      .width(kMotif.width())
      .height(kMotif.height())
      .shape(shapes::star(6, 0.46f, 0.14f))
      .fill(Fill::color(ink));
}

/** The plate every specimen on this sheet stands on, and the
 *  measure its caption is set to. */
const sketch::kit::Cell kSpecimen{
    .plate = {.width = kCell, .height = kPicture}};

}  // namespace

struct PlaceRepeatTiles {
  std::shared_ptr<instancing::Atlas> atlas;
  std::shared_ptr<instancing::Pool> plain, spun, faded;
  sk_sp<SkPicture> strip;

  void setup(sketch::SketchContext& ctx) {
    const sketch::kit::Provide presentation(sketch::kit::studyTheme());
    // nothing moves; the sheet is complete at once
    sketch::kit::stage(ctx, {.size = kCanvas, .captureAt = 0.05});

    atlas = std::make_shared<instancing::Atlas>();
    atlas->cell(motif(sketch::kit::theme().palette.figure), kMotif);

    plain = std::make_shared<instancing::Pool>();
    instancing::place::repeat(*plain, kCopies, {60, 108}, {kStep, 0});

    spun = std::make_shared<instancing::Pool>();
    instancing::place::repeat(*spun, kCopies, {58, 52}, {kStep, 9}, 0.18f,
                              0.90f);

    faded = std::make_shared<instancing::Pool>();
    instancing::place::repeat(*faded, kCopies, {60, 108}, {kStep, 0}, 0, 1.0f,
                              1.0f, 0.12f);

    // THE STRIP: one tree, taller than any tile, baked once. Authored as a
    // column because the tiles are tall — a transpose on the way out has
    // determinant -1 and composes with whatever mirroring the consumer
    // already applies.
    Element run = box()
                      .column()
                      .gap(8)
                      .padding(5)
                      .width(kTile.width())
                      // Dark on one side and light on the other, so a
                      // mirrored tile is legible AS mirrored.
                      .fill(linearGradient({0, 0}, {(float)kTile.width(), 0},
                                           {{0.09f, 0.10f, 0.12f, 1},
                                            {0.30f, 0.32f, 0.36f, 1}}));
    const SkColor4f figure = sketch::kit::theme().palette.figure;
    run.children({each(kTiles * 3, [&](int i) {
      return motif(i % 3 == 0 ? kWarm : figure);
    })});
    // …and re-recorded behind a bounding-box hierarchy, so each tile's
    // replay visits only the ops that meet it. Slicing without that is
    // quadratic: every tile would walk every tile's ops.
    strip = tiles::sliceable(
        snapshot(box().children({std::move(run)}), *ctx.fonts));

    ctx.composer.render(sketch::kit::page(
        {.title = "Copies and windows",
         .subtitle = "A repeated motif is a set of instances. A tiled strip is "
                     "one picture viewed in pieces.",
         .footer = "Translation and rotation advance linearly; repeated scale "
                   "multiplies. Mirroring belongs to the tile consumer."},
        box().column().gap(24).children(
            {sketch::kit::sectionHeader(
                 {.label = "01  COPY A MOTIF",
                  .note = "A pool stores a transform and opacity for each "
                          "instance"}),
             sketch::kit::comparison(
                 {.cases = {chain("TRANSLATE"), turned("ROTATE AND SHRINK"),
                            ramped("FADE")},
                  .measure = 1020,
                  .gap = 18}),
             box()
                 .row()
                 .alignItems(Align::Start)
                 .gap(18)
                 .children(
                     {box().column().gap(18).children(
                          {sketch::kit::sectionHeader(
                               {.label = "02  SLICE ONE PICTURE", .note = ""}),
                           sketch::kit::comparison(
                               {.cases = {sliced("FORWARD", false),
                                          sliced("MIRRORED", true)},
                                .measure = 674,
                                .gap = 18})}),
                      box().column().gap(18).children(
                          {sketch::kit::sectionHeader(
                               {.label = "TWO KINDS OF REPETITION",
                                .note = ""}),
                           document::caption(
                               "The upper row repeats geometry. Each star has "
                               "a position, a rotation, a scale and an "
                               "opacity.")
                               .width(328),
                           document::caption(
                               "The lower row repeats a window over one "
                               "recorded picture. Facing describes how the "
                               "consumer will read that window.")
                               .width(328)})})})));
  }

  Element pooled(const std::shared_ptr<instancing::Pool>& pool) const {
    return box().cover().children(
        {instancing::instances(atlas, pool, instancing::Mode::Data)});
  }

  sketch::kit::ComparisonCase chain(const char* caseTitle) const {
    return {.title = caseTitle,
            .control = "repeat · 9 copies · step = 26",
            .figure = sketch::kit::cell(kSpecimen, "", "", pooled(plain)),
            .note =
                "the plainest chain · position is start + translate "
                "× i, and every other lane is left alone"};
  }

  sketch::kit::ComparisonCase turned(const char* caseTitle) const {
    return {.title = caseTitle,
            .control =
                "…"
                ", rotateStep = 0.18, scaleStep = 0.90",
            .figure = sketch::kit::cell(kSpecimen, "", "", pooled(spun)),
            .note =
                "rotation LINEAR in the index, scale EXPONENTIAL · "
                "each copy is nine tenths of the one before it"};
  }

  sketch::kit::ComparisonCase ramped(const char* caseTitle) const {
    return {.title = caseTitle,
            .control =
                "…"
                ", opacityFrom = 1, opacityTo = 0.12",
            .figure = sketch::kit::cell(kSpecimen, "", "", pooled(faded)),
            .note =
                "the ramp writes the alphas() lane, composing with the "
                "authored tint · written only when the two arguments "
                "say something"};
  }

  /** The strip, cut into `kTiles` rasters and laid out with air between
   *  them, so the reader sees separate tiles rather than one picture. */
  sketch::kit::ComparisonCase sliced(const char* caseTitle,
                                     bool mirrored) const {
    sk_sp<SkPicture> art = strip;
    const auto facing =
        mirrored ? tiles::Facing::Mirrored : tiles::Facing::Forward;
    return {
        .title = caseTitle,
        .control = mirrored ? "tiles::window(tile, k, Down, Mirrored)"
                            : "tiles::window(tile, k, Flow::Down)",
        .figure = sketch::kit::cell(
            kSpecimen, "", "",
            custom(mirrored ? "tiles.mirrored" : "tiles.forward",
                   [art, facing](SkCanvas& canvas) {
                     constexpr float kAir = 4;
                     const float scale = 1.1f;
                     canvas.save();
                     canvas.translate((kCell - kTiles * kTile.width() * scale -
                                       (kTiles - 1) * kAir) /
                                          2,
                                      30);
                     canvas.scale(scale, scale);
                     for (int k = 0; k < kTiles; ++k) {
                       canvas.save();
                       canvas.translate(
                           k * ((float)kTile.width() + kAir / scale), 0);
                       canvas.clipRect(SkRect::MakeWH((float)kTile.width(),
                                                      (float)kTile.height()));
                       canvas.concat(
                           tiles::window(kTile, k, tiles::Flow::Down, facing));
                       canvas.drawPicture(art);
                       canvas.restore();
                     }
                     canvas.restore();
                   })
                .cover()),
        .note = mirrored ? "pre-flipped ACROSS the strip for a consumer "
                           "whose u runs backwards · legible in a PNG "
                           "either way, which is the trap"
                         : "four tiles of one baked picture, drawn apart "
                           "· sliceable() first, so each replay "
                           "visits only its own ops"};
  }
};

SIGIL_SKETCH(PlaceRepeatTiles, "Kit · API",
             "one motif as a copy chain in an instance pool under three "
             "parameterisations, and one long picture sliced into tiles "
             "forward and mirrored")
