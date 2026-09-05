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

#include <include/core/SkCanvas.h>
#include <include/core/SkPicture.h>
#include <sigilcompose/core/Core.h>
#include <sigilcompose/core/Instances.h>
#include <sigilcompose/core/Tiles.h>
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
using sigil::compose::toU8;

namespace {

constexpr SkSize kCanvas = {1100, 470};
constexpr float kCell = 200;
constexpr float kPicture = 200;

constexpr size_t kCopies = 9;  // copies in each chain
constexpr float kStep = 19;    // the per-copy translate, px
constexpr int kTiles = 4;      // slices the strip is cut into
constexpr SkSize kMotif = {34, 34};
constexpr SkISize kTile = {44, 128};

constexpr SkColor4f kFigure{0.90f, 0.83f, 0.68f, 1};
constexpr SkColor4f kWarm{0.86f, 0.52f, 0.34f, 1};

/** The one motif every chain repeats and the strip is built from. */
Element motif() {
  return box()
      .width(Dim(kMotif.width()))
      .height(Dim(kMotif.height()))
      .shape(shapes::star(6, 0.46f, 0.14f))
      .fill(Fill::color(kFigure));
}

Element cell(const char* call, const char* note, Element body) {
  return sketch::kit::caption(
      kCell, toU8(call), toU8(note),
      sketch::kit::well({.width = kCell, .height = kPicture})
          .child(std::move(body)));
}

}  // namespace

struct PlaceRepeatTiles final : sketch::Sketch {
  std::shared_ptr<instancing::Atlas> atlas;
  std::shared_ptr<instancing::Pool> plain, spun, faded;
  sk_sp<SkPicture> strip;

  void setup(sketch::SketchContext& ctx) override {
    // nothing moves; the sheet is complete at once
    sketch::kit::stage(ctx, {.size = kCanvas, .captureAt = 0.05});

    atlas = std::make_shared<instancing::Atlas>();
    atlas->cell(motif(), kMotif);

    plain = std::make_shared<instancing::Pool>();
    instancing::place::repeat(*plain, kCopies, {28, 100}, {kStep, 0});

    spun = std::make_shared<instancing::Pool>();
    instancing::place::repeat(*spun, kCopies, {34, 60}, {kStep, 9}, 0.18f,
                              0.90f);

    faded = std::make_shared<instancing::Pool>();
    instancing::place::repeat(*faded, kCopies, {28, 100}, {kStep, 0}, 0, 1.0f,
                              1.0f, 0.12f);

    // THE STRIP: one tree, taller than any tile, baked once. Authored as a
    // column because the tiles are tall — a transpose on the way out has
    // determinant -1 and composes with whatever mirroring the consumer
    // already applies.
    Element run = box()
                      .column()
                      .gap(8)
                      .padding(5)
                      .width(Dim(kTile.width()))
                      // Dark on one side and light on the other, so a
                      // mirrored tile is legible AS mirrored.
                      .fill(linearGradient({0, 0}, {(float)kTile.width(), 0},
                                           {{0.09f, 0.10f, 0.12f, 1},
                                            {0.30f, 0.32f, 0.36f, 1}}));
    for (int i = 0; i < kTiles * 3; ++i)
      run.child(box()
                    .width(Dim(kMotif.width()))
                    .height(Dim(kMotif.height()))
                    .shape(shapes::star(6, 0.46f, 0.14f))
                    .fill(Fill::color(i % 3 == 0 ? kWarm : kFigure)));
    // …and re-recorded behind a bounding-box hierarchy, so each tile's
    // replay visits only the ops that meet it. Slicing without that is
    // quadratic: every tile would walk every tile's ops.
    strip = tiles::sliceable(snapshot(box().child(std::move(run)), *ctx.fonts));

    ctx.composer.render(sketch::kit::page(
        {.title = toU8("REPEAT AND TILE \xc2\xb7 instancing::place::"
                       "repeat, tiles::window / tiles::sliceable"),
         .subtitle =
             toU8("dials \xc2\xb7 the copy count (9) \xc2\xb7 the "
                  "per-copy translate (19 px), rotation and scale step "
                  "\xc2\xb7 the opacity ramp \xc2\xb7 the tile count (4) "
                  "and its facing"),
         .footer = toU8("a chain's scale step is EXPONENTIAL and its "
                        "translate linear, and a tile is a clip and a "
                        "translate \xe2\x80\x94 there is no windowed "
                        "bake and no need for one, because neighbouring "
                        "tiles share their boundary texels")},
        kit::cells({.cells = {chain(), turned(), ramped(), sliced(false),
                              sliced(true)},
                    .gap = 12})));
  }

  Element pooled(const std::shared_ptr<instancing::Pool>& pool) const {
    return box().absolute().inset(0).child(
        instancing::instances(atlas, pool, instancing::Mode::Data));
  }

  Element chain() const {
    return cell("place::repeat(pool, 9, start, {19, 0})",
                "the plainest chain \xc2\xb7 position is start + translate "
                "\xc3\x97 i, and every other lane is left alone",
                pooled(plain));
  }

  Element turned() const {
    return cell(
        "\xe2\x80\xa6"
        ", rotateStep = 0.18, scaleStep = 0.90",
        "rotation LINEAR in the index, scale EXPONENTIAL \xc2\xb7 "
        "each copy is nine tenths of the one before it",
        pooled(spun));
  }

  Element ramped() const {
    return cell(
        "\xe2\x80\xa6"
        ", opacityFrom = 1, opacityTo = 0.12",
        "the ramp writes the alphas() lane, composing with the "
        "authored tint \xc2\xb7 written only when the two arguments "
        "say something",
        pooled(faded));
  }

  /** The strip, cut into `kTiles` rasters and laid out with air between
   *  them, so the reader sees separate tiles rather than one picture. */
  Element sliced(bool mirrored) const {
    sk_sp<SkPicture> art = strip;
    const auto facing =
        mirrored ? tiles::Facing::Mirrored : tiles::Facing::Forward;
    return cell(mirrored ? "tiles::window(tile, k, Down, Mirrored)"
                         : "tiles::window(tile, k, Flow::Down)",
                mirrored ? "pre-flipped ACROSS the strip for a consumer "
                           "whose u runs backwards \xc2\xb7 legible in a PNG "
                           "either way, which is the trap"
                         : "four tiles of one baked picture, drawn apart "
                           "\xc2\xb7 sliceable() first, so each replay "
                           "visits only its own ops",
                custom(mirrored ? "tiles.mirrored" : "tiles.forward",
                       [art, facing](SkCanvas& canvas, const PaintContext&) {
                         constexpr float kAir = 4;
                         const float scale = 0.62f;
                         canvas.save();
                         canvas.translate(10, 8);
                         canvas.scale(scale, scale);
                         for (int k = 0; k < kTiles; ++k) {
                           canvas.save();
                           canvas.translate(
                               k * ((float)kTile.width() + kAir / scale), 0);
                           canvas.clipRect(SkRect::MakeWH(
                               (float)kTile.width(), (float)kTile.height()));
                           canvas.concat(tiles::window(
                               kTile, k, tiles::Flow::Down, facing));
                           canvas.drawPicture(art);
                           canvas.restore();
                         }
                         canvas.restore();
                       })
                    .absolute()
                    .inset(0));
  }
};

SIGIL_SKETCH(PlaceRepeatTiles, "Kit \xc2\xb7 API",
             "one motif as a copy chain in an instance pool under three "
             "parameterisations, and one long picture sliced into tiles "
             "forward and mirrored")
