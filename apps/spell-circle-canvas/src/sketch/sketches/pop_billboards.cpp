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
 * One sprite serves a whole splat. The "Tex" lane `pop::Atlas` writes
 * rides on the cooked cloud for the STAMPING sink, which builds real
 * geometry with real uvs; this sink has a single image and a tint.
 *
 * EDIT THESE FIRST
 *   kMotes      — points in each cloud.
 *   kNoise      — the displacement Relax is asked to heal, px.
 *   kIterations — the strongest smoothing on the sheet.
 */

#include <include/core/SkCanvas.h>
#include <include/core/SkSurface.h>
#include <sigilcompose/core/Core.h>
#include <sigilcompose/kit/Specimen.h>
#include <sigilgeometry/kit/Solids.h>
#include <sigilgeometry/mesh/camera/Camera.h>
#include <sigilgeometry/mesh/pop/Points.h>
#include <sigilgeometry/mesh/pop/Pop.h>
#include <sigilsketch/canvas/Sketch.h>
#include <sigilweave/ports/SystemFontManager.h>
#include <sigilweave/style/Type.h>

#include <cmath>
#include <functional>
#include <string>
#include <vector>

namespace sketch = sigil::sketch;
namespace weave = sigil::weave;
namespace gm = sigil::geometry::mesh;
namespace camera = sigil::geometry::mesh::camera;
namespace points = sigil::geometry::mesh::points;

using namespace sigil::compose;
using sigil::compose::toU8;
namespace pop = sigil::geometry::mesh::pop;

namespace {

constexpr SkSize kCanvas = {1100, 748};
constexpr float kCell = 340;
constexpr float kPicture = 248;

constexpr int kMotes = 5200;     // points in each cloud
constexpr float kNoise = 20;     // the displacement Relax has to heal
constexpr int kIterations = 12;  // the strongest smoothing on the sheet

constexpr SkColor4f kGround{0.07f, 0.07f, 0.085f, 1};
constexpr SkColor4f kCellGround{0.09f, 0.095f, 0.11f, 1};
constexpr SkColor4f kInk{0.90f, 0.90f, 0.92f, 1};
constexpr SkColor4f kAsh{0.55f, 0.56f, 0.62f, 1};
constexpr SkColor4f kRule{0.20f, 0.21f, 0.25f, 1};

weave::TextStyle label(float size, SkColor4f color, float track = 0) {
  return weave::textStyle({.size = size, .color = color, .track = track});
}

weave::TextStyle mono(float size, SkColor4f color) {
  static const sk_sp<SkTypeface> face = weave::ports::pickTypeface(
      {"SF Mono", "Menlo", "DejaVu Sans Mono", "monospace"});
  return weave::textStyle({.face = face, .size = size, .color = color});
}

kit::Caption voice() {
  return {.where = kit::Caption::Where::Split,
          .label = label(12, kInk, 1.2f),
          .note = mono(10.5f, kAsh),
          .gap = 8,
          .noteMeasure = kCell};
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
  static const sk_sp<SkImage> image = [] {
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
  return image;
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
points::BillboardStyle strandStyle() {
  points::BillboardStyle style;
  style.size = 3.2f;
  style.additive = false;
  return style;
}

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

Element cell(const char* call, const std::string& note,
             std::function<void(SkCanvas&, SkSize)> draw) {
  return kit::cell(
      voice(), toU8(call), toU8(note),
      kit::well({.width = kCell,
                 .height = kPicture,
                 .ground = Fill::color(kCellGround)},
                custom(call, [draw = std::move(draw)](SkCanvas& canvas,
                                                      const PaintContext& pc) {
                  draw(canvas, pc.size);
                })));
}

}  // namespace

struct PopBillboards final : sketch::Sketch {
  void setup(sketch::SketchContext& ctx) override {
    ctx.canvas(kCanvas.width(), kCanvas.height());
    ctx.background(kGround);
    ctx.captureAt(0.05);  // nothing moves; the sheet is complete at once

    // The atlas cell each point drew, read back off the cooked cloud:
    // the lane is written here and consumed by the stamping sink.
    const gm::Cloud tagged = motes().atlas(4, 4).cloud();
    const std::vector<glm::vec4>* tex = tagged.colorIf("Tex");

    ctx.composer.render(
        kit::sheet(
            {.title = toU8("POP BILLBOARDS \xc2\xb7 cookBillboards + "
                           "BillboardStyle + Relax"),
             .subtitle = toU8("dials \xc2\xb7 the relax iterations (0, 3, "
                              "12) \xc2\xb7 the sprite \xc2\xb7 the atlas "
                              "cell (4 by 4)"),
             .footer = toU8("the splatting sink forms no geometry: it "
                            "projects, sorts back to front and draws one "
                            "sprite per point, which is why it is the sink "
                            "a camera-facing mark belongs to"),
             .titleStyle = label(14, kInk, 2.4f),
             .subtitleStyle = label(11.5f, kAsh, 0.8f),
             .footerStyle = label(11, kAsh, 0.4f),
             .marginX = 24,
             .marginTop = 20,
             .marginBottom = 16,
             .ground = Fill::color(kGround),
             .rule = Fill::color(kRule)},
            kit::cells(
                {.cells =
                     {kit::cells(
                          {.cells =
                               {cell("pop::Builder::billboards(canvas, "
                                     "camera, viewport)",
                                     kit::format(
                                         "%d points \xc2\xb7 size and tint "
                                         "lanes picked up unnamed \xc2\xb7 "
                                         "the default soft dot, additive",
                                         kMotes),
                                     [](SkCanvas& canvas, SkSize size) {
                                       points::BillboardStyle style;
                                       style.size = 2.6f;
                                       motes().billboards(canvas, stage(), size,
                                                          style);
                                     }),
                                cell("BillboardStyle{.sprite = ring}",
                                     kit::format(
                                         "one sprite for the whole splat "
                                         "\xc2\xb7 pop::Atlas wrote Tex "
                                         "cell (%.2f, %.2f) on point 0 for "
                                         "the STAMPING sink, which this "
                                         "one does not read",
                                         tex && !tex->empty()
                                             ? (double)(*tex)[0].x
                                             : 0.0,
                                         tex && !tex->empty()
                                             ? (double)(*tex)[0].y
                                             : 0.0),
                                     [](SkCanvas& canvas, SkSize size) {
                                       points::BillboardStyle style;
                                       style.sprite = ringSprite();
                                       style.size = 7;
                                       style.additive = false;
                                       motes().billboards(canvas, stage(), size,
                                                          style);
                                     }),
                                cell("BillboardStyle{.perspective = false}",
                                     "constant pixel size \xc2\xb7 near and "
                                     "far points splat the same, so the "
                                     "depth sort is the only thing left "
                                     "saying which is in front",
                                     [](SkCanvas& canvas, SkSize size) {
                                       points::BillboardStyle style;
                                       style.size = 4;
                                       style.perspective = false;
                                       style.additive = false;
                                       motes().billboards(canvas, stage(), size,
                                                          style);
                                     })},
                           .gap = 14}),
                      kit::cells(
                          {.cells =
                               {cell("no Relax",
                                     kit::format(
                                         "noise(%.0f, 0.075) straight off "
                                         "the loop scatter \xc2\xb7 "
                                         "consecutive points jump, so a "
                                         "frame threaded through them "
                                         "tears",
                                         (
                                             double)kNoise),
                                     [](SkCanvas& canvas, SkSize size) {
                                       kinked().billboards(canvas, stage(),
                                                           size, strandStyle());
                                     }),
                                cell("smooth(0.5, 3)",
                                     "Relax{.strength = 0.5, .iterations = "
                                     "3} \xc2\xb7 each point eases toward "
                                     "its chain-order neighbours' midpoint",
                                     [](SkCanvas& canvas, SkSize size) {
                                       kinked().smooth(0.5f, 3).billboards(
                                           canvas, stage(), size,
                                           strandStyle());
                                     }),
                                cell("smooth(0.9, 12)",
                                     kit::format(
                                         "strength 0.9 over %d passes "
                                         "\xc2\xb7 the run is continuous "
                                         "again \xe2\x80\x94 the "
                                         "amplitude survives, only the "
                                         "kinks go",
                                         kIterations),
                                     [](SkCanvas& canvas, SkSize size) {
                                       kinked()
                                           .smooth(0.9f, kIterations)
                                           .billboards(canvas, stage(), size,
                                                       strandStyle());
                                     })},
                           .gap = 14})},
                 .column = true,
                 .gap = 18}))
            .absolute()
            .inset(0));
  }
};

SIGIL_SKETCH(PopBillboards, "Kit \xc2\xb7 API",
             "the splatting sink that forms no geometry, its style dials, "
             "and Relax healing a noised loop back into one a sweep could "
             "follow")
