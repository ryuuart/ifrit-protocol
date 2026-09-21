/** @file
 * The same textured cockpit through the CPU and host-selected runtimes.
 * Both viewports use identical geometry, camera, textures and lighting.
 * Only mesh drawing changes executor; perspective image panels remain canvas
 * draws. The host viewport reports whether a device executor or the CPU
 * fallback was selected, so a raster capture cannot imply GPU coverage.
 */

// TAGS: Geometry/Meshes

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

namespace {

constexpr SkSize kCanvas = {1180, 840};
constexpr SkSize kCell = {536, 420};
constexpr int kPanels = 3;
constexpr float kCurve = 300;

constexpr SkColor4f kCellGround{0.035f, 0.038f, 0.055f, 1};

/** The specimen sheet, in this one's own look. */
sketch::kit::Theme sheetTheme() {
  sketch::kit::Theme look = sketch::kit::studyTheme();
  look.type.captionLabel = {.size = 11.5f, .track = 0.6f};
  return look;
}

/** What a card carries: a header pill, a stack of rules and a bar row.
 *  An element tree like any other — the only thing 3D about it is where
 *  it ends up. */
Element card(float w, float h, SkColor4f accent) {
  const SkColor4f faint{1, 1, 1, 0.22f};
  const auto rule = [w, faint](int i) {
    return box()
        .width(w - 44 - (float)i * 26)
        .height(6)
        .borderRadius({3})
        .fill(Fill::color(faint));
  };
  const auto bar = [accent](int i) {
    const float t = (float)i / 9.0f;
    return box()
        .width(8)
        .height(8 + 26.0f * (0.5f + 0.5f * std::sin(t * 8.0f + 1.1f)))
        .borderRadius({2})
        .fill(Fill::color({accent.fR, accent.fG, accent.fB, 0.85f}));
  };
  return box()
      .width(w)
      .height(h)
      .fill(Fill::color({0.05f, 0.065f, 0.115f, 0.94f}))
      .column()
      .gap(12)
      .padding(12)
      .children({box().width(w - 24).height(11).borderRadius({5}).fill(
                     Fill::color({accent.fR, accent.fG, accent.fB, 0.92f})),
                 box().column().gap(9).children({each(3, rule)}),
                 box()
                     .row()
                     .gap(5)
                     .alignItems(Align::End)
                     .children({each(10, bar)})});
}

}  // namespace

struct PainterGpu {
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
    const camera::Camera view{
        .eye = {0, 70, 720}, .target = {0, -10, 0}, .fovYDeg = 40};

    // A sheet has one facing, and this one is seen from the side its
    // winding calls the back; culling it would leave no ground at all.
    const render::MeshStyle ground{.baseColor = {0.16f, 0.3f, 0.5f, 0.55f},
                                   .ambient = {0.45f, 0.5f, 0.62f, 1},
                                   .specular = 0,
                                   .backfaceCull = false,
                                   .runtime = runtime};
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
    const render::MeshStyle emissive{.baseColor = {1, 1, 1, 1},
                                     .lights = {},
                                     .ambient = {0.9f, 0.9f, 0.9f, 1},
                                     .specular = 0,
                                     .texture = screen,
                                     .runtime = runtime};
    render::drawMesh(canvas, curved, camera::place({0, -96, 40}, 0, 8), view,
                     kCell, emissive);
  }

  Element viewport(const char* key, const render::Runtime& runtime) {
    return custom(key,
                  [this, runtime](SkCanvas& canvas) { draw(canvas, runtime); })
        .width(kCell.width())
        .height(kCell.height())
        .overflow(Overflow::Clip)
        .fill(Fill::color(kCellGround));
  }

  void setup(sketch::SketchContext& ctx) {
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
    ctx.composer.render(sketch::kit::page(
        {.title = "One cockpit, two executors",
         .subtitle = "The camera, meshes, textures and lighting are identical. "
                     "Only the mesh runtime is selected differently.",
         .footer = "The host selects the runtime. The sketch asks for its "
                   "value; CPU remains available for an explicit reference."},
        box().column().gap(26).children(
            {sketch::kit::comparison(
                 {.cases =
                      {{.title = "CPU REFERENCE",
                        .control = "render::Runtime::cpu()",
                        .figure =
                            viewport("painter.cpu", render::Runtime::cpu()),
                        .note = "Triangles are sorted back to front; their "
                                "edges are antialiased."},
                       {.title = "HOST RUNTIME",
                        .control = processIsCpu ? "CPU fallback selected"
                                                : "Diligent device selected",
                        .figure =
                            viewport("painter.host", sketch::painterRuntime()),
                        .note = processIsCpu
                                    ? "No device executor is installed. This "
                                      "is the same CPU path as the reference."
                                    : "Meshes use depth-tested device "
                                      "rasterization, then return pixels to "
                                      "the canvas."}},
                  .measure = 1100,
                  .gap = 28}),
             sketch::kit::sectionHeader(
                 {.label = "WHERE TO LOOK",
                  .note = "drawMesh changes executor; image panels remain "
                          "canvas draws"}),
             box().row().gap(28).children(
                 {text("FLAT CARDS\nPerspective image panels: the shared "
                       "canvas path.")
                      .width(348),
                  text("CURVED DISPLAY\nA textured triangle mesh: inspect the "
                       "silhouette.")
                      .width(348),
                  text("TRANSLUCENT FLOOR\nA mesh surface: inspect overlap and "
                       "edge treatment.")
                      .width(348)})})));
  }
};

SIGIL_SKETCH(PainterGpu, "Kit · API",
             "the mesh painter as a value — the same cockpit "
             "through Runtime::cpu() and through whichever executor the "
             "process installed")
