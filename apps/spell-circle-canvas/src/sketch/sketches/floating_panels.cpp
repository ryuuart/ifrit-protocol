// floating_panels.cpp — 2D CONTENT STOOD UP IN SPACE, flat and curved.
// =============================================================================
// Two entry points answer "put this picture over there", and they differ
// in one thing only:
//
//   render::drawImagePanel — a FLAT rect at a placement. The image is
//     mapped corner to corner; the perspective is a matrix concat, so a
//     panel costs one draw whatever is on it.
//   MeshStyle::texture on mesh::cylinderPanel — the same picture on a
//     CURVED sheet. Now the mapping is per-triangle, and the picture
//     bends because the surface does.
//
// WHAT IS ON THE PANELS is described in compose and baked once: a card
// is a column of boxes, a gauge is two filled sectors, a bar row is
// boxes of differing height. Nothing here paints a rounded rect by hand
// — a panel's content is an element tree like any other, and the only
// thing 3D about it is where it ends up.
//
// The floor is `mesh::grid` over a flat function, lit at very low alpha.
// It is not decoration: without a ground plane the cards have no
// horizon and the camera angle reads as arbitrary.
//
// EDIT THESE FIRST
//   the three camera::place calls — the cockpit's splay.
//   cylinderPanel's radius        — larger is flatter; at a large enough
//                                   value the curved screen and a flat
//                                   panel are the same picture.

#include <sigilcompose/shape/Shapes.h>
#include <sigilcompose/texture/Texture.h>
#include <sigilgeometry/mesh/Mesh.h>
#include <sigilgeometry/mesh/camera/Camera.h>
#include <sigilgeometry/mesh/render/Painter.h>
#include <sigilsketch/canvas/Sketch.h>

#include <cmath>
#include <memory>
#include <vector>

namespace sketch = sigil::sketch;

using namespace sigil::compose;
namespace mesh = sigil::geometry::mesh;
namespace camera = sigil::geometry::mesh::camera;
namespace render = sigil::geometry::mesh::render;

namespace {

constexpr SkSize kCanvas = {1240, 720};

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
      .child(box()
                 .width(gauge)
                 .height(gauge)
                 .absolute()
                 .inset(w - gauge - 16, h - gauge - 16, 16, 16)
                 .shape(shapes::sector(130, 280, 0.72f))
                 .fill(Fill::color({1, 1, 1, 0.15f})))
      .child(box()
                 .width(gauge)
                 .height(gauge)
                 .absolute()
                 .inset(w - gauge - 16, h - gauge - 16, 16, 16)
                 .shape(shapes::sector(130, 200, 0.72f))
                 .fill(Fill::color(accent)));
}

}  // namespace

struct FloatingPanels final : sketch::Sketch {
  sk_sp<SkImage> cardA, cardB, cardC, screen;
  mesh::Mesh floor, curved;

  /** The scenes behind the pictures. A texture scene owns the surface
   *  its image was taken from, so it is held for as long as the image
   *  is. */
  std::vector<std::shared_ptr<TextureScene>> scenes;

  /** An element tree painted to pixels at a stated size. */
  sk_sp<SkImage> bake(const Element& tree, sigil::weave::FontContext& f, int w,
                      int h) {
    const auto scene = TextureScene::make({w, h}, f);
    scene->render(tree);
    scenes.push_back(scene);
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
    render::drawMesh(canvas, floor, glm::mat4(1.0f), view, kCanvas, ground);

    render::drawImagePanel(canvas, cardA, 360, 240,
                           camera::place({-350, 120, -80}, 34), view, kCanvas,
                           0.95f);
    render::drawImagePanel(canvas, cardB, 360, 240,
                           camera::place({0, 130, 30}, 0, -4), view, kCanvas,
                           0.98f);
    render::drawImagePanel(canvas, cardC, 360, 240,
                           camera::place({350, 110, -80}, -34), view, kCanvas,
                           0.95f);

    // The curved sheet: the same kind of picture, mapped per triangle.
    // Unlit on purpose — a screen is its own light, and a light term on
    // it would read as a smear across the curve. One field says that:
    // no ambient under the picture, no emitter, specular or rim over it.
    render::MeshStyle emissive;
    emissive.texture = screen;
    emissive.baseColor = {1, 1, 1, 1};
    emissive.lit = false;
    render::drawMesh(canvas, curved, camera::place({0, -160, 60}, 0, 10), view,
                     kCanvas, emissive);
  }

  void setup(sketch::SketchContext& ctx) override {
    ctx.canvas(kCanvas.width(), kCanvas.height());
    ctx.background({0.027f, 0.027f, 0.047f, 1});
    ctx.captureAt(1.0);

    if (ctx.fonts) {
      cardA =
          bake(card(360, 240, {0.2f, 0.85f, 1.0f, 1}), *ctx.fonts, 360, 240);
      cardB =
          bake(card(360, 240, {1.0f, 0.6f, 0.25f, 1}), *ctx.fonts, 360, 240);
      cardC =
          bake(card(360, 240, {0.7f, 0.45f, 1.0f, 1}), *ctx.fonts, 360, 240);
      screen =
          bake(card(720, 200, {0.3f, 1.0f, 0.6f, 1}), *ctx.fonts, 720, 200);
    }
    floor = mesh::grid(24, 24, [](float u, float v) -> glm::vec3 {
      return {(u - 0.5f) * 1400, -170, (v - 0.5f) * 1400};
    });
    curved = mesh::cylinderPanel(680, 190, 420, 48, 10);

    ctx.composer.render(custom([this](SkCanvas& canvas, const PaintContext&) {
                          draw(canvas);
                        }).inset(0));
  }
};

SIGIL_SKETCH(FloatingPanels, "Kit · API",
             "render::drawImagePanel and a textured cylinderPanel — the "
             "same composed card flat in space and bent around a curve")
