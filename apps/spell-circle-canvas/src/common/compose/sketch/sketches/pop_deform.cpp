// pop_deform.cpp — SELECTORS, MASKS AND DEFORMERS in geometry::pop.
// =============================================================================
// Links geometry:: like pop_lanes.cpp: every cloud is cooked once by the CPU
// reference executor in setup() and splatted by points::drawBillboards.
// SigilWorld cooks the identical chains on the GPU; this sketch is the
// Skia-painter view of the same language.
//
// One column of points, six ways:
//   1. select()      — a Group writes a mask lane from a region; the
//                       colour ramp reads that lane, so you SEE the mask.
//   2. .masked()     — the same Math on every point, taken only as far as
//                       the mask says; the feather is a graded band.
//   3. twist()       — rotate about the axis, more the higher you go.
//   4. taper()       — scale toward the axis, more the higher you go.
//   5. bend()        — the band becomes an arc; past the band, rigid.
//   6. peak() + orient() — push along Dir, then re-aim Dir by a matrix.
//
// EDIT THESE FIRST
//   kFeather   — the selector's soft edge (0 = hard 0/1 selection).
//   kTwistDeg / kTaper / kBendDeg — the deformer amounts.
//   kMaskDeformers — true routes every deformer through the mask too, so
//                    only the selected band twists, tapers and bends.

#include <include/core/SkCanvas.h>
#include <include/core/SkSurface.h>
#include <sigilgeometry/Points.h>
#include <sigilgeometry/Pop.h>
#include <sigilgeometry/Space.h>
#include <sigilsketch/Sketch.h>

#include <cmath>
#include <vector>

using namespace sigil::compose;
using namespace sigil::compose::util;
namespace geometry = sigil::geometry;

namespace {

constexpr int kCount = 1400;
constexpr float kFeather = 0.35f;
constexpr float kTwistDeg = 150.0f;
constexpr float kTaper = 0.25f;
constexpr float kBendDeg = 80.0f;
constexpr bool kMaskDeformers = false;
constexpr float kPanel = 180.0f;
constexpr float kHeight = 300.0f;  // the column: y in [-150, 150]

sigil::weave::TextStyle type(float size, SkColor4f color) {
  sigil::weave::TextStyle style;
  style.shaping.fontSize = size;
  style.paint.foreground.setColor4f(color, nullptr);
  style.paint.foreground.setAntiAlias(true);
  return style;
}

const SkColor4f kInk{0.90f, 0.93f, 0.97f, 1};
const SkColor4f kDim{0.55f, 0.60f, 0.70f, 1};
const SkColor4f kFrame{0.24f, 0.28f, 0.36f, 1};

/** A thin vertical loop: points scatter along it with a radial spread,
 *  so the cloud is a fuzzy column standing on the y axis. */
std::vector<glm::vec3> column() {
  return {{0, -kHeight / 2, 0},
          {0, kHeight / 2, 0},
          {0, kHeight / 2, 1},
          {0, -kHeight / 2, 1}};
}

geometry::space::Camera lookAtColumn() {
  geometry::space::Camera camera;
  camera.eye = {260, 120, 720};
  camera.target = {0, 0, 0};
  camera.fovYDeg = 30;
  return camera;
}

const sk_sp<SkImage>& disc() {
  static const sk_sp<SkImage> img = [] {
    sk_sp<SkSurface> s = SkSurfaces::Raster(SkImageInfo::MakeN32Premul(32, 32));
    SkCanvas* c = s->getCanvas();
    c->clear(SK_ColorTRANSPARENT);
    SkPaint p;
    p.setAntiAlias(true);
    p.setColor(SK_ColorWHITE);
    c->drawCircle(16, 16, 14, p);
    return s->makeImageSnapshot();
  }();
  return img;
}

Element splat(geometry::Cloud cloud) {
  return custom([cloud = std::move(cloud)](SkCanvas& canvas,
                                           const PaintContext& paint) {
           geometry::points::BillboardStyle style;
           style.sprite = disc();
           style.size = 7;
           style.sizeLane = "size";
           style.tintLane = "tint";
           style.additive = false;
           style.depthSort = true;
           geometry::points::drawBillboards(canvas, cloud, lookAtColumn(),
                                         paint.size, style);
         })
      .inset(0)
      .cache(Cache::None);
}

Element panel(const char* title, const char* note, Element inner) {
  return box()
      .width(kPanel)
      .column()
      .gap(5)
      .child(text(toU8(title), type(12, kInk)))
      .child(box()
                 .width(kPanel)
                 .height(kPanel * 1.6f)
                 .clip()
                 .stroke(stroke(1.0f, Fill::color(kFrame)))
                 .child(std::move(inner)))
      .child(text(toU8(note), type(10.5f, kDim)));
}

/** The shared head of every chain: the column, spread, sized, and a
 *  band across its middle selected into "band" — feathered so the ramp
 *  shows a gradient at the edges. Colour then reads the mask: cool
 *  outside, hot inside. */
geometry::pop::Builder base() {
  const std::vector<glm::vec4> stops = {{0.16f, 0.22f, 0.45f, 1},
                                        {0.85f, 0.35f, 0.30f, 1},
                                        {1.00f, 0.85f, 0.35f, 1}};
  return geometry::pop::on(column())
      .count(kCount)
      .window(0.5f, 0.5f)
      .spread(28)
      .seed(3)
      .vary(0.5f)
      .select("band", geometry::pop::Select::Shape::Box, {0, 20, 0}, {80, 55, 80},
              kFeather)
      .rampBy("band", 0, stops);
}

}  // namespace

struct PopDeform : sigil::compose::sketch::Sketch {
  geometry::Cloud selected, masked, twisted, tapered, bent, peaked;

  void setup(sketch::SketchContext& ctx) override {
    ctx.canvas(1240, 420);
    ctx.background({0.055f, 0.06f, 0.085f, 1});

    const char* mask = kMaskDeformers ? "band" : "";
    const auto maskIf = [&](geometry::pop::Builder b) {
      return kMaskDeformers ? b.masked(mask) : b;
    };

    selected = base().cloud();
    // The mask at work: everyone gets the same Math, taken by "band".
    masked = base().move({90, 0, 0}).masked("band").cloud();
    // Twist about an axis standing beside the column, so a symmetric
    // column has something to show: it becomes a helix.
    twisted = maskIf(base().twist(kTwistDeg, {0, 1, 0}, -kHeight / 2,
                                  kHeight / 2, {-55, 0, 0}))
                  .cloud();
    tapered = maskIf(base().taper(kTaper, {0, 1, 0}, -kHeight / 2, kHeight / 2))
                  .cloud();
    bent = maskIf(base().bend(kBendDeg, {0, 1, 0}, {1, 0, 0}, -kHeight / 4,
                              kHeight / 2))
               .cloud();
    // Peak pushes along Dir — the loop tangent, i.e. straight up the
    // column — masked to the band, after orient() has tipped Dir over
    // by 60 degrees so the band leans out.
    peaked = base()
                 .orient(geometry::space::place({}, 0, 0, 60))
                 .peak(70)
                 .masked("band")
                 .cloud();

    ctx.composer.render(
        stack()
            .child(text(toU8("geometry::pop \xc2\xb7 select() writes a mask, "
                             ".masked() takes it, and twist / taper / bend / "
                             "peak deform the same column"),
                        type(15, kInk))
                       .left(30)
                       .top(16))
            .child(
                box()
                    .row()
                    .left(30)
                    .top(48)
                    .gap(14)
                    .child(panel("select(\"band\", Box, feather)",
                                 "colour reads the mask lane", splat(selected)))
                    .child(panel("move().masked(\"band\")",
                                 "one Math, taken by the mask", splat(masked)))
                    .child(panel("twist(150, +Y, origin -55x)",
                                 "a helix: more turn with height",
                                 splat(twisted)))
                    .child(panel("taper(0.25)", "toward the axis at the top",
                                 splat(tapered)))
                    .child(panel("bend(80)", "the band arcs; past it, rigid",
                                 splat(bent)))
                    .child(panel("orient() . peak(70).masked()",
                                 "push along a re-aimed Dir", splat(peaked))))
            .child(text(toU8("every chain here cooks identically on "
                             "SigilWorld's GPU executor \xe2\x80\x94 the "
                             "mask is one more lane in the arena"),
                        type(11, kDim))
                       .left(30)
                       .bottom(14)));
  }
};

SIGIL_SKETCH(PopDeform)
