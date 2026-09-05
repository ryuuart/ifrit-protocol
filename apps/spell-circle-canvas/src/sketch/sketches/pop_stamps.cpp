// pop_stamps.cpp — THE POP CHAINS THAT FORM BODIES.
// =============================================================================
// A pop chain is a value: a list of edits over points, cooked only when
// something asks for the result. What it is cooked INTO is the sink at
// the end, and the sinks are where a chain stops being points and starts
// being a body. Four of them, over one ring of ten control points.
//
//   .sweep()      a tube. The chain's cooked points ARE the rail, so a
//                 noise op before the sweep is a wobbly pipe and not a
//                 wobbly texture on a straight one.
//   .stamps()     one small mesh at every point. `.atlas(2, 2)` picks a
//                 cell of the shared texture per point, `.vary()` sizes
//                 them apart, `.fade()` runs a colour along the chain
//                 and `.lookAt()` turns every stamp to the camera — so
//                 one call scatters variety rather than repeating one
//                 sprite.
//   a PROFILE     any outline sweeps: a star cross-section on a
//                 smoothed, noised ring is extrusion spoken as a chain.
//   pop::on(mesh) a chain SEEDED FROM A FORMED BODY — glints scattered
//                 over the crown's own surface. The sink's output is a
//                 legal source, which is what keeps the language closed.
//
// The ribbon at the bottom is `pop::cookSweep` on a WINDOW of the ring:
// `.window()` reaches into the middle of the chain, the smooth heals the
// kinks the noise put there, and the band twists — so it draws with the
// backface cull off, which is the honest answer for a surface with two
// sides on a painter with no depth buffer.
//
// EDIT THESE FIRST
//   the .count() values   — how densely each chain resolves.
//   .noise(amplitude, frequency) — the wobble, on the rail not the skin.
//   .smooth(strength, iterations) — drop it and the star sweep kinks.

#include <include/core/SkMatrix.h>
#include <sigilgeometry/kit/Sections.h>
#include <sigilgeometry/kit/Silhouettes.h>
#include <sigilcompose/texture/Texture.h>
#include <sigilgeometry/kit/Solids.h>
#include <sigilgeometry/mesh/Mesh.h>
#include <sigilgeometry/mesh/camera/Camera.h>
#include <sigilgeometry/mesh/curve/Curve.h>
#include <sigilgeometry/mesh/pop/Pop.h>
#include <sigilgeometry/mesh/render/Painter.h>
#include <sigilsketch/canvas/Sketch.h>

#include <cmath>
#include <memory>
#include <vector>

namespace sketch = sigil::sketch;
namespace shapes = sigil::geometry::shapes;
namespace sections = sigil::geometry::sections;

using namespace sigil::compose;
namespace mesh = sigil::geometry::mesh;
namespace camera = sigil::geometry::mesh::camera;
namespace pop = sigil::geometry::mesh::pop;
namespace render = sigil::geometry::mesh::render;

namespace {

constexpr SkSize kCanvas = {1240, 720};

/** A 2 x 2 sprite sheet, described rather than painted: four motifs in
 *  four cells, which is exactly what `.atlas(2, 2)` indexes. */
Element atlasSheet(float cell) {
  const auto motif = [&](float col, float row, auto shape, SkColor4f color) {
    return box()
        .width(cell)
        .height(cell)
        .absolute()
        .inset(col * cell, row * cell, cell - col * cell, cell - row * cell)
        .padding(cell * 0.12f)
        .child(box().grow().shape(shape).fill(Fill::color(color)));
  };
  return stack()
      .width(cell * 2)
      .height(cell * 2)
      .child(motif(0, 0, shapes::circle(), {0.4f, 0.85f, 1.0f, 1}))
      .child(motif(1, 0, shapes::annulus(0.62f), {1.0f, 0.6f, 0.3f, 1}))
      .child(motif(0, 1, shapes::polygon(4), {0.6f, 1.0f, 0.6f, 1}))
      .child(motif(1, 1, shapes::star(4, 0.35f), {1.0f, 0.8f, 0.3f, 1}));
}

std::vector<glm::vec3> ringPoints() {
  std::vector<glm::vec3> ring;
  for (int i = 0; i < 10; ++i) {
    const float a = (float)i / 10.0f * 6.2831853f;
    ring.emplace_back(std::cos(a) * 230, std::sin(a * 2.0f) * 60,
                      std::sin(a) * 230);
  }
  return ring;
}

}  // namespace

struct PopStamps final : sketch::Sketch {
  sk_sp<SkImage> atlas;
  mesh::Mesh tube, plates, crown, glints, ribbon;
  glm::mat4 crownPlace{1.0f};

  /** An element tree painted to pixels, square, at a stated side. The
   *  session owns the scene the picture was taken from and lets it go
   *  when the body declares again, so the sketch holds an image and
   *  nothing else. */
  static sk_sp<SkImage> bake(sketch::SketchContext& ctx, const Element& tree,
                             int side) {
    const std::shared_ptr<TextureScene> scene = ctx.textureScene({side, side});
    if (!scene) return nullptr;
    scene->render(tree);
    return scene->image();
  }

  void draw(SkCanvas& canvas) const {
    camera::Camera view;
    view.eye = {0, 260, 980};
    view.target = {0, 20, 0};
    view.fovYDeg = 42;

    render::MeshStyle steel;
    steel.baseColor = {0.62f, 0.7f, 0.82f, 1};
    steel.specular = 0.8f;
    steel.shininess = 48;
    render::drawMesh(canvas, tube, camera::place({-330, 40, 0}, 24, -10), view,
                     kCanvas, steel);

    render::MeshStyle sprites;
    sprites.baseColor = {1, 1, 1, 1};
    sprites.ambient = {0.85f, 0.85f, 0.9f, 1};
    sprites.specular = 0;
    sprites.texture = atlas;
    render::drawMesh(canvas, plates, camera::place({330, 60, 0}, -16), view,
                     kCanvas, sprites);

    render::MeshStyle gold = steel;
    gold.baseColor = {0.95f, 0.72f, 0.3f, 1};
    render::drawMesh(canvas, crown, crownPlace, view, kCanvas, gold);

    render::MeshStyle glint;
    glint.baseColor = {1.0f, 0.95f, 0.8f, 1};
    glint.ambient = {0.85f, 0.8f, 0.7f, 1};
    glint.specular = 0;
    render::drawMesh(canvas, glints, crownPlace, view, kCanvas, glint);

    render::MeshStyle jade = steel;
    jade.baseColor = {0.4f, 0.85f, 0.6f, 1};
    jade.backfaceCull = false;
    render::drawMesh(canvas, ribbon, camera::place({0, -60, 140}, 0, 14), view,
                     kCanvas, jade);
  }

  void setup(sketch::SketchContext& ctx) override {
    ctx.canvas(kCanvas.width(), kCanvas.height());
    ctx.background({0.051f, 0.051f, 0.075f, 1});
    ctx.captureAt(1.0);

    atlas = bake(ctx, atlasSheet(128), 256);

    const std::vector<glm::vec3> ring = ringPoints();
    const glm::vec3 eye = {0, 260, 980};

    tube = pop::on(ring)
               .count(220)
               .noise(26, 0.004f)
               .sweep(sections::circle(14), true,
                      {.segments = 160, .scale = 11});

    plates = pop::on(ring)
                 .count(900)
                 .spread(26)
                 .vary(0.6f)
                 .fade({1.0f, 0.7f, 0.55f, 1}, {0.6f, 0.85f, 1.0f, 1})
                 .atlas(2, 2)
                 .lookAt(eye)
                 .stamps(mesh::quad(11, 11));

    crown = pop::on(ring)
                .count(140)
                .noise(20, 0.004f)
                .smooth(0.5f, 2)
                .sweep(pop::profile::fromPath(
                           shapes::star(5, 14.0f / 30.0f)
                               .path({60, 60})
                               .makeTransform(SkMatrix::Translate(-30, -30))),
                       true,
                       {.segments = 160,
                        .normals = pop::SweepOptions::Normals::Geometric});
    crownPlace = camera::place({0, 255, -140}, 14, -10, 0, 0.85f);
    glints = pop::on(crown, 600).jitter(1.5f).stamps(mesh::quad(3, 3));

    ribbon = pop::cookSweep(pop::on(ring)
                                .count(120)
                                .window(0.5f, 0.5f)
                                .noise(16, 0.004f)
                                .smooth(0.6f, 3),
                            sections::line(), false,
                            {.segments = 120,
                             .scale = 42,
                             .normals = pop::SweepOptions::Normals::Frame});

    ctx.composer.render(custom([this](SkCanvas& canvas, const PaintContext&) {
                          draw(canvas);
                        }).inset(0));
  }
};

SIGIL_SKETCH(PopStamps, "Kit · API",
             "the pop sinks — sweep, atlas stamps, an outline profile, a "
             "chain seeded from a formed body, and a windowed ribbon")
