// geo_groups.cpp — A HOUDINI .geo COMES IN, GROUPS AND ALL, and its groups
// are pop masks the moment they land.
// =============================================================================
// The file is parsed by SigilGeometry's importer, poured into a Cloud, and
// that cloud SEEDS a chain (pop::on(cloud), the PointSet generator). Its
// point group "ring" is a 0/1 lane under that name — exactly what
// `.masked("ring")` reads.
//
// The .geo text is generated below in the shape Houdini writes (paged
// attributes, boolRLE groups) so this file stays self-contained; drop a
// real save in its place and nothing else changes.
//
//   1. as saved     — Cd from the file colours the points; the "ring"
//                     group is drawn larger by a masked Math on Scale.
//   2. peak outside — everyone outside the ring peaks along N; the ring
//                     stays (the group inverted into a second mask).
//   3. twist ring   — only the ring turns about +Y.
//
// EDIT THESE FIRST
//   kSide      — the grid's side in points (the file is regenerated).
//   kRingRadius / kRingWidth — which points the group holds.
//   kTwistDeg  — panel 3's amount.

#include <include/core/SkCanvas.h>
#include <sigilcompose/kit/Sprites.h>
#include <sigilcompose/typography/Typography.h>
#include <sigilgeometry/mesh/camera/Camera.h>
#include <sigilgeometry/mesh/codec/Decode.h>
#include <sigilgeometry/mesh/pop/Points.h>
#include <sigilgeometry/mesh/pop/Pop.h>
#include <sigilsketch/canvas/Sketch.h>

#include <cmath>
#include <string>
#include <vector>

namespace sketch = sigil::sketch;

using namespace sigil::compose;
namespace geometry = sigil::geometry;

namespace {

constexpr int kSide = 28;
constexpr float kSpacing = 14.0f;
constexpr float kRingRadius = 120.0f;
constexpr float kRingWidth = 34.0f;
constexpr float kTwistDeg = 70.0f;
constexpr float kPanel = 360.0f;

const SkColor4f kInk{0.90f, 0.93f, 0.97f, 1};
const SkColor4f kDim{0.55f, 0.60f, 0.70f, 1};
const SkColor4f kFrame{0.24f, 0.28f, 0.36f, 1};

/** A .geo save of a flat grid of points with N up, a Cd sweep across x,
 *  and a point group "ring": paged P (one page, the way Houdini pages
 *  1024 elements), tuple-listed N and Cd, a boolRLE group. */
std::string houdiniGeo() {
  const int n = kSide * kSide;
  std::string p, nrm, cd, rle;
  int runFlag = -1, runLen = 0;
  const auto flush = [&] {
    if (runLen == 0) return;
    if (!rle.empty()) rle += ',';
    rle += std::to_string(runLen) + (runFlag ? ",true" : ",false");
  };
  for (int i = 0; i < n; ++i) {
    const float x = ((float)(i % kSide) - (float)(kSide - 1) * 0.5f) * kSpacing;
    const int row = i / kSide;
    const float z = ((float)row - (float)(kSide - 1) * 0.5f) * kSpacing;
    if (i) {
      p += ',';
      nrm += ',';
      cd += ',';
    }
    p += std::to_string(x) + ",0," + std::to_string(z);
    nrm += "[0,1,0]";
    const float t = (float)(i % kSide) / (float)(kSide - 1);
    cd += "[" + std::to_string(0.2f + 0.7f * t) + "," + std::to_string(0.45f) +
          "," + std::to_string(0.9f - 0.7f * t) + "]";
    const float r = std::sqrt(x * x + z * z);
    const int inRing = std::abs(r - kRingRadius) < kRingWidth * 0.5f;
    if (inRing == runFlag) {
      ++runLen;
    } else {
      flush();
      runFlag = inRing;
      runLen = 1;
    }
  }
  flush();
  return "[\"fileversion\",\"20.5.278\",\"pointcount\"," + std::to_string(n) +
         ",\"vertexcount\",0,\"primitivecount\",0,"
         "\"topology\",[\"pointref\",[\"indices\",[]]],"
         "\"attributes\",[\"pointattributes\",["
         "[[\"scope\",\"public\",\"type\",\"numeric\",\"name\",\"P\","
         "\"options\",{}],[\"size\",3,\"storage\",\"fpreal32\",\"values\","
         "[\"size\",3,\"storage\",\"fpreal32\",\"packing\",[3],\"pagesize\","
         "1024,\"constantpageflags\",[[false]],\"rawpagedata\",[" +
         p +
         "]]]],"
         "[[\"scope\",\"public\",\"type\",\"numeric\",\"name\",\"N\","
         "\"options\",{}],[\"size\",3,\"storage\",\"fpreal32\",\"values\","
         "[\"size\",3,\"storage\",\"fpreal32\",\"tuples\",[" +
         nrm +
         "]]]],"
         "[[\"scope\",\"public\",\"type\",\"numeric\",\"name\",\"Cd\","
         "\"options\",{}],[\"size\",3,\"storage\",\"fpreal32\",\"values\","
         "[\"size\",3,\"storage\",\"fpreal32\",\"tuples\",[" +
         cd +
         "]]]]]],"
         "\"primitives\",[],"
         "\"pointgroups\",[[[\"name\",\"ring\"],[\"selection\",[\"unordered\","
         "[\"boolRLE\",[" +
         rle + "]]]]]]]";
}

geometry::mesh::camera::Camera lookDown() {
  geometry::mesh::camera::Camera camera;
  camera.eye = {0, 520, 620};
  camera.target = {0, 0, 0};
  camera.fovYDeg = 40;
  return camera;
}

/** The point stamp, baked once for the process. */
const sk_sp<SkImage>& disc() {
  static const sk_sp<SkImage> img = kit::dotSprite();
  return img;
}

Element splat(geometry::mesh::Cloud cloud) {
  return custom([cloud = std::move(cloud)](SkCanvas& canvas,
                                           const PaintContext& paint) {
           geometry::mesh::points::BillboardStyle style;
           style.sprite = disc();
           style.size = 6;
           style.sizeLane = "size";
           style.tintLane = "tint";
           style.additive = false;
           style.depthSort = true;
           geometry::mesh::points::drawBillboards(canvas, cloud, lookDown(),
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
      .child(text(toU8(title), type({.size = 12.5f, .color = kInk})))
      .child(box()
                 .width(kPanel)
                 .height(kPanel * 0.8f)
                 .clip()
                 .stroke(stroke(1.0f, Fill::color(kFrame)))
                 .child(std::move(inner)))
      .child(text(toU8(note), type({.size = 10.5f, .color = kDim})));
}

// a literal table; only allocation could throw
// NOLINTNEXTLINE(bugprone-throwing-static-initialization)
const geometry::mesh::pop::Math kRingLarger{
    "Scale", {2.2f, 2.2f, 2.2f, 2.2f}, {}, "ring"};

}  // namespace

struct GeoGroups : sketch::Sketch {
  geometry::mesh::Cloud saved, peaked, twisted;
  std::string caption;

  void setup(sketch::SketchContext& ctx) override {
    ctx.captureAt(6.0);
    ctx.canvas(1200, 400);
    ctx.background({0.055f, 0.06f, 0.085f, 1});

    const std::string geo = houdiniGeo();
    const std::optional<geometry::mesh::codec::decode::Model> model =
        geometry::mesh::codec::decode::model(geo.data(), geo.size(),
                                             "grid.geo");
    if (!model || model->parts.empty()) {
      caption = "the .geo did not parse";
      ctx.composer.render(text(toU8(caption), type({.size = 15, .color = kInk}))
                              .left(30)
                              .top(16));
      return;
    }
    // asCloud(): positions, "normal" from N, "tint" from Cd, and every
    // group as a 0/1 scalar lane under its own name.
    const geometry::mesh::Cloud seed = model->parts.front().asCloud();
    int inRing = 0;
    if (const std::vector<float>* ring = seed.scalarIf("ring"))
      for (float f : *ring) inRing += f > 0.5f;
    caption = std::to_string(seed.size()) +
              " points, group \"ring\" = " + std::to_string(inRing) +
              " of them";

    // 1. As saved, the ring drawn larger: a Math on Scale, masked.
    saved = geometry::mesh::pop::on(seed).op(kRingLarger).cloud();
    // 2. Peak everyone OUTSIDE the ring: the group inverted into a
    // second lane by a Math, and the peak masked by that.
    peaked = geometry::mesh::pop::on(seed)
                 .copy("ring", "outside")
                 .op(geometry::mesh::pop::Math{
                     "outside", {-1, 0, 0, 0}, {1, 0, 0, 0}})
                 .peak(60)
                 .masked("outside")
                 .op(kRingLarger)
                 .cloud();
    // 3. Only the ring turns.
    twisted = geometry::mesh::pop::on(seed)
                  .twist(kTwistDeg, {0, 1, 0}, -1, 1)
                  .masked("ring")
                  .peak(30)
                  .masked("ring")
                  .op(kRingLarger)
                  .cloud();

    ctx.composer.render(
        stack()
            .child(text(toU8("geometry::decode \xc2\xb7 a Houdini .geo's point "
                             "group is a pop mask the moment it lands "
                             "\xe2\x80\x94 " +
                             caption),
                        type({.size = 15, .color = kInk}))
                       .left(30)
                       .top(16))
            .child(
                box()
                    .row()
                    .left(30)
                    .top(48)
                    .gap(20)
                    .child(panel("pop::on(part.asCloud())",
                                 "Cd from the file; group \"ring\" scaled up",
                                 splat(saved)))
                    .child(panel("peak(60).masked(\"outside\")",
                                 "the inverted group; the ring stays put",
                                 splat(peaked)))
                    .child(panel("twist(70).masked(\"ring\")",
                                 "only the group turns", splat(twisted))))
            .child(text(toU8("a point group arrives from the file as a 0/1 "
                             "lane under its own name, which is what "
                             "masked() reads"),
                        type({.size = 11, .color = kDim}))
                       .left(30)
                       .bottom(14)));
  }
};

SIGIL_SKETCH(
    GeoGroups, "Kit \xc2\xb7 API",
    "geometry::decode of a Houdini .geo \xe2\x80\x94 its point group is a "
    "pop mask on arrival, and pop::on(cloud) seeds the chain from it")
