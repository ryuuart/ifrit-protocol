// geo_groups.cpp — A HOUDINI .geo GOES OUT AND COMES BACK, AND THE GROUP
// SURVIVES THE TRIP AS THE MASK IT ALWAYS WAS.
// =============================================================================
// A flat grid is built with points::grid, given a tint sweep and a 0/1
// lane "ring", and written as Houdini's JSON .geo by encode::geo. That
// text is then read by the .geo importer, poured into a Cloud, and that
// cloud SEEDS a chain (pop::on(cloud), the PointSet generator) whose lane
// "ring" is exactly what `.masked("ring")` reads.
//
// THE GROUP AND THE LANE ARE THE SAME THING, which is what the round trip
// shows: the reader turns a .geo point group INTO a 0/1 scalar lane under
// its own name, and nothing on this side can tell such a lane from any
// other scalar — so the writer sends it back as the attribute it became.
// Drop a real Houdini save with a real `ring` group in place of the
// encoded text and every panel below is unchanged.
//
//   1. as saved     — the tint lane colours the points; the "ring" lane
//                     is drawn larger by a masked Math on Scale.
//   2. peak outside — everyone outside the ring peaks along N; the ring
//                     stays (the lane inverted into a second mask).
//   3. twist ring   — only the ring turns about +Y.
//
// EDIT THESE FIRST
//   kSide      — the grid's side in points.
//   kRingRadius / kRingWidth — which points the lane holds.
//   kTwistDeg  — panel 3's amount.

#include <include/core/SkCanvas.h>
#include <sigilcompose/core/Core.h>
#include <sigilcompose/kit/Specimen.h>
#include <sigilcompose/kit/Sprites.h>
#include <sigilgeometry/mesh/camera/Camera.h>
#include <sigilgeometry/mesh/codec/Decode.h>
#include <sigilgeometry/mesh/codec/Encode.h>
#include <sigilgeometry/mesh/pop/Points.h>
#include <sigilgeometry/mesh/pop/Pop.h>
#include <sigilsketch/canvas/Sketch.h>
#include <sigilsketch/kit/Kit.h>
#include <sigilweave/style/Type.h>

#include <cmath>
#include <string>
#include <vector>

namespace sketch = sigil::sketch;
namespace weave = sigil::weave;

using namespace sigil::compose;
namespace geometry = sigil::geometry;

namespace {

constexpr int kSide = 28;
constexpr float kSpacing = 14.0f;
constexpr float kRingRadius = 120.0f;
constexpr float kRingWidth = 34.0f;
constexpr float kTwistDeg = 70.0f;
constexpr float kPanel = 360.0f;

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
  look.type.captionLabel = {.size = 12.5f, .track = 0.4f};
  look.type.captionNote = {.size = 10.5f, .track = 0.2f};
  look.spacing.marginX = 30;
  look.spacing.marginTop = 22;
  look.spacing.captionGap = 5;
  return look;
}

const SkColor4f kGround{0.055f, 0.06f, 0.085f, 1};
const SkColor4f kRule{0.19f, 0.20f, 0.26f, 1};
const SkColor4f kInk{0.90f, 0.93f, 0.97f, 1};
const SkColor4f kDim{0.55f, 0.60f, 0.70f, 1};
const SkColor4f kFrame{0.24f, 0.28f, 0.36f, 1};

/** The grid this study saves: a flat lattice with N up, a tint sweep
 *  across x, and a 0/1 lane "ring" — a point group's own spelling on this
 *  side of the seam. */
geometry::mesh::Cloud sourceGrid() {
  const float half = (float)(kSide - 1) * 0.5f * kSpacing;
  geometry::mesh::Cloud cloud = geometry::mesh::points::grid(
      {-half, 0, -half}, {2 * half, 0, 0}, {0, 0, 2 * half}, kSide, kSide);
  // grid() faces its lattice by du x dv, which for x then z points down;
  // this study peaks UP, so the plate's own normal is stated.
  for (glm::vec3& n : cloud.vector("normal")) n = {0, 1, 0};
  std::vector<glm::vec4>& tint = cloud.color("tint");
  std::vector<float>& ring = cloud.scalar("ring");
  for (size_t i = 0; i < cloud.size(); ++i) {
    const float t = (float)(i % (size_t)kSide) / (float)(kSide - 1);
    tint[i] = {0.2f + 0.7f * t, 0.45f, 0.9f - 0.7f * t, 1};
    const glm::vec3 p = cloud.positions[i];
    const float r = std::sqrt(p.x * p.x + p.z * p.z);
    ring[i] = std::abs(r - kRingRadius) < kRingWidth * 0.5f ? 1.0f : 0.0f;
  }
  return cloud;
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
  return sketch::kit::caption(kPanel, toU8(title), toU8(note),
                              box()
                                  .width(kPanel)
                                  .height(kPanel * 0.8f)
                                  .clip()
                                  .stroke(stroke(1.0f, Fill::color(kFrame)))
                                  .child(std::move(inner)));
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
    const sketch::kit::Provide look(sheetTheme());
    sketch::kit::stage(ctx, {.size = {1200, 440}});
    // Every cloud is cooked in setup; nothing reads the clock.
    ctx.captureAt(0.05);

    const std::string geo = geometry::mesh::codec::encode::geo(sourceGrid());
    const std::optional<geometry::mesh::codec::decode::Model> model =
        geometry::mesh::codec::decode::model(geo.data(), geo.size(),
                                             "grid.geo");
    if (!model || model->parts.empty()) {
      caption = "the .geo did not parse";
      ctx.composer.render(
          text(toU8(caption), weave::textStyle({.size = 15, .color = kInk}))
              .left(30)
              .top(16));
      return;
    }
    // asCloud(): positions, "normal" from N, "tint" from Cd, and every
    // group — and every scalar attribute a group came back as — under its
    // own name.
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

    ctx.composer.render(sketch::kit::page(
        {.title = toU8("GEO GROUPS \xc2\xb7 a point group is a pop mask "
                       "the moment it lands"),
         .subtitle = toU8(caption),
         .footer = toU8("a point group arrives from the file as a 0/1 "
                        "lane under its own name — which is what "
                        "masked() reads, and what encode::geo writes "
                        "back out")},
        kit::cells(
            {.cells = {panel("pop::on(part.asCloud())",
                             "Cd from the file; group \"ring\" scaled up",
                             splat(saved)),
                       panel("peak(60).masked(\"outside\")",
                             "the inverted group; the ring stays put",
                             splat(peaked)),
                       panel("twist(70).masked(\"ring\")",
                             "only the group turns", splat(twisted))},
             .gap = 20})));
  }
};

SIGIL_SKETCH(
    GeoGroups, "Kit \xc2\xb7 API",
    "a Houdini .geo written and read back \xe2\x80\x94 its point group is "
    "a pop mask on arrival, and pop::on(cloud) seeds the chain from it")
