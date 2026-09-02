// pop_prims.cpp — THE PRIMITIVE CLASS: attributes that live on TRIANGLES.
// =============================================================================
// A mesh carries lanes on its points and lanes on its PRIMITIVES, and
// the second class is not a convenience for the first. The two triangles
// of a quad can hold different values; no point or vertex attribute can
// say that, because the two triangles share every point they have.
// Three stanzas, left to right, and the middle one is the reason the
// class needs a bake at all.
//
//   1. FACETS  a prim "Color" lane written straight onto a formed body
//      and read natively by `MeshStyle::primColorLane`. The two
//      triangles of every quad alternate brightness — that alternation
//      IS the demonstration.
//   2. BAKED   the SAME lane through `mesh::bakePrimColor`, which
//      unwelds each triangle into three of its own vertices and writes
//      the value per vertex. That is how the class reaches a pipeline
//      with vertex attributes and nothing else. Both styles run flat —
//      no specular, no rim — so the two pictures are comparable and any
//      difference is the bake's, not the lighting's.
//   3. PIECES  the point class promoted INTO the prim class.
//      `.promote("Id")` stamps every triangle with its owning point's
//      index, so a stamp instance is a RUN OF TRIANGLES sharing an Id
//      value rather than a second container the renderer must know
//      about. Colouring by that Id is what makes the runs visible.
//
// The colours come off a perceptual ramp rather than a hue formula:
// interpolating in OKLab is what keeps a run of tints from collapsing
// into mud in its middle, and the alternation in stanzas 1 and 2 is a
// lightness step on the same ramp.
//
// EDIT THESE FIRST
//   the torus segment counts — how many facets the alternation runs over.
//   kPieces                  — instances in stanza 3; the Id stride
//                              below scatters neighbouring runs apart.

#include <sigilgeometry/kit/Solids.h>
#include <sigilgeometry/mesh/Mesh.h>
#include <sigilgeometry/mesh/camera/Camera.h>
#include <sigilgeometry/mesh/pop/Pop.h>
#include <sigilgeometry/mesh/render/Painter.h>
#include <sigilmaterial/color/Color.h>
#include <sigilsketch/canvas/Sketch.h>

#include <algorithm>
#include <cmath>
#include <vector>

namespace sketch = sigil::sketch;

using namespace sigil::compose;
namespace mesh = sigil::geometry::mesh;
namespace camera = sigil::geometry::mesh::camera;
using sigil::geometry::mesh::pop;
namespace render = sigil::geometry::mesh::render;
namespace material = sigil::material;

namespace {

constexpr SkSize kCanvas = {1240, 720};
constexpr int kPieces = 64;

/** A four-stop ramp walked in OKLab, at a lightness multiplier. @p f
 *  wraps, so the ramp closes on itself the way a loop of facets needs
 *  it to.
 *
 *  Named for the space it walks: `ramp` alone is compose's transition
 *  spelled in milliseconds, and both take two floats. */
glm::vec4 oklabRamp(float f, float value) {
  static const material::Color stops[5] = {{0.98f, 0.42f, 0.30f, 1},
                                           {0.98f, 0.85f, 0.35f, 1},
                                           {0.35f, 0.88f, 0.62f, 1},
                                           {0.40f, 0.55f, 0.98f, 1},
                                           {0.98f, 0.42f, 0.30f, 1}};
  f -= std::floor(f);
  const float scaled = f * 4.0f;
  const int i = std::min((int)scaled, 3);
  const material::Color c =
      material::lerpOklab(stops[i], stops[i + 1], scaled - (float)i);
  return {c.r * value, c.g * value, c.b * value, 1.0f};
}

}  // namespace

struct PopPrims final : sketch::Sketch {
  mesh::Mesh facets, baked, pieces;

  void draw(SkCanvas& canvas) const {
    camera::Camera view;
    view.eye = {0, 210, 900};
    view.target = {0, 0, 0};
    view.fovYDeg = 42;

    render::MeshStyle flat;
    flat.baseColor = {1, 1, 1, 1};
    flat.ambient = {0.34f, 0.34f, 0.38f, 1};
    flat.specular = 0;  // no view-dependent term: 1 and 2 must be comparable
    flat.rim = 0;

    render::MeshStyle lane = flat;
    lane.primColorLane = "Color";
    render::drawMesh(canvas, facets, camera::place({-380, 10, 0}, 0, -28), view,
                     kCanvas, lane);

    render::drawMesh(canvas, baked, camera::place({0, 10, 0}, 0, -28), view,
                     kCanvas, flat);

    render::MeshStyle stamped = lane;
    stamped.ambient = {0.9f, 0.9f, 0.95f, 1};
    render::drawMesh(canvas, pieces, camera::place({380, 10, 0}), view, kCanvas,
                     stamped);
  }

  void setup(sketch::SketchContext& ctx) override {
    ctx.canvas(kCanvas.width(), kCanvas.height());
    ctx.background({0.051f, 0.051f, 0.075f, 1});
    ctx.captureAt(1.0);

    // 1 — a prim lane written straight onto a formed body.
    facets = mesh::torus(130, 46, 34, 14);
    {
      std::vector<glm::vec4>& color = facets.prim("Color");
      const size_t quads = color.size() / 2;
      for (size_t t = 0; t < color.size(); ++t) {
        // The two triangles of a quad share its ramp position and differ
        // only in lightness — which is the alternation this stanza is for.
        const size_t quad = t / 2;
        color[t] =
            oklabRamp((float)quad / (float)quads, t % 2 == 0 ? 1.0f : 0.55f);
      }
    }

    // 2 — the same lane unwelded into per-vertex colours.
    baked = mesh::bakePrimColor(facets, "Color");

    // 3 — the point class promoted into the prim class.
    std::vector<glm::vec3> loop;
    for (int i = 0; i < 12; ++i) {
      const float a = (float)i / 12.0f * 6.2831853f;
      loop.emplace_back(160.0f * std::cos(a), 40.0f * std::sin(a * 3.0f),
                        160.0f * std::sin(a));
    }
    pieces = pop::on(loop)
                 .count(kPieces)
                 .spread(30)
                 .vary(0.45f)
                 .lookAt({0, 210, 900})
                 .promote("Id")
                 .stamps(mesh::quad(46, 46));
    if (const std::vector<glm::vec4>* ids = pieces.primIf("Id")) {
      std::vector<glm::vec4>& tint = pieces.prim("Color");
      for (size_t t = 0; t < tint.size(); ++t) {
        const int id = (int)(*ids)[t].x;
        tint[t] = oklabRamp((float)(id * 19 % kPieces) / (float)kPieces,
                            t % 2 == 0 ? 1.0f : 0.62f);
      }
    }

    ctx.composer.render(custom([this](SkCanvas& canvas, const PaintContext&) {
                          draw(canvas);
                        }).inset(0));
  }
};

SIGIL_SKETCH(PopPrims, "Kit · API",
             "attributes on triangles — a prim colour lane read natively, "
             "the same lane baked into vertices, and a point lane "
             "promoted so stamp runs are addressable")
