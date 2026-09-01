// mesh_primitives.cpp — THE FOUR WAYS A BODY GETS MADE, under one camera.
// =============================================================================
// Everything the mesh layer generates is the same `Mesh` value, and the
// painter that draws it knows nothing about which generator produced it.
// So the interesting thing about a lineup like this is not the shapes,
// it is that placing, lighting and drawing them is ONE call each,
// whatever they came from.
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
// The lights are one warm key and one cool fill, stated once and shared:
// a lineup lit two different ways is a lineup you cannot read.
//
// EDIT THESE FIRST
//   camera.eye / fovYDeg — the whole plate reframes.
//   the profile loop     — the vase's silhouette, one number per step.
//   MeshStyle::shininess — the Blinn exponent, i.e. how tight the
//                          highlights pull in.

#include <include/core/SkMatrix.h>
#include <sigilcompose/shape/Shapes.h>
#include <sigilgeometry/mesh/Mesh.h>
#include <sigilgeometry/mesh/camera/Camera.h>
#include <sigilgeometry/mesh/render/Painter.h>
#include <sigilsketch/canvas/Sketch.h>

#include <cmath>
#include <vector>

namespace sketch = sigil::sketch;

using namespace sigil::compose;
namespace mesh = sigil::geometry::mesh;
namespace camera = sigil::geometry::mesh::camera;
namespace render = sigil::geometry::mesh::render;

namespace {

constexpr SkSize kCanvas = {1240, 720};

}  // namespace

struct MeshPrimitives final : sketch::Sketch {
  mesh::Mesh star, ring, vase, pedestal;

  void draw(SkCanvas& canvas) const {
    camera::Camera view;
    view.eye = {0, 260, 950};
    view.target = {0, -30, 0};
    view.fovYDeg = 42;

    render::MeshStyle steel;
    steel.baseColor = {0.72f, 0.75f, 0.82f, 1};
    steel.lights = {{{-0.55f, -0.7f, -0.45f}, {1.0f, 0.96f, 0.9f, 1}, 1.1f},
                    {{0.7f, -0.2f, -0.3f}, {0.4f, 0.55f, 0.9f, 1}, 0.5f}};
    steel.specular = 0.9f;
    steel.shininess = 64;

    render::MeshStyle slate = steel;
    slate.baseColor = {0.3f, 0.32f, 0.4f, 1};
    slate.specular = 0.4f;
    render::drawMesh(canvas, pedestal, camera::place({0, -150, 0}), view,
                     kCanvas, slate);

    render::drawMesh(canvas, star, camera::place({-300, 60, 0}, 38, -18, 8),
                     view, kCanvas, steel);

    render::MeshStyle bronze = steel;
    bronze.baseColor = {0.85f, 0.55f, 0.3f, 1};
    render::drawMesh(canvas, ring, camera::place({20, 40, -60}, 0, -32, 14),
                     view, kCanvas, bronze);

    render::MeshStyle jade = steel;
    jade.baseColor = {0.35f, 0.8f, 0.6f, 1};
    render::drawMesh(canvas, vase, camera::place({330, 30, -30}), view, kCanvas,
                     jade);
  }

  void setup(sketch::SketchContext& ctx) override {
    ctx.canvas(kCanvas.width(), kCanvas.height());
    ctx.background({0.051f, 0.051f, 0.075f, 1});
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

    ctx.composer.render(custom([this](SkCanvas& canvas, const PaintContext&) {
                          draw(canvas);
                        }).inset(0));
  }
};

SIGIL_SKETCH(MeshPrimitives, "Kit · API",
             "extrude, revolve, torus and superellipsoid under one "
             "camera — four generators, one Mesh currency, one draw call "
             "each")
