/** @file
 * pop_billboards — the sink that forms no geometry, and the operator
 * that heals a cloud before one does.
 *
 * `cookBillboards` is the one point-operator sink with no Mesh in it: a
 * billboard faces the EYE, so it is answered where the eye is rather
 * than in the world. It cooks the chain, projects every point, sorts
 * back to front and splats a sprite. The "size" and "tint" lanes a cook
 * exports are picked up without the caller naming them, so a chain that
 * varied either shows it.
 *
 * `Relax` is the other half of the sheet. It eases each point toward
 * the midpoint of its CHAIN-ORDER neighbours, which is what heals the
 * kinks `Noise` leaves before a swept sink threads a frame through
 * them. It is double-buffered, so chain order cannot leak into the
 * result — and that is also why it has no per-point kernel and a device
 * executor declines it: a point reads two it does not own.
 *
 * One sprite serves a whole splat. The "Tex" lane `pop::AtlasCell` writes
 * rides on the cooked cloud for the STAMPING sink, which builds real
 * geometry with real uvs; this sink has a single image and a tint.
 *
 * EDIT THESE FIRST
 *   kMotes      — points in each cloud.
 *   kNoise      — the displacement Relax is asked to heal, px.
 *   kIterations — the strongest smoothing on the sheet.
 */

// TAGS: Geometry/Points, Motion/Particles

#include <include/core/SkCanvas.h>
#include <include/core/SkSurface.h>
#include <sigilcompose/core/Core.h>
#include <sigilcompose/kit/Document.h>
#include <sigilcompose/kit/Specimen.h>
#include <sigilgeometry/kit/Solids.h>
#include <sigilgeometry/mesh/camera/Camera.h>
#include <sigilgeometry/mesh/pop/Points.h>
#include <sigilgeometry/mesh/pop/Pop.h>
#include <sigilsketch/canvas/Sketch.h>
#include <sigilsketch/kit/Kit.h>

#include <cmath>
#include <functional>
#include <string>
#include <vector>

namespace sketch = sigil::sketch;
namespace gm = sigil::geometry::mesh;
namespace camera = sigil::geometry::mesh::camera;
namespace points = sigil::geometry::mesh::points;

using namespace sigil::compose;
namespace pop = sigil::geometry::mesh::pop;

namespace {

constexpr SkSize kCanvas = {1100, 900};
constexpr float kCell = 328;
constexpr float kPicture = 216;

constexpr int kMotes = 5200;     // points in each cloud
constexpr float kNoise = 20;     // the displacement Relax has to heal
constexpr int kIterations = 12;  // the strongest smoothing on the sheet

/** The specimen sheet, in this one's own look. */
sketch::kit::Theme sheetTheme() {
  sketch::kit::Theme look = sketch::kit::studyTheme();
  look.palette.cellGround = {0.09f, 0.095f, 0.11f, 1};
  look.type.captionLabel = {.size = 12, .track = 1.2f};
  look.spacing.captionGap = 8;
  return look;
}

camera::Camera stage() {
  camera::Camera view;
  view.eye = {0, 74, 235};
  view.target = {0, 0, 0};
  view.fovYDeg = 40;
  return view;
}

/** A hollow ring sprite, baked once — a shape whose EDGE is visible, so
 *  a splat's projected size and the sink's sorting are both legible
 *  where the default soft dot would just be a haze. */
sk_sp<SkImage> ringSprite() {
  return [] {
    constexpr int kSide = 64;
    sk_sp<SkSurface> surface =
        SkSurfaces::Raster(SkImageInfo::MakeN32Premul(kSide, kSide));
    SkCanvas* canvas = surface->getCanvas();
    canvas->clear(SK_ColorTRANSPARENT);
    SkPaint ring;
    ring.setAntiAlias(true);
    ring.setStyle(SkPaint::kStroke_Style);
    ring.setStrokeWidth(7);
    ring.setColor(SK_ColorWHITE);
    canvas->drawCircle(kSide * 0.5f, kSide * 0.5f, kSide * 0.34f, ring);
    return surface->makeImageSnapshot();
  }();
}

/** The subject cloud: points scattered on a torus, sized and tinted by
 *  their own lanes so the sink's automatic lane pickup has something to
 *  pick up. */
pop::Builder motes() {
  return pop::on(gm::torus(74, 26, 64, 32), kMotes)
      .vary(0.7f, 1.0f)
      .rampBy({{0.30f, 0.62f, 0.98f, 1}, {1.00f, 0.62f, 0.28f, 1}});
}

/** How the second row splats: opaque, small and depth-sorted, so what
 *  the eye reads is where the points ARE rather than how many of them
 *  piled up on one pixel. */
const points::BillboardStyle kStrand{.size = 3.2f, .additive = false};

/** The other subject: a ring of points pushed off their loop by Noise,
 *  which is the kink Relax exists to take out. */
pop::Builder kinked() {
  std::vector<glm::vec3> loop;
  for (int i = 0; i < 96; ++i) {
    const float a = 6.2831853f * (float)i / 96.0f;
    loop.push_back({64 * std::cos(a), 24 * std::sin(2 * a), 64 * std::sin(a)});
  }
  return pop::on(std::move(loop))
      .count(1400)
      .seed(4)
      .noise(kNoise, 0.075f)
      .rampBy({{0.98f, 0.84f, 0.42f, 1}, {0.42f, 0.86f, 0.72f, 1}});
}

/** One splat of @p chain in @p style, into the cell's own box: the sink
 *  forms no geometry, so a cell is the chain, the style and nothing. */
std::function<void(SkCanvas&, SkSize)> splat(pop::Builder chain,
                                             points::BillboardStyle style) {
  return [chain = std::move(chain), style = std::move(style)](SkCanvas& canvas,
                                                              SkSize size) {
    chain.billboards(canvas, stage(), size, style);
  };
}

Element figure(const char* key, std::function<void(SkCanvas&, SkSize)> draw) {
  return sketch::kit::well(
      {.width = kCell, .height = kPicture},
      custom(key, [draw = std::move(draw)](SkCanvas& canvas,
                                           const PaintContext& pc) {
        draw(canvas, pc.size);
      }));
}

}  // namespace

struct PopBillboards {
  void setup(sketch::SketchContext& ctx) {
    const sketch::kit::Provide look(sheetTheme());
    // nothing moves; the sheet is complete at once
    sketch::kit::stage(ctx, {.size = kCanvas, .captureAt = 0.05});

    // The atlas cell each point drew, read back off the cooked cloud:
    // the lane is written here and consumed by the stamping sink.
    const gm::Cloud tagged = motes().atlas(4, 4).cloud();
    const std::vector<glm::vec4>* tex = tagged.colorIf("Tex");

    const auto texCell = [tex](int axis) {
      return tex && !tex->empty()
                 ? (double)((axis == 0) ? (*tex)[0].x : (*tex)[0].y)
                 : 0.0;
    };
    ctx.composer.render(sketch::kit::page(
        {.title = "The mark and the path",
         .subtitle = "First change how points are drawn; then change where "
                     "they lie · fixed camera within each comparison",
         .footer = kit::formatted(
             "Billboards project and paint one sprite per point. Atlas cells "
             "belong to the stamping sink; point 0 carries Tex (%.2f, %.2f).",
             texCell(0), texCell(1))},
        box().column().gap(22).children(
            {document::h2(
                 "01 / ONE TORUS, THREE DRAWING STYLES · 5,200 POINTS"),
             sketch::kit::comparison(
                 {.cases = {{.title = "SOFT LIGHT",
                             .control = "soft dot · additive · size 2.6",
                             .figure =
                                 figure("soft", splat(motes(), {.size = 2.6f})),
                             .note = "Overlapping light builds a glow. Point "
                                     "lanes supply size and tint."},
                            {.title = "HOLLOW RINGS",
                             .control = "ring sprite · opaque · size 7",
                             .figure =
                                 figure("rings",
                                        splat(motes(), {.sprite = ringSprite(),
                                                        .size = 7,
                                                        .additive = false})),
                             .note = "A crisp rim reveals the projected size "
                                     "of each mark."},
                            {.title = "CONSTANT PIXELS",
                             .control = "perspective = false · size 4",
                             .figure = figure(
                                 "pixels", splat(motes(), {.size = 4,
                                                           .additive = false,
                                                           .perspective =
                                                               false})),
                             .note = "Near and far points share a pixel size; "
                                     "depth order still separates them."}},
                  .measure = 1020,
                  .gap = 18}),
             document::h2(
                 "02 / ONE NOISED LOOP, THREE RELAXATION AMOUNTS · 1,400 "
                 "POINTS"),
             sketch::kit::comparison(
                 {.cases =
                      {{.title = "UNTOUCHED",
                        .control = "0 passes · noise amplitude 20",
                        .figure = figure("noise", splat(kinked(), kStrand)),
                        .note = "Consecutive points jump across the displaced "
                                "loop."},
                       {.title = "GENTLE RELAXATION",
                        .control = "3 passes · strength 0.5",
                        .figure = figure(
                            "gentle", splat(kinked().smooth(0.5f, 3), kStrand)),
                        .note = "Each point moves toward its two chain-order "
                                "neighbours."},
                       {.title = "CONTINUOUS STRAND",
                        .control = "12 passes · strength 0.9",
                        .figure = figure(
                            "smooth",
                            splat(kinked().smooth(0.9f, kIterations), kStrand)),
                        .note = "The fine kinks soften while the broad "
                                "displacement remains."}},
                  .measure = 1020,
                  .gap = 18})})));
  }
};

SIGIL_SKETCH(PopBillboards, "Kit · API",
             "the splatting sink that forms no geometry, its style dials, "
             "and Relax healing a noised loop back into one a sweep could "
             "follow")
