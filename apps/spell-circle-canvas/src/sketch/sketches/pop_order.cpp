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

// TAGS: Geometry/Points

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
constexpr float kPanel = 498.0f;

/** The specimen sheet, in this one's own look. */
sketch::kit::Theme sheetTheme() {
  sketch::kit::Theme look = sketch::kit::studyTheme();
  look.palette.ground = {0.055f, 0.06f, 0.085f, 1};
  look.palette.ink = {0.90f, 0.93f, 0.97f, 1};
  look.palette.rule = {0.19f, 0.20f, 0.26f, 1};
  look.type.captionLabel = {.size = 13, .track = 0.4f};
  return look;
}

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
  return {.eye = {0, 180, 1160}, .target = {0, 0, 0}, .fovYDeg = 34};
}

/** A HARD-EDGED sprite, and it is load-bearing: a soft dot's rim is
 *  semi-transparent, so a mis-ordered sprite reads as haze rather than as
 *  occlusion. The rim is BLACK because the tint is applied by kModulate —
 *  0 * anything is 0, so a black outline survives every tint. */
sk_sp<SkImage> disc() {
  return [] {
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
}

/** The sink: NO depth sort and NO additive blending, because both of those
 *  hide what `order()` does. kSrcOver means the last sprite drawn wins, and
 *  chain order decides who is last. */
Element splat(mesh::Cloud cloud, float spriteSize) {
  // KEYLESS: what the program closes over is a whole point cloud, which no
  // key spells — and the sink paints live at `Cache::None`, so its node was
  // never going to prune.
  // The stamp is baked into the program BY VALUE, once per describe: asked
  // for inside the body it is a 64 px surface rasterised on every paint,
  // which at `Cache::None` is every frame.
  return custom([cloud = std::move(cloud), spriteSize, sprite = disc()](
                    SkCanvas& canvas, const PaintContext& paint) {
           mesh::points::drawBillboards(
               canvas, cloud, lookAtCrown(), paint.size,
               {.sprite = sprite,
                .size = spriteSize,
                .sizeLane = "size",
                .tintLane = "tint",
                .additive = false,     // kSrcOver: order decides the picture
                .depthSort = false});  // the sink's own sort would mask it
         })
      .inset(0)
      .cache(Cache::None);
}

}  // namespace

struct PopOrder {
  mesh::Cloud unsorted, sorted;

  void setup(sketch::SketchContext& ctx) {
    const sketch::kit::Provide look(sheetTheme());
    // Both clouds are cooked in setup; nothing reads the clock.
    sketch::kit::stage(ctx, {.size = {1100, 690}, .captureAt = 0.05});

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

    const auto figure = [](mesh::Cloud cloud) {
      return sketch::kit::well({.width = kPanel, .height = 390})
          .children({splat(std::move(cloud), 34)});
    };
    ctx.composer.render(sketch::kit::page(
        {.title = "Which point owns the overlap?",
         .subtitle = "240 opaque sprites · identical positions, colours and "
                     "seed · the drawing order is the only change",
         .footer = "The sink's depth sort is disabled in both views. order() "
                   "is a CPU permutation, with every point lane following the "
                   "same ordering."},
        box().column().gap(20).children(
            {sketch::kit::comparison(
                 {.cases = {{.title = "SCATTER ORDER",
                             .control = "the generator's sequence",
                             .figure = figure(unsorted),
                             .note = "Dark, distant sprites can cover bright, "
                                     "nearby sprites. The last mark wins."},
                            {.title = "DEPTH ORDER",
                             .control = "order({0, 0, 1}) · farthest first",
                             .figure = figure(sorted),
                             .note =
                                 "Near sprites land last. Overlap now agrees "
                                 "with the depth encoded by colour."}},
                  .measure = 1020,
                  .gap = 24}),
             sketch::kit::readout(
                 {{.name = "DEPTH KEY",
                   .value = "dark blue / far     →     pale blue / near"},
                  {.name = "MEMBERSHIP",
                   .value = kit::formatted("%zu before     %zu after",
                                           unsorted.size(), sorted.size())}},
                 {.nameMeasure = 110})})));
  }
};

SIGIL_SKETCH(PopOrder, "Kit · API",
             "geometry::pop order() — the same points twice, "
             "with and without one call, on a sink that has no depth "
             "buffer")
