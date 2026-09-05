/** @file
 * painter_gpu — the mesh painter as a value, and the two executors that
 * stand behind it.
 *
 * `render::MeshStyle::runtime` is who performs a draw, and it is an
 * ordinary comparable value. `render::Runtime::cpu()` is the built-in
 * executor — it needs no device, sorts triangles back to front and
 * antialiases their edges, and is what a byte-identity plate is hashed
 * from. `world::diligent::painterRuntime(device)` is the same seam on a
 * GPU: it rasterises with a depth test and no edge antialiasing, then
 * reads the pixels back onto the canvas the caller passed.
 *
 * A SKETCH NAMES NEITHER. `sketch::painterRuntime()` is the one the
 * process installed — the device executor where a host brought a device
 * up, the CPU one where it did not — so a sketch writes the line once and
 * is correct on both tiers. Run this file plainly and the two cells hold
 * the same picture; run it with `--gpu` and the right-hand cell's meshes
 * are rasterised on the device while the left stays on the CPU. The note
 * under the right cell says which of the two it got, by comparing the
 * value.
 *
 * WHAT MAY DIFFER, and what may not: the FLAT PANELS are the canvas's own
 * draw on either executor, because `drawImagePanel` concats the
 * perspective and hands the image to Skia — a panel on a GPU-backed
 * canvas was already on the GPU. Only `drawMesh` changes hands, so the
 * floor and the curved sheet are where a difference can show at all.
 *
 * EDIT THESE FIRST
 *   kPanels — how many flat cards stand in each cell.
 *   kCurve  — the curved sheet's radius; larger is flatter.
 */

#include <sigilcompose/core/Core.h>
#include <sigilcompose/kit/Specimen.h>
#include <sigilcompose/texture/Texture.h>
#include <sigilgeometry/kit/Solids.h>
#include <sigilgeometry/mesh/Mesh.h>
#include <sigilgeometry/mesh/camera/Camera.h>
#include <sigilgeometry/mesh/render/Painter.h>
#include <sigilgeometry/mesh/render/Runtime.h>
#include <sigilsketch/canvas/Sketch.h>
#include <sigilsketch/kit/Kit.h>

#include <cmath>
#include <memory>
#include <string>
#include <utility>

namespace sketch = sigil::sketch;
namespace mesh = sigil::geometry::mesh;
namespace camera = sigil::geometry::mesh::camera;
namespace render = sigil::geometry::mesh::render;

using namespace sigil::compose;
using sigil::compose::toU8;

namespace {

constexpr SkSize kCanvas = {1180, 700};
constexpr SkSize kCell = {536, 512};
constexpr int kPanels = 3;
constexpr float kCurve = 300;

constexpr SkColor4f kCellGround{0.035f, 0.038f, 0.055f, 1};
constexpr SkColor4f kRule{0.20f, 0.21f, 0.25f, 1};

/** The house sheet, in this one's own look. */
sketch::kit::Theme sheetTheme() {
  sketch::kit::Theme look = sketch::kit::houseTheme();
  look.type.captionLabel = {.size = 11.5f, .track = 0.6f};
  look.type.captionNote = {.size = 11, .track = 0.3f};
  return look;
}

/** The one voice both cells are captioned in: the call over the picture,
 *  what that executor did under it. */
/** What a card carries: a header pill, a stack of rules and a bar row.
 *  An element tree like any other — the only thing 3D about it is where
 *  it ends up. */
Element card(float w, float h, SkColor4f accent) {
  const SkColor4f faint{1, 1, 1, 0.22f};
  auto rules = box().column().gap(9);
  for (int i = 0; i < 3; ++i)
    rules.child(box()
                    .width(w - 44 - (float)i * 26)
                    .height(6)
                    .corners({3})
                    .fill(Fill::color(faint)));

  auto bars = box().row().gap(5).alignItems(Align::End);
  for (int i = 0; i < 10; ++i) {
    const float t = (float)i / 9.0f;
    bars.child(
        box()
            .width(8)
            .height(8 + 26.0f * (0.5f + 0.5f * std::sin(t * 8.0f + 1.1f)))
            .corners({2})
            .fill(Fill::color({accent.fR, accent.fG, accent.fB, 0.85f})));
  }

  return box()
      .width(w)
      .height(h)
      .fill(Fill::color({0.05f, 0.065f, 0.115f, 0.94f}))
      .column()
      .gap(12)
      .padding(12)
      .child(box().width(w - 24).height(11).corners({5}).fill(
          Fill::color({accent.fR, accent.fG, accent.fB, 0.92f})))
      .child(std::move(rules))
      .child(std::move(bars));
}

}  // namespace

struct PainterGpu final : sketch::Sketch {
  sk_sp<SkImage> cards[kPanels];
  sk_sp<SkImage> screen;
  mesh::Mesh floor, curved;
  bool processIsCpu = true;

  static sk_sp<SkImage> bake(sketch::SketchContext& ctx, const Element& tree,
                             int w, int h) {
    const std::shared_ptr<TextureScene> scene = ctx.textureScene({w, h});
    if (!scene) return nullptr;
    scene->render(tree);
    return scene->image();
  }

  /** One cell's cockpit, drawn through @p runtime and nothing else. Every
   *  mesh, style and camera below is the same in both cells. */
  void draw(SkCanvas& canvas, const render::Runtime& runtime) const {
    camera::Camera view;
    view.eye = {0, 70, 720};
    view.target = {0, -10, 0};
    view.fovYDeg = 40;

    render::MeshStyle ground;
    ground.baseColor = {0.16f, 0.3f, 0.5f, 0.55f};
    ground.ambient = {0.45f, 0.5f, 0.62f, 1};
    ground.specular = 0;
    // A sheet has one facing, and this one is seen from the side its
    // winding calls the back; culling it would leave no ground at all.
    ground.backfaceCull = false;
    ground.runtime = runtime;
    render::drawMesh(canvas, floor, glm::mat4(1.0f), view, kCell, ground);

    for (int i = 0; i < kPanels; ++i) {
      const float x = ((float)i - (float)(kPanels - 1) * 0.5f) * 190.0f;
      const float yaw = -((float)i - (float)(kPanels - 1) * 0.5f) * 26.0f;
      render::drawImagePanel(canvas, cards[i], 176, 116,
                             camera::place({x, 90, -40}, yaw), view, kCell,
                             0.97f, runtime);
    }

    // The curved sheet: the same kind of picture, mapped per triangle,
    // and a surface that is its own light so the curve reads as a screen.
    render::MeshStyle emissive;
    emissive.texture = screen;
    emissive.baseColor = {1, 1, 1, 1};
    emissive.ambient = {0.9f, 0.9f, 0.9f, 1};
    emissive.lights = {};
    emissive.specular = 0;
    emissive.runtime = runtime;
    render::drawMesh(canvas, curved, camera::place({0, -96, 40}, 0, 8), view,
                     kCell, emissive);
  }

  Element cell(const char* call, std::u8string note,
               const render::Runtime& runtime) {
    return sketch::kit::caption(
        kCell.width(), toU8(call), std::move(note),
        custom(std::string("cell.") + call,
               [this, runtime](SkCanvas& canvas, const PaintContext&) {
                 draw(canvas, runtime);
               })
            .width(kCell.width())
            .height(kCell.height())
            // The viewport a mesh is projected onto is the
            // cell, and the canvas it lands on is the sheet:
            // without this the floor runs under the cell
            // beside it.
            .clip()
            .fill(Fill::color(kCellGround)));
  }

  void setup(sketch::SketchContext& ctx) override {
    const sketch::kit::Provide look(sheetTheme());
    // nothing moves; the sheet is complete at once
    sketch::kit::stage(ctx, {.size = kCanvas, .captureAt = 0.05});

    const SkColor4f accents[3] = {{0.2f, 0.85f, 1.0f, 1},
                                  {1.0f, 0.62f, 0.26f, 1},
                                  {0.68f, 0.45f, 1.0f, 1}};
    for (int i = 0; i < kPanels; ++i)
      cards[i] = bake(ctx, card(176, 116, accents[i % 3]), 176, 116);
    screen = bake(ctx, card(380, 108, {0.3f, 1.0f, 0.6f, 1}), 380, 108);
    floor = mesh::grid(20, 20, [](float u, float v) -> glm::vec3 {
      return {(u - 0.5f) * 900, -168, (v - 0.5f) * 900};
    });
    curved = mesh::cylinderPanel(364, 104, kCurve, 40, 8);

    // The comparison IS the readout: a value equal to the built-in one
    // says no host installed a device executor in this process.
    processIsCpu = sketch::painterRuntime() == render::Runtime::cpu();
    const std::u8string got =
        processIsCpu
            ? u8"this process installed none, so it IS Runtime::cpu() — "
              u8"run with --gpu and the same call answers "
              u8"diligent::painterRuntime"
            : u8"a device executor: diligent::painterRuntime, rasterising "
              u8"with a depth test and reading the pixels back onto this "
              u8"canvas";

    ctx.composer.render(sketch::kit::page(
        {.title = toU8("PAINTER RUNTIME \xc2\xb7 MeshStyle::runtime + "
                       "sketch::painterRuntime()"),
         .subtitle = toU8("dials \xc2\xb7 the runtime (named on each "
                          "cell) \xc2\xb7 the panel count (3 flat cards "
                          "and one curved sheet per cell)"),
         .footer = toU8("drawMesh changes hands; drawImagePanel does "
                        "not \xe2\x80\x94 a panel concats the "
                        "perspective and hands the image to the canvas, "
                        "so the cards are the same on both executors and "
                        "only the floor and the curve can differ")},
        kit::cells({.cells = {cell("render::Runtime::cpu()",
                                   u8"the built-in executor — no device, "
                                   u8"triangles sorted back to front, their "
                                   u8"edges antialiased",
                                   render::Runtime::cpu()),
                              cell("sketch::painterRuntime()", got,
                                   sketch::painterRuntime())},
                    .gap = 22,
                    .divider = Fill::color(kRule),
                    .align = Align::Start})));
  }
};

SIGIL_SKETCH(PainterGpu, "Kit \xc2\xb7 API",
             "the mesh painter as a value \xe2\x80\x94 the same cockpit "
             "through Runtime::cpu() and through whichever executor the "
             "process installed")
