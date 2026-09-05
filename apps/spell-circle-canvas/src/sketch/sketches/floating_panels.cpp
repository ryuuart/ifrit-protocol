/** @file
 * floating_panels — 2D CONTENT STOOD UP IN SPACE, flat and curved.
 *
 * Two entry points answer "put this picture over there", and they differ
 * in one thing only:
 *
 *   render::drawImagePanel — a FLAT rect at a placement. The image is
 *     mapped corner to corner; the perspective is a matrix concat, so a
 *     panel costs one draw whatever is on it.
 *   MeshStyle::texture on mesh::cylinderPanel — the same picture on a
 *     CURVED sheet. Now the mapping is per-triangle, and the picture
 *     bends because the surface does.
 *
 * WHAT IS ON THE PANELS is described in compose and baked once through
 * `ctx.textureScene()`, the host's own door for a compose scene painted
 * into a texture: a card is a column of boxes, a gauge is two filled
 * sectors, a bar row is boxes of differing height. Nothing here paints a
 * rounded rect by hand — a panel's content is an element tree like any
 * other, and the only thing 3D about it is where it ends up. The session
 * keeps every scene it hands out, so the sketch holds an image and
 * nothing else.
 *
 * WHO DRAWS THE MESHES is one value, `painter()`, and it is the process's
 * own: `sketch::painterRuntime()` is the device executor where a host
 * brought a device up and the CPU one where it did not, so the line is
 * written once and both tiers are correct. Run this file plainly and the
 * meshes go through the CPU executor, which sorts triangles back to front
 * and antialiases their edges; run it with `--gpu` and the same call goes
 * through `diligent::painterRuntime`, which depth-tests them and does
 * not. The two draw the same picture and not the same bytes, and where
 * they disagree is exactly the two MESH draws: the floor plane and the
 * curved screen, whose triangle edges and texture sampling are each
 * executor's own. The three flat cards are identical on both, because a
 * panel is not a mesh draw at all — the perspective is a matrix concat
 * and the image lands on whatever surface the canvas already is.
 *
 * The floor is `mesh::grid` over a flat function, lit at very low alpha.
 * It is not decoration: without a ground plane the cards have no horizon
 * and the camera angle reads as arbitrary.
 *
 * EDIT THESE FIRST
 *   the three camera::place calls — the cockpit's splay.
 *   cylinderPanel's radius        — larger is flatter; at a large enough
 *                                   value the curved screen and a flat
 *                                   panel are the same picture.
 */

#include <sigilcompose/texture/Texture.h>
#include <sigilgeometry/kit/Silhouettes.h>
#include <sigilgeometry/kit/Solids.h>
#include <sigilgeometry/mesh/Mesh.h>
#include <sigilgeometry/mesh/camera/Camera.h>
#include <sigilgeometry/mesh/render/Painter.h>
#include <sigilgeometry/mesh/render/Runtime.h>
#include <sigilsketch/canvas/Sketch.h>
#include <sigilsketch/kit/Kit.h>

#include <cmath>
#include <memory>
#include <vector>

namespace sketch = sigil::sketch;
namespace shapes = sigil::geometry::shapes;

using namespace sigil::compose;
namespace mesh = sigil::geometry::mesh;
namespace camera = sigil::geometry::mesh::camera;
namespace render = sigil::geometry::mesh::render;

namespace {

constexpr SkSize kCanvas = {1240, 720};

/** WHO DRAWS THE MESHES: the process's own painter, whatever it is. A
 *  host that brought a device up installed the device executor here; a
 *  host that did not hands back the CPU one, which is what a plate is
 *  hashed from. Every style, mesh and camera below is the same either
 *  way — the runtime is the only thing that differs. */
render::Runtime painter() { return sketch::painterRuntime(); }

/** A readout card, as an element tree: a header pill, a stack of tick
 *  rows, a two-sector gauge and a bar row. Sized in px because it is
 *  baked to a picture of a stated size rather than laid out in a
 *  window. */
Element card(float w, float h, SkColor4f accent) {
  const SkColor4f ink = {1, 1, 1, 0.25f};
  auto rows = box().column().gap(14);
  for (int i = 0; i < 4; ++i)
    rows.child(box()
                   .width(w - 60 - (float)i * 40)
                   .height(8)
                   .corners({4})
                   .fill(Fill::color(ink)));

  auto bars = box().row().gap(6).alignItems(Align::End);
  for (int i = 0; i < 14; ++i) {
    const float t = (float)i / 13.0f;
    bars.child(
        box()
            .width(10)
            .height(10 + 34.0f * (0.5f + 0.5f * std::sin(t * 9.0f + 1.7f)))
            .corners({2})
            .fill(Fill::color({accent.fR, accent.fG, accent.fB, 0.85f})));
  }

  const float gauge = 108;
  return stack()
      .width(w)
      .height(h)
      .fill(Fill::color({0.055f, 0.071f, 0.125f, 0.9f}))
      .child(box()
                 .column()
                 .gap(18)
                 .absolute()
                 .inset(16, 16, 16, 16)
                 .child(box().width(w - 32).height(14).corners({7}).fill(
                     Fill::color({accent.fR, accent.fG, accent.fB, 0.9f})))
                 .child(std::move(rows))
                 .child(std::move(bars)))
      .child(sketch::kit::gauge({.fraction = 200.0f / 280.0f,
                                 .diameter = gauge,
                                 .thickness = gauge * 0.5f * (1 - 0.72f),
                                 .startDeg = 130,
                                 .sweepDeg = 280,
                                 .track = Fill::color({1, 1, 1, 0.15f}),
                                 .bar = Fill::color(accent)})
                 .absolute()
                 .inset(w - gauge - 16, h - gauge - 16, 16, 16));
}

}  // namespace

struct FloatingPanels final : sketch::Sketch {
  sk_sp<SkImage> cardA, cardB, cardC, screen;
  mesh::Mesh floor, curved;

  /** An element tree painted to pixels at a stated size. The session owns
   *  the scene; what comes back here is the image. */
  static sk_sp<SkImage> bake(sketch::SketchContext& ctx, const Element& tree,
                             int w, int h) {
    const std::shared_ptr<TextureScene> scene = ctx.textureScene({w, h});
    if (!scene) return nullptr;
    scene->render(tree);
    return scene->image();
  }

  void draw(SkCanvas& canvas) const {
    camera::Camera view;
    view.eye = {0, 80, 900};
    view.target = {0, 0, 0};
    view.fovYDeg = 38;

    render::MeshStyle ground;
    ground.baseColor = {0.16f, 0.3f, 0.5f, 0.5f};
    ground.ambient = {0.45f, 0.5f, 0.62f, 1};
    ground.specular = 0;
    // A sheet has one facing, and this one is being looked at from the
    // side the winding calls the back. Culling it would leave the frame
    // with no ground at all.
    ground.backfaceCull = false;
    ground.runtime = painter();
    render::drawMesh(canvas, floor, glm::mat4(1.0f), view, kCanvas, ground);

    render::drawImagePanel(canvas, cardA, 360, 240,
                           camera::place({-350, 120, -80}, 34), view, kCanvas,
                           0.95f, painter());
    render::drawImagePanel(canvas, cardB, 360, 240,
                           camera::place({0, 130, 30}, 0, -4), view, kCanvas,
                           0.98f, painter());
    render::drawImagePanel(canvas, cardC, 360, 240,
                           camera::place({350, 110, -80}, -34), view, kCanvas,
                           0.95f, painter());

    // The curved sheet: the same kind of picture, mapped per triangle.
    // Unlit on purpose — a screen emits, and a light term on it would
    // read as a smear across the curve.
    render::MeshStyle emissive;
    emissive.texture = screen;
    emissive.baseColor = {1, 1, 1, 1};
    emissive.ambient = {0.9f, 0.9f, 0.9f, 1};
    emissive.lights = {};
    emissive.specular = 0;
    emissive.runtime = painter();
    render::drawMesh(canvas, curved, camera::place({0, -160, 60}, 0, 10), view,
                     kCanvas, emissive);
  }

  void setup(sketch::SketchContext& ctx) override {
    sketch::kit::stage(ctx,
                       {.size = SkSize::Make(kCanvas.width(), kCanvas.height()),
                        .captureAt = 1.0,
                        .background = SkColor4f{0.027f, 0.027f, 0.047f, 1}});

    cardA = bake(ctx, card(360, 240, {0.2f, 0.85f, 1.0f, 1}), 360, 240);
    cardB = bake(ctx, card(360, 240, {1.0f, 0.6f, 0.25f, 1}), 360, 240);
    cardC = bake(ctx, card(360, 240, {0.7f, 0.45f, 1.0f, 1}), 360, 240);
    screen = bake(ctx, card(720, 200, {0.3f, 1.0f, 0.6f, 1}), 720, 200);
    floor = mesh::grid(24, 24, [](float u, float v) -> glm::vec3 {
      return {(u - 0.5f) * 1400, -170, (v - 0.5f) * 1400};
    });
    curved = mesh::cylinderPanel(680, 190, 420, 48, 10);

    ctx.composer.render(custom([this](SkCanvas& canvas, const PaintContext&) {
                          draw(canvas);
                        }).inset(0));
  }
};

SIGIL_SKETCH(FloatingPanels, "Kit \xc2\xb7 API",
             "render::drawImagePanel and a textured cylinderPanel — the "
             "same composed card flat in space and bent around a curve")
