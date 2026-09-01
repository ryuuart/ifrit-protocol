// spline_stations.cpp — INSTANCES STANDING ON A CURVE'S OWN FRAMES.
// =============================================================================
// A spline scattered into points writes more than positions. Each point
// carries the frame the curve had where it sits — `t` along the run, the
// tangent, the normal and the binormal — as ordinary named lanes on the
// cloud. Which means an instancer never has to be told where to face:
// it is handed the NAME of a lane and reads the direction out of it.
//
//   THE RAIL   the knot swept with a circle profile, so the curve the
//              stations sit on is visible as a body rather than implied.
//   THE LANES  a cooked `facing` lane, written here: the binormal leaned
//              toward the normal, which is what tilts a flat panel like a
//              solar array rather than standing it edge-on. A `tint`
//              lane graded along `t`.
//   THE STAMP  `points::quads` — one call, one mesh, every station.
//              `InstanceOptions` names the two lanes and nothing else.
//   THE PROOF  the same spline value projected to a 2D path and stroked
//              over the top. One value fed the body and the outline.
//
// EDIT THESE FIRST
//   the lean factor in the facing loop — at 0 the panels stand along the
//   binormal, at large values they lie flat against the curve.
//   the station count in onSpline    — how many panels ride the rail.

#include <include/core/SkPaint.h>
#include <sigilgeometry/mesh/camera/Camera.h>
#include <sigilgeometry/mesh/curve/Curve.h>
#include <sigilgeometry/mesh/pop/Points.h>
#include <sigilgeometry/mesh/render/Painter.h>
#include <sigilsketch/canvas/Sketch.h>

#include <cmath>

namespace sketch = sigil::sketch;

using namespace sigil::compose;
namespace mesh = sigil::geometry::mesh;
namespace camera = sigil::geometry::mesh::camera;
namespace curve = sigil::geometry::mesh::curve;
namespace points = sigil::geometry::mesh::points;
namespace render = sigil::geometry::mesh::render;

namespace {

constexpr SkSize kCanvas = {1240, 720};

curve::Spline3 knot() {
  curve::Spline3 spline;
  spline.closed = true;
  for (int i = 0; i < 10; ++i) {
    const float a = (float)i / 10.0f * 6.2831853f;
    spline.points.emplace_back(std::cos(a) * 300, std::sin(a * 3.0f) * 110,
                               std::sin(a) * 300);
  }
  return spline;
}

}  // namespace

struct SplineStations final : sketch::Sketch {
  curve::Spline3 rail;
  mesh::Mesh tube, stations;

  void draw(SkCanvas& canvas) const {
    camera::Camera view;
    view.eye = {60, 500, 780};
    view.target = {0, -20, 0};
    view.fovYDeg = 44;

    render::MeshStyle steel;
    steel.baseColor = {0.6f, 0.68f, 0.8f, 1};
    steel.specular = 0.9f;
    steel.shininess = 48;
    render::drawMesh(canvas, tube, glm::mat4(1.0f), view, kCanvas, steel);

    render::MeshStyle panels;
    panels.baseColor = {1, 1, 1, 1};
    panels.ambient = {0.85f, 0.85f, 0.9f, 1};
    panels.specular = 0;
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
    ctx.background({0.027f, 0.027f, 0.047f, 1});
    ctx.captureAt(1.0);

    rail = knot();
    tube = curve::sweep(rail, curve::profile::circle(12),
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

SIGIL_SKETCH(SplineStations, "Kit · API",
             "points::quads over a spline's frames — a cooked facing lane "
             "tilts every panel, and the same spline draws as an outline")
