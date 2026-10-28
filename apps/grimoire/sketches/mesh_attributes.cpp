/** @file
 * mesh_attributes — the two classes a lane can live in: on the POINTS,
 * where a group is a lane of zeros and ones, and on the TRIANGLES, where
 * two faces that share every point can still differ.
 *
 *   01  THE POINT CLASS. A flat grid carries a tint and a 0/1 lane
 *       "ring" — a point group's own spelling on this side of a file:
 *       Houdini's reader turns a group into exactly this lane, and
 *       nothing downstream can tell it from any other scalar. The lane
 *       is a mask the moment it exists: a Math on Scale draws the ring
 *       larger, the lane inverted into "outside" lets everything but the
 *       ring peak, and a twist masked by the ring turns the ring alone.
 *   02  THE PRIMITIVE CLASS. A "Color" lane written per TRIANGLE onto a
 *       formed torus, the two triangles of every quad a lightness step
 *       apart — no point or vertex lane can say that, because the two
 *       share every point they have. The world body reads it natively
 *       through `.primitiveColor("Color")`; `mesh::bakePrimitiveColor`
 *       unwelds each triangle into three vertices of its own and writes
 *       the value per vertex, which is how the class reaches a consumer
 *       that reads vertex colours and nothing else. Both wear one matte
 *       finish under one light, so any difference is the bake's.
 *
 * EDIT THESE FIRST
 *   kSide                    — the grid's side in points.
 *   kRingRadius / kRingWidth — which points the "ring" lane holds.
 *   kTwistDegrees            — how far the ring turns.
 *   the torus segment counts — how many facets the alternation runs over.
 */

// TAGS: Geometry/Points, Geometry/Meshes

#include <sigilcompose/core/Core.h>
#include <sigilcompose/kit/Document.h>
#include <sigilcompose/kit/Sprites.h>
#include <sigildraw/Pen.h>
#include <sigilgeometry/kit/Solids.h>
#include <sigilgeometry/mesh/Mesh.h>
#include <sigilgeometry/mesh/camera/Camera.h>
#include <sigilgeometry/mesh/pop/Points.h>
#include <sigilgeometry/mesh/pop/Pop.h>
#include <sigilmaterial/color/Color.h>
#include <sigilmaterial/surface/Surface.h>
#include <sigilsketch/kit/Kit.h>
#include <sigilsketch/scene2d/Scene2D.h>
#include <sigilsketch/scene3d/Scene3D.h>
#include <sigilworld/World.h>

#include <cmath>
#include <string>
#include <utility>
#include <vector>

namespace material = sigil::material;
namespace sketch = sigil::sketch;
namespace mesh = sigil::geometry::mesh;
namespace camera = sigil::geometry::mesh::camera;
namespace points = sigil::geometry::mesh::points;
namespace pop = sigil::geometry::mesh::pop;
namespace world = sigil::world;

using namespace sigil::compose;

namespace {

constexpr SkSize kCanvas = {1200, 1080};
constexpr float kMeasure = 1120;

constexpr int kSide = 28;
constexpr float kSpacing = 14;
constexpr float kRingRadius = 120;
constexpr float kRingWidth = 34;
constexpr float kTwistDegrees = 70;
constexpr float kPanel = 360;

constexpr SkISize kBody{540, 300};

/** The grid: a flat lattice spanned z then x, so its normal (du × dv)
 *  faces up; a tint across x, and the 0/1 lane "ring". */
mesh::Cloud grid() {
  const float half = (float)(kSide - 1) * 0.5f * kSpacing;
  mesh::Cloud cloud = points::grid({-half, 0, -half}, {0, 0, 2 * half},
                                   {2 * half, 0, 0}, kSide, kSide);
  std::vector<glm::vec4>& tint = cloud.color("tint");
  std::vector<float>& ring = cloud.scalar("ring");
  for (size_t index = 0; index < cloud.size(); ++index) {
    const glm::vec3 position = cloud.positions[index];
    const float across = (position.x + half) / (2 * half);
    tint[index] = {0.2f + 0.7f * across, 0.45f, 0.9f - 0.7f * across, 1};
    const float radius = std::hypot(position.x, position.z);
    ring[index] =
        std::abs(radius - kRingRadius) < kRingWidth * 0.5f ? 1.0f : 0.0f;
  }
  return cloud;
}

const camera::Camera kGridView{.eye = {0, 520, 620},
                               .target = {0, 0, 0},
                               .verticalFieldOfViewDegrees = 40};

// a literal table; only allocation could throw
// NOLINTNEXTLINE(bugprone-throwing-static-initialization)
const pop::Math kRingLarger{"Scale", {2.2f, 2.2f, 2.2f, 2.2f}, {}, "ring"};

/** One splat of a cooked @p cloud, painted once and held as a texture. */
Element splat(const char* key, mesh::Cloud cloud) {
  return sketch::kit::well(
      {.width = kPanel, .height = kPanel * 0.8f},
      custom(key, [cloud = std::move(cloud), sprite = kit::dotSprite()](
                      sigil::draw::Pen& pen, const PaintContext& paint) {
        points::drawBillboards(*pen.canvas(), cloud, kGridView, paint.size,
                               {.sprite = sprite->frames().front().image,
                                .size = 6,
                                .sizeLane = "size",
                                .tintLane = "tint",
                                .additive = false});
      }).cache(Cache::Texture));
}

/** The primitive lane's colour for triangle @p triangle of @p count: a
 *  walk from warm to cool and back round the torus, the second triangle
 *  of each quad a lightness step darker. */
glm::vec4 facetColour(size_t triangle, size_t count) {
  const float around = (float)(triangle / 2) / (float)(count / 2);
  const material::Color colour =
      material::lerpOklab({0.98f, 0.55f, 0.30f, 1}, {0.40f, 0.55f, 0.98f, 1},
                          1 - std::abs(2 * around - 1));
  const float value = triangle % 2 == 0 ? 1.0f : 0.55f;
  return {colour.r * value, colour.g * value, colour.b * value, 1};
}

}  // namespace

struct MeshAttributes {
  /** The primitive class is drawn by baked world frames, so the sheet needs
   *  a device and is unavailable, saying so, where none is installed. */
  static constexpr bool needsDevice = true;
  static bool available(std::string* why) {
    return sketch::runtimeAvailable(why);
  }

  /** One body under one light, matte and white so what shows is its
   *  colour, baked at the size its well will have. */
  static Element figure(sketch::Scene2DContext& ctx, world::Mesh body) {
    const camera::Camera view{.eye = {0, 400, 330},
                              .target = {0, -10, 0},
                              .verticalFieldOfViewDegrees = 34};
    body.key("subject")
        .transform(camera::place({}, 0, -28))
        .fill(material::surface::program(
            {.baseColor = {1, 1, 1, 1}, .roughness = 1}));
    const world::Frame frame(world::group(
        {world::camera(view, {.toneCurve = world::ToneCurve::Neutral}),
         world::directionalLight({-0.5f, -0.8f, -0.4f}, {.intensity = 2.6f}),
         world::ambientLight({.intensity = 0.9f}), std::move(body)}));
    return sketch::kit::well(
        {.width = (float)kBody.width(), .height = (float)kBody.height()},
        image(ctx.bakeScene3D(frame, kBody, {.camera = view}),
              material::Fit::Stretch));
  }

  void setup(sketch::Scene2DContext& ctx) {
    sketch::kit::stage(
        ctx, {.styleSheet = sketch::kit::featureStyleSheet(
                  sketch::kit::Density::Spacious),
              .size = kCanvas,
              .captureAt = 0.05,
              .background = material::Color{0.051f, 0.051f, 0.075f, 1}});

    // 01 — the point class.
    const mesh::Cloud source = grid();
    size_t inRing = 0;
    if (const std::vector<float>* ring = source.scalarIf("ring"))
      for (const float member : *ring) inRing += member > 0.5f;
    const mesh::Cloud marked = pop::on(source).operation(kRingLarger).cloud();
    const mesh::Cloud complement =
        pop::on(source)
            .copy("ring", "outside")
            .operation(pop::Math{"outside", {-1, 0, 0, 0}, {1, 0, 0, 0}})
            .peak(60)
            .masked("outside")
            .operation(kRingLarger)
            .cloud();
    const mesh::Cloud turned = pop::on(source)
                                   .twist(kTwistDegrees, {0, 1, 0}, -1, 1)
                                   .masked("ring")
                                   .peak(30)
                                   .masked("ring")
                                   .operation(kRingLarger)
                                   .cloud();

    // 02 — the primitive class.
    mesh::Mesh facets = mesh::torus(130, 46, 34, 14);
    std::vector<glm::vec4>& colour = facets.primitive("Color");
    for (size_t triangle = 0; triangle < colour.size(); ++triangle)
      colour[triangle] = facetColour(triangle, colour.size());
    const mesh::Mesh baked = mesh::bakePrimitiveColor(facets, "Color");

    ctx.composer.render(sketch::kit::page(
        {.title = "Where a lane lives",
         .subtitle = "On the points, a group is a lane of zeros and ones; on "
                     "the triangles, a value two faces sharing every point "
                     "can disagree on",
         .footer = "Larger dots mark the ring. The first two bodies share the "
                   "mesh, camera and matte light; baking duplicates vertices "
                   "so neighbouring triangles keep their own colours."},
        box().column().gap(26).children(
            {document::h2(kit::formatted(
                 "01 / A GROUP IS A LANE · %zu POINTS, %zu IN \"ring\"",
                 source.size(), inRing)),
             sketch::kit::comparison(
                 {.cases = {{.title = "THE GROUP, MARKED",
                             .control = "Math on Scale, masked by \"ring\"",
                             .figure = splat("marked", marked),
                             .note = "The tint lane colours the points; the "
                                     "ring is drawn larger."},
                            {.title = "MOVE ITS COMPLEMENT",
                             .control = "peak(60).masked(\"outside\")",
                             .figure = splat("complement", complement),
                             .note = "The lane inverted into a second mask; "
                                     "the ring stays put."},
                            {.title = "TURN THE GROUP",
                             .control = "twist(70).masked(\"ring\")",
                             .figure = splat("turned", turned),
                             .note = "Only the group turns."}},
                  .measure = kMeasure,
                  .gap = 20}),
             document::h2("02 / A LANE ON THE TRIANGLES"),
             sketch::kit::comparison(
                 {.cases =
                      {{.title = "READ NATIVELY",
                        .control = "primitiveColor(\"Color\")",
                        .figure = figure(
                            ctx, world::mesh(facets).primitiveColor("Color")),
                        .note = kit::formatted(
                            "%zu vertices · %zu triangles. Two triangles "
                            "share points and differ in brightness.",
                            facets.vertexCount(), facets.triangleCount())},
                       {.title = "BAKED INTO VERTICES",
                        .control = "bakePrimitiveColor(\"Color\")",
                        .figure = figure(ctx, world::mesh(baked)),
                        .note = kit::formatted(
                            "%zu vertices · %zu triangles. The same picture, "
                            "spelled per vertex.",
                            baked.vertexCount(), baked.triangleCount())}},
                  .measure = kMeasure,
                  .gap = 24})})));
  }
};

SIGIL_SKETCH(MeshAttributes, "Kit · API",
             "the two classes a lane lives in: a point group as a 0/1 lane "
             "masking scale, peak and twist, and a primitive colour lane "
             "read natively beside the same lane baked into vertices")
