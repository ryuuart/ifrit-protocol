// pop_order.cpp — ONE OPERATOR: Builder::order(), a PERMUTATION.
// =============================================================================
// The Skia point sink has no depth buffer, so CHAIN ORDER *is* painter
// order: whichever point the chain hands over last is the sprite drawn
// last, and the last sprite drawn wins the pixel. `order()` is therefore
// not a decoration on the picture, it is the picture.
//
// Two panels, the same points, the same colours, the same seed. The only
// difference between them is one call. On the left the points arrive in
// scatter order, which is the order the generator happened to write
// them; on the right they arrive sorted along the camera's own axis,
// farthest first. Colour is driven from P.z, so colour IS depth here —
// which is what makes the left panel visibly wrong rather than merely
// different: a dark far sprite sits on top of a bright near one.
//
// Everything else about the two chains is the same value, built by one
// lambda, so there is nowhere for a second difference to hide.
//
// EDIT THESE FIRST
//   kOrderAxis / kDescending — the sort key, `dot(P, axis)`. The camera
//              sits on +z, so farthest-first is ASCENDING z. Flip
//              kDescending to true and the right panel becomes the exact
//              opposite mistake to the left one's.
//   kCount   — points per cloud. Fewer points, less overlap, and the
//              ordering stops mattering: overlap is what makes order
//              real.
//   kSpread  — how far off the ring the scatter throws each point, which
//              is the other half of how much the near and far arcs
//              overlap on screen.
//
// The three ways things move: none of them, deliberately. Both clouds are
// cooked ONCE; the two leaves are immediate-mode `custom()` programs only
// because a projection is cheaper to redo than to cache.

#include <include/core/SkCanvas.h>
#include <include/core/SkSurface.h>
#include <sigilcompose/core/Core.h>
#include <sigilcompose/kit/Specimen.h>
#include <sigilgeometry/mesh/camera/Camera.h>
#include <sigilgeometry/mesh/pop/Points.h>
#include <sigilgeometry/mesh/pop/Pop.h>
#include <sigilsketch/canvas/Sketch.h>
#include <sigilsketch/kit/Kit.h>

#include <cmath>
#include <utility>
#include <vector>

namespace sketch = sigil::sketch;

using namespace sigil::compose;
namespace mesh = sigil::geometry::mesh;

namespace {

constexpr int kCount = 240;               // few, big and overlapping
constexpr float kSpread = 62.0f;          // how far off the ring they throw
constexpr glm::vec3 kOrderAxis{0, 0, 1};  // the sort key: dot(P, axis)
constexpr bool kDescending = false;       // false = ascending = farthest first
constexpr float kPanel = 340.0f;

/** The house sheet, in this one's own look. */
sketch::kit::Theme sheetTheme() {
  sketch::kit::Theme look = sketch::kit::houseTheme();
  look.palette.ground = {0.055f, 0.06f, 0.085f, 1};
  look.palette.ink = {0.90f, 0.93f, 0.97f, 1};
  look.palette.ash = {0.55f, 0.60f, 0.70f, 1};
  look.palette.rule = {0.19f, 0.20f, 0.26f, 1};
  look.type.title = {.size = 15, .track = 2};
  look.type.subtitle = {.size = 11, .track = 0.6f};
  look.type.footer = {.size = 10.5f, .track = 0.2f};
  look.type.captionLabel = {.size = 13, .track = 0.4f};
  look.type.captionNote = {.size = 11, .track = 0.2f};
  look.spacing.marginX = 30;
  look.spacing.marginTop = 22;
  look.spacing.captionGap = 5;
  return look;
}

const SkColor4f kInk{0.90f, 0.93f, 0.97f, 1};
const SkColor4f kDim{0.55f, 0.60f, 0.70f, 1};
const SkColor4f kGround{0.055f, 0.06f, 0.085f, 1};
const SkColor4f kRule{0.19f, 0.20f, 0.26f, 1};
const SkColor4f kFrame{0.24f, 0.28f, 0.36f, 1};

/** A CROWN: a closed ring in the XZ plane with a threefold vertical wave.
 *  One property is doing the work here — the near and far arcs OVERLAP on
 *  screen, which is the only condition under which draw order is visible
 *  at all. The wave is what keeps them from overlapping as one flat band. */
std::vector<glm::vec3> crown(float radius, float rise, int knots) {
  std::vector<glm::vec3> loop;
  for (int i = 0; i < knots; ++i) {
    const float a = 2.0f * (float)M_PI * (float)i / (float)knots;
    loop.emplace_back(radius * std::cos(a), rise * std::sin(3.0f * a),
                      radius * std::sin(a));
  }
  return loop;
}

mesh::camera::Camera lookAtCrown() {
  mesh::camera::Camera camera;
  camera.eye = {0, 150, 980};
  camera.target = {0, 0, 0};
  camera.fovYDeg = 34;
  return camera;
}

/** A HARD-EDGED sprite, and it is load-bearing: a soft dot's rim is
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
Element splat(mesh::Cloud cloud, float spriteSize) {
  return custom([cloud = std::move(cloud), spriteSize](
                    SkCanvas& canvas, const PaintContext& paint) {
           mesh::points::BillboardStyle style;
           style.sprite = disc();
           style.size = spriteSize;
           style.sizeLane = "size";
           style.tintLane = "tint";
           style.additive = false;   // kSrcOver: order decides the picture
           style.depthSort = false;  // the sink's own sort would mask it
           mesh::points::drawBillboards(canvas, cloud, lookAtCrown(),
                                        paint.size, style);
         })
      .inset(0)
      .cache(Cache::None);
}

Element panel(const char* title, const char* note, Element inner) {
  return sketch::kit::caption(
      kPanel, toU8(title), toU8(note),
      box()
          .width(kPanel)
          .height(kPanel)
          .clip()  // the projection is wider than the frame
          .stroke(stroke(1.0f, Fill::color(kFrame)))
          .child(std::move(inner)));
}

}  // namespace

struct PopOrder : sketch::Sketch {
  mesh::Cloud unsorted, sorted;

  void setup(sketch::SketchContext& ctx) override {
    const sketch::kit::Provide look(sheetTheme());
    sketch::kit::stage(ctx, {.size = {760, 500}});
    // Both clouds are cooked in setup; nothing reads the clock.
    ctx.captureAt(0.05);

    const std::vector<glm::vec3> loop = crown(215, 190, 72);

    // Colour is driven from P.z over the ring's own depth range, so
    // colour IS depth and a mis-ordered sprite is visible as a dark dot
    // sitting on top of a bright one.
    const std::vector<glm::vec4> depthStops = {{0.06f, 0.08f, 0.16f, 1},
                                               {0.20f, 0.38f, 0.55f, 1},
                                               {0.95f, 0.98f, 1.00f, 1}};
    const auto depthChain = [&] {
      return mesh::pop::on(loop)
          .count(kCount)
          .spread(kSpread)
          .seed(5)
          .vary(0.45f)
          .rampBy(mesh::pop::Lane::P, 2, depthStops, -230.0f, 230.0f);
    };
    unsorted = depthChain().cloud();
    sorted = depthChain().order(kOrderAxis, kDescending).cloud();

    ctx.composer.render(sketch::kit::page(
        {.title = toU8("POP ORDER \xc2\xb7 order() is a PERMUTATION, "
                       "and the point sink draws in it"),
         .subtitle = toU8("colour is driven from P.z over the ring's own "
                          "depth range, so colour IS depth and a "
                          "mis-ordered sprite is a dark dot sitting on "
                          "a bright one"),
         .footer = toU8("Sort is CPU-only and stated as a boundary: a "
                        "permutation is not a per-point map, so "
                        "SigilWorld declines a chain holding one")},
        kit::cells(
            {.cells = {panel("no order() \xc2\xb7 WRONG",
                             "scatter order = painter order",
                             splat(unsorted, 34)),
                       panel("order({0,0,1}) \xc2\xb7 right",
                             "farthest first, one call", splat(sorted, 34))},
             .gap = 20})));
  }
};

SIGIL_SKETCH(PopOrder, "Kit \xc2\xb7 API",
             "geometry::pop order() \xe2\x80\x94 the same points twice, "
             "with and without one call, on a sink that has no depth "
             "buffer")
