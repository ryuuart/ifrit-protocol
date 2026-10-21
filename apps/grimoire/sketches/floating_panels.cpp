/** @file
 * floating_panels — 2D CONTENT STOOD UP IN SPACE, flat and curved.
 *
 * Two bodies answer "put this picture over there", and they differ in one
 * thing only:
 *
 *   a QUAD wearing the picture — a flat rect at a placement, the image
 *     mapped corner to corner.
 *   a CYLINDER PANEL wearing the same picture — a CURVED sheet, where the
 *     picture bends because the surface does.
 *
 * Both wear the picture the same way: a texture in the base-colour slot
 * of a surface that is its own light, because a screen emits and a light
 * term on it would read as a smear across the curve.
 *
 * WHAT IS ON THE PANELS is described in compose and painted once through
 * `ctx.texture()`, the host's own door for a compose tree painted into a
 * texture: a card is a column of boxes, a gauge is two filled
 * sectors, a bar row is boxes of differing height. Nothing here paints a
 * rounded rect by hand — a panel's content is an element tree like any
 * other, and the only thing 3D about it is where it ends up.
 *
 * THE COCKPIT IS ONE WORLD FRAME baked once into the page's picture
 * through `ctx.bakeScene3D`, on the device the process draws on; the sheet
 * is unavailable, saying why, where there is none.
 *
 * The floor is `mesh::grid` over a flat function. It is not decoration:
 * without a ground plane the cards have no horizon and the camera angle
 * reads as arbitrary.
 *
 * EDIT THESE FIRST
 *   the three camera::place calls — the cockpit's splay.
 *   cylinderPanel's radius        — larger is flatter; at a large enough
 *                                   value the curved screen and a flat
 *                                   panel are the same picture.
 */

// TAGS: Geometry/Meshes

#include <include/core/SkCanvas.h>
#include <include/core/SkImage.h>
#include <include/core/SkSamplingOptions.h>
#include <sigilcompose/texture/Texture.h>
#include <sigildraw/Pen.h>
#include <sigilgeometry/kit/Solids.h>
#include <sigilgeometry/mesh/Mesh.h>
#include <sigilgeometry/mesh/camera/Camera.h>
#include <sigilmaterial/color/Color.h>
#include <sigilmaterial/surface/Surface.h>
#include <sigilmaterial/texture/Texture.h>
#include <sigilmedia/advanced/Skia.h>
#include <sigilsketch/kit/Kit.h>
#include <sigilsketch/scene2d/Scene2D.h>
#include <sigilsketch/scene3d/Scene3D.h>
#include <sigilworld/World.h>

#include <algorithm>
#include <cmath>
#include <memory>
#include <ranges>
#include <string>
#include <utility>
#include <vector>

namespace material = sigil::material;
namespace sketch = sigil::sketch;
namespace world = sigil::world;

using namespace sigil::compose;
namespace mesh = sigil::geometry::mesh;
namespace camera = sigil::geometry::mesh::camera;

namespace {

constexpr SkSize kCanvas = {1240, 720};

/** A screen's surface: @p picture in the base-colour slot of a surface
 *  that is its own light, at @p opacity; @p layered writes no depth, so
 *  the screen is laid among the other such bodies back to front. */
material::Material screen(const material::Texture& picture, float opacity,
                          bool layered = false) {
  material::Material surface = material::surface::unlit(
      {.baseColor = {1, 1, 1, opacity}, .depthWrite = layered ? 0.0f : -1.0f});
  if (picture.valid())
    surface.slot(material::surface::kBaseColorSlot, picture);
  return surface;
}

/** A readout card, as an element tree: a header pill, a stack of tick
 *  rows, a two-sector gauge and a bar row. Sized in px because it is
 *  baked to a picture of a stated size rather than laid out in a
 *  window. */
Element card(float w, float h, material::Color accent) {
  const material::Color ink = {1, 1, 1, 0.25f};
  const float gauge = 108;
  return stack()
      .width(w)
      .height(h)
      .fill(Fill::color({0.055f, 0.071f, 0.125f, 0.9f}))
      .children(
          {box().column().gap(18).absolute().inset(16).children(
               {// the header pill
                box().width(w - 32).height(14).borderRadius({7}).fill(
                    Fill::color({accent.r, accent.g, accent.b, 0.9f})),
                // the tick rows, each shorter than the one above it
                box().column().gap(14).children(
                    {each(4,
                          [w, ink](int i) {
                            return box()
                                .width(w - 60 - (float)i * 40)
                                .height(8)
                                .borderRadius({4})
                                .fill(Fill::color(ink));
                          })}),
                // the bar row, standing on its own foot
                box()
                    .row()
                    .gap(6)
                    .alignItems(Align::End)
                    .children({each(
                        std::views::iota(0, 14),
                        [accent](int i) {
                          const float t = (float)i / 13.0f;
                          return box()
                              .width(10)
                              .height(10 +
                                      34.0f * (0.5f + 0.5f * std::sin(t * 9.0f +
                                                                      1.7f)))
                              .borderRadius({2})
                              .fill(Fill::color(
                                  {accent.r, accent.g, accent.b, 0.85f}));
                        })})}),
           sketch::kit::gauge({.fraction = 200.0f / 280.0f,
                               .diameter = gauge,
                               .thickness = gauge * 0.5f * (1 - 0.72f),
                               .startDegrees = 130,
                               .sweepDegrees = 280,
                               .track = Fill::color({1, 1, 1, 0.15f}),
                               .bar = Fill::color(accent)})
               .absolute()
               .inset(h - gauge - 16, 16, 16, w - gauge - 16)});
}

}  // namespace

struct FloatingPanels {
  /** The cockpit is a frame baked on the device, so the sheet needs one
   *  and is unavailable, saying so, where none is installed. */
  static constexpr bool needsDevice = true;
  static bool available(std::string* why) {
    return sketch::runtimeAvailable(why);
  }

  void setup(sketch::Scene2DContext& ctx) {
    const sketch::kit::Provide presentation(
        sketch::kit::featureTheme(sketch::kit::Density::Spacious));
    sketch::kit::stage(ctx,
                       {.size = SkSize::Make(kCanvas.width(), kCanvas.height()),
                        .captureAt = 1.0,
                        .background = sketch::kit::theme().palette.ground});

    const material::Texture cardA =
        ctx.texture(card(360, 240, {0.2f, 0.85f, 1.0f, 1}), {360, 240});
    const material::Texture cardB =
        ctx.texture(card(360, 240, {1.0f, 0.6f, 0.25f, 1}), {360, 240});
    const material::Texture cardC =
        ctx.texture(card(360, 240, {0.7f, 0.45f, 1.0f, 1}), {360, 240});
    const material::Texture wide =
        ctx.texture(card(720, 200, {0.3f, 1.0f, 0.6f, 1}), {720, 200});

    const mesh::Mesh floor = mesh::grid(24, 24, [](float u, float v) {
      return glm::vec3{(u - 0.5f) * 1400, -170, (v - 0.5f) * 1400};
    });
    const camera::Camera view{
        .eye = {0, 80, 900}, .target = {0, 0, 0}, .fovYDegrees = 38};

    // A sheet has one facing, and the floor is seen from the side its
    // winding calls the back, so both of its sides are drawn. The floor and
    // the curved screen write no depth, so they are laid back to front:
    // the screen dips below the floor's plane and is still seen whole, as
    // a screen standing in front of a floor reads.
    const world::Frame cockpit(world::group(
        {world::mesh(floor)
             .key("floor")
             .backface(material::Backface::Visible)
             .fill(material::surface::unlit(
                 {.baseColor = {0.137f, 0.169f, 0.227f, 1}, .depthWrite = 0})),
         world::mesh(mesh::quad(360, 240))
             .key("card.left")
             .transform(camera::place({-350, 120, -80}, 34))
             .fill(screen(cardA, 0.95f)),
         world::mesh(mesh::quad(360, 240))
             .key("card.middle")
             .transform(camera::place({0, 130, 30}, 0, -4))
             .fill(screen(cardB, 0.98f)),
         world::mesh(mesh::quad(360, 240))
             .key("card.right")
             .transform(camera::place({350, 110, -80}, -34))
             .fill(screen(cardC, 0.95f)),
         // The curved sheet: the same kind of picture, mapped per
         // triangle.
         world::mesh(mesh::cylinderPanel(680, 190, 420, 48, 10))
             .key("screen")
             .transform(camera::place({0, -160, 60}, 0, 10))
             .fill(screen(wide, 1, /*layered=*/true))}));
    const sk_sp<SkImage> picture =
        ctx.bakeScene3D(cockpit, {(int)kCanvas.width(), (int)kCanvas.height()},
                        {.camera = view});

    ctx.composer.render(sketch::kit::page(
        {.title = "One composition, flat and curved",
         .subtitle = "The same retained card becomes a "
                     "plane or a cylindrical screen.",
         .footer = "Three flat cards above · one "
                   "picture mapped over a curved mesh below"},
        custom("floating.panels", [picture](sigil::draw::Pen& pen,
                                            const PaintContext& paint) {
          if (!picture) return;
          const float scale = std::min(paint.size.x / kCanvas.width(),
                                       paint.size.y / kCanvas.height());
          const float width = kCanvas.width() * scale;
          const float height = kCanvas.height() * scale;
          pen.canvas()->drawImageRect(
              picture,
              SkRect::MakeXYWH((paint.size.x - width) / 2,
                               (paint.size.y - height) / 2, width, height),
              SkSamplingOptions(SkFilterMode::kLinear));
        })));
  }
};

SIGIL_SKETCH(FloatingPanels, "Kit · API",
             "a textured quad and a textured cylinderPanel in one world "
             "frame — the same composed card flat in space and bent around "
             "a curve")
