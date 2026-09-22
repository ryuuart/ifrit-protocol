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

// TAGS: Geometry/Points

#include <include/core/SkMatrix.h>
#include <sigilcompose/kit/Frame.h>
#include <sigilcompose/kit/Specimen.h>
#include <sigilcompose/texture/Texture.h>
#include <sigilgeometry/kit/Sections.h>
#include <sigilgeometry/kit/Silhouettes.h>
#include <sigilgeometry/kit/Solids.h>
#include <sigilgeometry/mesh/Mesh.h>
#include <sigilgeometry/mesh/camera/Camera.h>
#include <sigilgeometry/mesh/curve/Curve.h>
#include <sigilgeometry/mesh/pop/Pop.h>
#include <sigilgeometry/mesh/render/Painter.h>
#include <sigilmaterial/color/Color.h>
#include <sigilsketch/canvas/Sketch.h>
#include <sigilsketch/kit/Kit.h>

#include <array>
#include <cmath>
#include <memory>
#include <utility>
#include <vector>

namespace material = sigil::material;
namespace sketch = sigil::sketch;
namespace shapes = sigil::geometry::shapes;
namespace sections = sigil::geometry::sections;

using namespace sigil::compose;
namespace mesh = sigil::geometry::mesh;
namespace camera = sigil::geometry::mesh::camera;
namespace pop = sigil::geometry::mesh::pop;
namespace render = sigil::geometry::mesh::render;

namespace {

constexpr SkSize kCanvas = {1240, 900};

/** A 2 x 2 sprite sheet, described rather than painted: four motifs in
 *  four cells, which is exactly what `.atlas(2, 2)` indexes — the cell a
 *  motif lands in is its own place in the run. */
Element atlasSheet(float cell) {
  const std::array<std::pair<Shape, material::Color>, 4> motifs{
      {{shapes::circle(), {0.4f, 0.85f, 1.0f, 1}},
       {shapes::annulus(0.62f), {1.0f, 0.6f, 0.3f, 1}},
       {shapes::polygon(4), {0.6f, 1.0f, 0.6f, 1}},
       {shapes::star(4, 0.35f), {1.0f, 0.8f, 0.3f, 1}}}};
  return stack().width(cell * 2).height(cell * 2).children(each(
      motifs, [cell](const std::pair<Shape, material::Color>& motif, std::size_t i) {
        return kit::at((float)(i % 2) * cell, (float)(i / 2) * cell, cell, cell)
            .padding(cell * 0.12f)
            .children({box()
                           .flexGrow()
                           .shape(motif.first)
                           .fill(Fill::color(motif.second))});
      }));
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

struct PopStamps {
  sk_sp<SkImage> atlas;
  mesh::Mesh tube, plates, crown, glints, ribbon;

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

  Element figure(const char* key, int output) const {
    return sketch::kit::well(
        {.width = 568, .height = 238},
        custom(key, [this, output](SkCanvas& canvas, const PaintContext& pc) {
          const camera::Camera view{
              .eye = {0, 260, 980}, .target = {0, 20, 0}, .fovYDeg = 42};
          render::MeshStyle style;
          style.baseColor = {0.62f, 0.7f, 0.82f, 1};
          style.specular = 0.8f;
          style.shininess = 48;
          if (output == 0) {
            render::drawMesh(canvas, tube, camera::place({}, 24, -10), view,
                             pc.size, style);
          } else if (output == 1) {
            style.baseColor = {1, 1, 1, 1};
            style.ambient = {0.85f, 0.85f, 0.9f, 1};
            style.specular = 0;
            style.texture = atlas;
            render::drawMesh(canvas, plates, camera::place({}, -16), view,
                             pc.size, style);
          } else if (output == 2) {
            const glm::mat4 placed = camera::place({}, 14, -10, 0, 0.85f);
            style.baseColor = {0.95f, 0.72f, 0.3f, 1};
            render::drawMesh(canvas, crown, placed, view, pc.size, style);
            style.baseColor = {1.0f, 0.95f, 0.8f, 1};
            style.ambient = {0.85f, 0.8f, 0.7f, 1};
            style.specular = 0;
            render::drawMesh(canvas, glints, placed, view, pc.size, style);
          } else {
            style.baseColor = {0.4f, 0.85f, 0.6f, 1};
            style.backfaceCull = false;
            render::drawMesh(canvas, ribbon, camera::place({}, 0, 14), view,
                             pc.size, style);
          }
        }));
  }

  void setup(sketch::SketchContext& ctx) {
    const sketch::kit::Provide look(sketch::kit::studyTheme());
    sketch::kit::stage(ctx,
                       {.size = SkSize::Make(kCanvas.width(), kCanvas.height()),
                        .captureAt = 1.0,
                        .background = material::Color{0.051f, 0.051f, 0.075f, 1}});

    atlas = bake(ctx, atlasSheet(128), 256);

    const std::vector<glm::vec3> ring = ringPoints();
    const glm::vec3 eye = {0, 260, 980};

    tube =
        pop::on(ring)
            .count(220)
            .noise(26, 0.004f)
            .sweep(sections::circle(14), true, {.segments = 160, .scale = 11});

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

    ctx.composer.render(sketch::kit::page(
        {.title = "One path, four surfaces",
         .subtitle = "A closed ring of ten control points · the point chain "
                     "stays editable until a sink forms the body",
         .footer = "A formed body can become a source again: the crown carries "
                   "600 glints sampled from its own surface. The open ribbon "
                   "is drawn on both sides."},
        box().column().gap(28).children(
            {sketch::kit::comparison(
                 {.cases = {{.title = "SWEEP A CIRCLE",
                             .control = "220 points → noise → circle profile",
                             .figure = figure("sink.tube", 0),
                             .note =
                                 kit::formatted("A displaced rail makes a "
                                                "wobbly tube. %zu triangles.",
                                                tube.triangleCount())},
                            {.title = "STAMP AN ATLAS",
                             .control = "900 points → spread → atlas(2, 2)",
                             .figure = figure("sink.atlas", 1),
                             .note = "Four texture motifs, varied in size and "
                                     "turned toward the camera."}},
                  .measure = 1160,
                  .gap = 24}),
             sketch::kit::comparison(
                 {.cases =
                      {{.title = "SWEEP AN OUTLINE",
                        .control = "140 points → noise → smooth → star profile",
                        .figure = figure("sink.profile", 2),
                        .note = "A star cross-section makes the crown; its "
                                "surface seeds a second point chain."},
                       {.title = "OPEN A WINDOW",
                        .control = "120 points → half-window → line profile",
                        .figure = figure("sink.ribbon", 3),
                        .note = "An open profile and a partial rail form a "
                                "ribbon instead of a closed pipe."}},
                  .measure = 1160,
                  .gap = 24})})));
  }
};

SIGIL_SKETCH(PopStamps, "Kit · API",
             "the pop sinks — sweep, atlas stamps, an outline profile, a "
             "chain seeded from a formed body, and a windowed ribbon")
