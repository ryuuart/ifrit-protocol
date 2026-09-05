// mesh_generators.cpp — EVERY GENERATOR HANDS BACK THE SAME MESH.
// =============================================================================
// The mesh layer makes bodies six ways on this sheet and the painter
// that draws them knows nothing about which call produced which. So the
// interesting thing about a lineup like this is not the shapes: it is
// that placing, lighting and drawing them is ONE call each, whatever
// they came from, and that the last two are made out of a curve without
// leaving that currency.
//
// THE LINEUP, on the left, from a shape and from numbers:
//
//   EXTRUDE        a 2D outline given thickness. The outline is a shape
//                  value from the shape kit, so anything an element can
//                  BE is also something the mesh layer can thicken.
//   REVOLVE        a profile lathed about the axis — the vase's silhouette
//                  is a list of (radius, height) pairs and nothing else.
//   TORUS          a stock body, for the same reason a stock circle
//                  exists: it is what the others get compared against.
//   SUPERELLIPSOID the rounded box, at an exponent that reads as a slab.
//                  It is the pedestal, and its job is to be the one large
//                  flat-ish surface in the frame: the lighting direction
//                  is only legible on something broad enough to show the
//                  falloff across it.
//
// THE RAIL, on the right, from a curve:
//
//   SWEEP          the knot run through a circle profile, so the curve
//                  is a body rather than an implication.
//   QUADS          a spline scattered into points writes more than
//                  positions: each point carries the frame the curve had
//                  where it sits — `t`, the tangent, the normal, the
//                  binormal — as ordinary named lanes. So an instancer
//                  is never told a direction; it is handed the NAME of a
//                  lane and reads the direction out of it. The `facing`
//                  lane here is cooked by this sketch — the binormal
//                  leaned toward the normal, which tilts a panel like a
//                  solar array rather than standing it edge-on — and it
//                  slots in exactly where a built-in lane would.
//
// One value fed the rail's body and the white outline stroked over it:
// `curve::project` takes the same spline the sweep took.
//
// The lights are one warm key and one cool fill, stated once and shared
// by both halves: a lineup lit two different ways is a lineup you cannot
// read. The panels are the exception and say so — `MeshStyle::lit =
// false` is a surface that is its own light, which is what an array of
// thin plates wants so its tint lane is legible rather than shaded.
//
// EDIT THESE FIRST
//   camera.eye / fovYDeg — the whole plate reframes.
//   the profile loop     — the vase's silhouette, one number per step.
//   the lean factor in the facing loop — at 0 the panels stand along the
//                          binormal, at large values they lie flat.
//   the station count in onSpline — how many panels ride the rail.

#include <include/core/SkMatrix.h>
#include <include/core/SkPaint.h>
#include <sigilgeometry/kit/Sections.h>
#include <sigilgeometry/kit/Silhouettes.h>
#include <sigilgeometry/mesh/pop/Sweep.h>
#include <sigilgeometry/kit/Solids.h>
#include <sigilgeometry/mesh/Mesh.h>
#include <sigilgeometry/mesh/camera/Camera.h>
#include <sigilgeometry/mesh/curve/Curve.h>
#include <sigilgeometry/mesh/pop/Points.h>
#include <sigilgeometry/mesh/render/Painter.h>
#include <sigilsketch/canvas/Sketch.h>

#include <cmath>
#include <vector>

namespace sketch = sigil::sketch;
namespace shapes = sigil::geometry::shapes;

using namespace sigil::compose;
namespace mesh = sigil::geometry::mesh;
namespace camera = sigil::geometry::mesh::camera;
namespace curve = sigil::geometry::mesh::curve;
namespace points = sigil::geometry::mesh::points;
namespace render = sigil::geometry::mesh::render;

namespace {

constexpr SkSize kCanvas = {1400, 500};
/** How far apart the two halves stand, so one camera holds both. */
constexpr float kLineupAt = -400.0f;
constexpr float kRailAt = 470.0f;

curve::Spline3 knot() {
  curve::Spline3 spline;
  spline.closed = true;
  for (int i = 0; i < 10; ++i) {
    const float a = (float)i / 10.0f * 6.2831853f;
    spline.points.emplace_back(kRailAt + std::cos(a) * 250,
                               std::sin(a * 3.0f) * 105, std::sin(a) * 250);
  }
  return spline;
}

}  // namespace

struct MeshGenerators final : sketch::Sketch {
  mesh::Mesh star, ring, vase, pedestal, tube, stations;
  curve::Spline3 rail;

  void draw(SkCanvas& canvas) const {
    camera::Camera view;
    view.eye = {0, 520, 1000};
    view.target = {0, 0, 0};
    view.fovYDeg = 34;

    render::MeshStyle steel;
    steel.baseColor = {0.72f, 0.75f, 0.82f, 1};
    steel.lights = {{{-0.55f, -0.7f, -0.45f}, {1.0f, 0.96f, 0.9f, 1}, 1.1f},
                    {{0.7f, -0.2f, -0.3f}, {0.4f, 0.55f, 0.9f, 1}, 0.5f}};
    steel.specular = 0.9f;
    steel.shininess = 64;

    render::MeshStyle slate = steel;
    slate.baseColor = {0.3f, 0.32f, 0.4f, 1};
    slate.specular = 0.4f;
    render::drawMesh(canvas, pedestal, camera::place({kLineupAt, -150, 0}),
                     view, kCanvas, slate);

    render::drawMesh(canvas, star,
                     camera::place({kLineupAt - 290, 60, 0}, 38, -18, 8), view,
                     kCanvas, steel);

    render::MeshStyle bronze = steel;
    bronze.baseColor = {0.85f, 0.55f, 0.3f, 1};
    render::drawMesh(canvas, ring,
                     camera::place({kLineupAt + 20, 40, -60}, 0, -32, 14), view,
                     kCanvas, bronze);

    render::MeshStyle jade = steel;
    jade.baseColor = {0.35f, 0.8f, 0.6f, 1};
    render::drawMesh(canvas, vase, camera::place({kLineupAt + 310, 30, -30}),
                     view, kCanvas, jade);

    render::MeshStyle rail_steel = steel;
    rail_steel.baseColor = {0.6f, 0.68f, 0.8f, 1};
    rail_steel.shininess = 48;
    render::drawMesh(canvas, tube, glm::mat4(1.0f), view, kCanvas, rail_steel);

    // The panels are their own light, so the tint lane graded along `t`
    // reads as the lane rather than as the key's falloff across it.
    render::MeshStyle panels;
    panels.lit = false;
    panels.baseColor = {0.86f, 0.87f, 0.92f, 1};
    render::drawMesh(canvas, stations, glm::mat4(1.0f), view, kCanvas, panels);

    SkPaint wire;
    wire.setAntiAlias(true);
    wire.setStyle(SkPaint::kStroke_Style);
    wire.setStrokeWidth(1.2f);
    wire.setColor4f({1, 1, 1, 0.35f});
    canvas.drawPath(curve::project(rail, view, kCanvas, 400), wire);
  }

  void setup(sketch::SketchContext& ctx) override {
    ctx.canvas(kCanvas.width(), kCanvas.height());
    ctx.background({0.04f, 0.04f, 0.062f, 1});
    ctx.captureAt(1.0);

    // The bodies are built once. A generator's cost belongs to the
    // description, not to the frame: nothing below changes per frame, so
    // rebuilding them there would be paying for the same vertices again.
    star = mesh::extrude(shapes::star(5, 44.0f / 95.0f)
                             .path({190, 190})
                             .makeTransform(SkMatrix::Translate(-95, -95)),
                         {.depth = 40});
    ring = mesh::torus(110, 40);

    std::vector<glm::vec2> profile;
    for (int i = 0; i <= 24; ++i) {
      const float t = (float)i / 24.0f;
      const float r = 46.0f + 34.0f * std::sin(t * 3.1f + 0.4f) +
                      18.0f * std::sin(t * 8.0f);
      profile.emplace_back(r, (t - 0.5f) * 240.0f);
    }
    vase = mesh::revolve(profile);
    pedestal = mesh::superellipsoid({420, 26, 200}, 6);

    rail = knot();
    tube = mesh::pop::sweep(rail, sections::circle(12),
                        {.segments = 220, .scale = 9});

    mesh::Cloud cloud = points::onSpline(rail, 14);
    {
      std::vector<glm::vec4>& tint = cloud.color("tint");
      std::vector<glm::vec3>& facing = cloud.vector("facing");
      const std::vector<float>& t = *cloud.scalarIf("t");
      const std::vector<glm::vec3>& normal = *cloud.vectorIf("normal");
      const std::vector<glm::vec3>& binormal = *cloud.vectorIf("binormal");
      for (size_t i = 0; i < cloud.size(); ++i) {
        tint[i] = {0.35f + 0.6f * t[i], 0.9f - 0.5f * t[i], 1.0f, 0.85f};
        const glm::vec3 lean = binormal[i] + normal[i] * 1.2f;
        const float len = glm::length(lean);
        facing[i] = len > 1e-6f ? lean * (1.0f / len) : normal[i];
      }
    }
    points::InstanceOptions options;
    options.orientLane = "facing";
    options.tintLane = "tint";
    stations = points::quads(cloud, 96, 64, options);

    ctx.composer.render(custom([this](SkCanvas& canvas, const PaintContext&) {
                          draw(canvas);
                        }).inset(0));
  }
};

SIGIL_SKETCH(MeshGenerators, "Kit · API",
             "extrude, revolve, torus, superellipsoid, sweep and quads "
             "under one camera — six generators, one Mesh currency, one "
             "draw call each")
