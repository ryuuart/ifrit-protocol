// pop_lanes.cpp — TWO POP OPERATORS: Builder::rampBy() (Lookup) and
// Builder::order() (Sort), from SigilGeometry.
// =============================================================================
// Links geometry:: like shapeworks_lab.cpp: the clouds are cooked by the CPU
// reference executor in setup() and splatted by points::drawBillboards, so
// this file is the adapter and nothing else.
//
// rampBy — `fade()` grown up. Pick the SOURCE lane, pick which component
// reads, state the range it spans, hand over as many stops as the curve
// needs. Panel 1 is the loud default (a gradient down T); panel 2 drives
// colour from P.y over a stated range, i.e. "colour by height".
//
// order — a PERMUTATION, and the reason it exists is right here: the Skia
// point sink has no depth buffer, so CHAIN ORDER *is* painter order. Panels
// 3 and 4 are the same points with the same colours; the only
// difference is one `.order()` call, and panel 3 is visibly wrong.
//
// EDIT THESE FIRST
//   kOrderAxis / kDescending — panel 4's sort. The camera sits on +z, so
//              farthest-first is ASCENDING z. Flip kDescending to true and
//              panel 4 becomes the exact opposite mistake to panel 3's.
//   kCount   — points per cloud. Fewer points, less overlap, and the
//              ordering stops mattering: overlap is what makes order real.
//   kRange   — panel 2's `low`/`high` for the height ramp. Narrow it and
//              the table clips at both ends.
//
// The three ways things move (hello.cpp): none of them, deliberately. The
// clouds are cooked ONCE; the four leaves are immediate-mode `custom()`
// programs only because a projection is cheaper to redo than to cache.

#include <include/core/SkCanvas.h>
#include <include/core/SkSurface.h>
#include <sigilgeometry/Points.h>
#include <sigilgeometry/Pop.h>
#include <sigilgeometry/Space.h>
#include <sigilsketch/Sketch.h>

#include <cmath>
#include <vector>

using namespace sigil::compose;
namespace geometry = sigil::geometry;

namespace {

constexpr int kCount = 520;       // panels 1 and 2
constexpr int kOrderCount = 240;  // panels 3 and 4: fewer, bigger, overlapping
const glm::vec3 kOrderAxis{0, 0, 1};  // the sort key: dot(P, axis)
constexpr bool kDescending = false;   // false = ascending = farthest first
constexpr float kRange = 210.0f;      // panel 2's height ramp, +-kRange
constexpr float kPanel = 274.0f;

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

/** A CROWN: a closed ring in the XZ plane with a threefold vertical wave.
 *  Two properties are doing work here. The near and far arcs OVERLAP on
 *  screen, which is the only condition under which draw order is visible at
 *  all; and HEIGHT rises and falls three times per lap, so "colour by T"
 *  and "colour by height" are visibly different answers. */
std::vector<glm::vec3> crown(float radius, float rise, int knots) {
  std::vector<glm::vec3> loop;
  for (int i = 0; i < knots; ++i) {
    const float a = 2.0f * (float)M_PI * (float)i / (float)knots;
    loop.push_back({radius * std::cos(a), rise * std::sin(3.0f * a),
                    radius * std::sin(a)});
  }
  return loop;
}

geometry::space::Camera lookAtCrown() {
  geometry::space::Camera camera;
  camera.eye = {0, 150, 980};
  camera.target = {0, 0, 0};
  camera.fovYDeg = 34;
  return camera;
}

/** A HARD-EDGED sprite, and it is load-bearing: the stock soft dot's rim is
 *  semi-transparent, so a mis-ordered sprite reads as haze rather than as
 *  occlusion. The rim is BLACK because the tint is applied by kModulate —
 *  0 * anything is 0, so a black outline survives every tint. */
const sk_sp<SkImage>& disc() {
  static const sk_sp<SkImage> img = [] {
    sk_sp<SkSurface> s = SkSurfaces::Raster(SkImageInfo::MakeN32Premul(64, 64));
    SkCanvas* c = s->getCanvas();
    c->clear(SK_ColorTRANSPARENT);
    SkPaint p;
    p.setAntiAlias(true);
    p.setColor(SK_ColorWHITE);
    c->drawCircle(32, 32, 30, p);
    p.setColor(SK_ColorBLACK);
    p.setStyle(SkPaint::kStroke_Style);
    p.setStrokeWidth(4);
    c->drawCircle(32, 32, 28, p);
    return s->makeImageSnapshot();
  }();
  return img;
}

/** The sink: NO depth sort and NO additive blending, because both of those
 *  hide what `order()` does. kSrcOver means the last sprite drawn wins, and
 *  chain order decides who is last. */
Element splat(geometry::Cloud cloud, float spriteSize) {
  return custom([cloud = std::move(cloud), spriteSize](
                    SkCanvas& canvas, const PaintContext& paint) {
           geometry::points::BillboardStyle style;
           style.sprite = disc();
           style.size = spriteSize;
           style.sizeLane = "size";
           style.tintLane = "tint";
           style.additive = false;   // kSrcOver: order decides the picture
           style.depthSort = false;  // the sink's own sort would mask it
           geometry::points::drawBillboards(canvas, cloud, lookAtCrown(),
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
      .child(text(toU8(title), type(13, kInk)))
      .child(box()
                 .width(kPanel)
                 .height(kPanel)
                 .clip()  // the projection is wider than the frame
                 .stroke(stroke(1.0f, Fill::color(kFrame)))
                 .child(std::move(inner)))
      .child(text(toU8(note), type(11, kDim)));
}

}  // namespace

struct PopLanes : sigil::compose::sketch::Sketch {
  geometry::Cloud downT, byHeight, unsorted, sorted;

  void setup(sketch::SketchContext& ctx) override {
    ctx.canvas(1200, 400);
    ctx.background({0.055f, 0.06f, 0.085f, 1});

    const std::vector<glm::vec3> loop = crown(215, 190, 72);

    // The FOUR-stop table both ramps use. A Lookup's stops are a CURVE
    // sampled with linear interpolation, not a palette of discrete cells.
    const std::vector<glm::vec4> stops = {{0.10f, 0.05f, 0.30f, 1},
                                          {0.85f, 0.20f, 0.45f, 1},
                                          {1.00f, 0.72f, 0.25f, 1},
                                          {0.85f, 1.00f, 0.95f, 1}};

    // 1 — the loud default: `rampBy(stops)` == rampBy(Lane::T, 0, stops).
    downT = geometry::pop::on(loop)
                .count(kCount)
                .spread(62)
                .seed(5)
                .vary(0.45f)
                .rampBy(stops)
                .cloud();

    // 2 — the full form: ANY lane, ANY component, ANY range. Component 1
    // of P is y, so this is "colour by height" over [-kRange, kRange].
    byHeight = geometry::pop::on(loop)
                   .count(kCount)
                   .spread(62)
                   .seed(5)
                   .vary(0.45f)
                   .rampBy(geometry::pop::Lane::P, 1, stops, -kRange, kRange)
                   .cloud();

    // 3 and 4 — identical but for `.order()`. Colour is driven from P.z,
    // so colour IS depth and a mis-ordered sprite is visible as a dark dot
    // sitting on top of a bright one.
    const auto depthChain = [&](const std::vector<glm::vec4>& table) {
      return geometry::pop::on(loop)
          .count(kOrderCount)
          .spread(62)
          .seed(5)
          .vary(0.45f)
          .rampBy(geometry::pop::Lane::P, 2, table, -230.0f, 230.0f);
    };
    const std::vector<glm::vec4> depthStops = {{0.06f, 0.08f, 0.16f, 1},
                                               {0.20f, 0.38f, 0.55f, 1},
                                               {0.95f, 0.98f, 1.00f, 1}};
    unsorted = depthChain(depthStops).cloud();
    sorted = depthChain(depthStops).order(kOrderAxis, kDescending).cloud();

    ctx.composer.render(
        stack()
            .child(text(toU8("geometry::pop \xc2\xb7 rampBy() drives one lane "
                             "from another; order() is a PERMUTATION, and "
                             "the point sink draws in it"),
                        type(15, kInk))
                       .left(30)
                       .top(16))
            .child(box()
                       .row()
                       .left(30)
                       .top(48)
                       .gap(16)
                       .child(panel("rampBy(stops)", "the default: T -> Color",
                                    splat(downT, 16)))
                       .child(panel("rampBy(P, 1, stops, -210, 210)",
                                    "colour by HEIGHT", splat(byHeight, 16)))
                       .child(panel("no order() \xc2\xb7 WRONG",
                                    "scatter order = painter order",
                                    splat(unsorted, 34)))
                       .child(panel("order({0,0,1}) \xc2\xb7 right",
                                    "farthest first, one call",
                                    splat(sorted, 34))))
            .child(text(toU8("Sort is CPU-only and stated as a boundary: a "
                             "permutation is not a per-point map, so "
                             "SigilWorld declines a chain holding one"),
                        type(11, kDim))
                       .left(30)
                       .bottom(14)));
  }
};

SIGIL_SKETCH(PopLanes)
