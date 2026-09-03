/** @file
 * mesh_normal_bridge — A 3D BODY SHADED BY A 2D SURFACE RECIPE, and the
 * other source of the map it reads.
 *
 * The material layer shades a NORMAL MAP: hand it a picture whose pixels
 * encode surface directions and an environment to look those directions
 * up in, and it returns a shader. Where that normal map comes from is not
 * its business, and this sheet stands the two sources side by side.
 *
 *   FROM A MESH. `MeshStyle::Mode::Normals` rasterises the interpolated
 *     surface normal per pixel instead of a lit colour — a G-buffer, in
 *     one pass, on the same executor that would have drawn the body.
 *   FROM AN OUTLINE. `bevelNormals(path, bevelPx)` derives a rounded
 *     shoulder from a flat path's coverage. No mesh exists anywhere in
 *     that panel; the shoulder is a distance field over the silhouette.
 *
 * Both encode device-space normals as rgb = n·0.5 + 0.5, and both feed
 * the SAME recipe with the SAME environment and the same parameters — so
 * what the third panel shows is that the two encodings agree, and a
 * recipe cannot tell which one it was handed.
 *
 * THE THREE PASSES, per meshed body:
 *   1. NORMALS. The mesh is drawn into an offscreen surface in Normals
 *      mode. The clear colour is the FLAT normal (0,0,1) encoded, so
 *      pixels outside the silhouette read as facing the viewer rather
 *      than as garbage.
 *   2. RECIPE. `kit::chrome` / `kit::gold` over that map and an
 *      environment. Nothing in the recipe knows it is looking at a mesh.
 *   3. COVERAGE. The mesh is rasterised a second time in any opaque mode
 *      to lay down its silhouette, and the shader is painted over it
 *      through kSrcIn — so the recipe reaches exactly the body's pixels.
 * The bevelled panel needs no pass 1 and no pass 3: a path is its own
 * coverage, so the shader is painted straight through `drawPath`.
 *
 * WHY CURVATURE, NOT FLATNESS. Both meshed bodies curve in every
 * direction. An extruded cap would present one normal over its whole face
 * and sample the environment at one point, which is a flat colour and
 * says nothing about the bridge.
 *
 * EDIT THESE FIRST
 *   the superellipsoid's exponent — rounder bodies sweep more of the
 *                 environment across the silhouette.
 *   kBevelPx      — the shoulder's width on the third panel. Wide enough
 *                 and the squircle reads as the same kind of body the
 *                 mesh on the left is.
 *   the environments — studio and sunset are differently coloured skies,
 *                 and chrome shows the difference hardest.
 */

#include <include/core/SkColor.h>
#include <include/core/SkSurface.h>
#include <sigilgeometry/kit/Solids.h>
#include <sigilgeometry/mesh/Mesh.h>
#include <sigilgeometry/mesh/camera/Camera.h>
#include <sigilgeometry/mesh/render/Painter.h>
#include <sigilmaterial/kit/Surfaces.h>
#include <sigilmaterial/skia/Draw.h>
#include <sigilmaterial/skia/SkiaCompiler.h>
#include <sigilmaterial/texture/EnvironmentMap.h>
#include <sigilgeometry/kit/Silhouettes.h>
#include <sigilmaterial/texture/Surface.h>
#include <sigilsketch/canvas/Sketch.h>
#include <sigilweave/style/Type.h>

namespace sketch = sigil::sketch;
namespace shapes = sigil::geometry::shapes;
namespace weave = sigil::weave;

using namespace sigil::compose;
namespace mesh = sigil::geometry::mesh;
namespace camera = sigil::geometry::mesh::camera;
namespace render = sigil::geometry::mesh::render;
namespace material = sigil::material;
namespace kit = sigil::material::kit;

namespace {

constexpr SkSize kCanvas = {1320, 720};
/** The bevelled panel's shoulder, px. */
constexpr float kBevelPx = 54.0f;
/** Where the three bodies stand across the canvas. */
constexpr float kStations[3] = {-430, 0, 430};

constexpr SkColor4f kInk{0.90f, 0.93f, 0.97f, 1};
constexpr SkColor4f kDim{0.56f, 0.61f, 0.72f, 1};

weave::TextStyle label(float size, SkColor4f color, float track = 0) {
  return weave::textStyle({.size = size, .color = color, .track = track});
}

/** The third panel's silhouette: a squircle, in CANVAS coordinates,
 *  because `bevelNormals` places its map so a shader's device xy reads
 *  the normal under it. */
SkPath squircle() {
  const float side = 300;
  return shapes::squircle(3.4f)
      .path({side, side})
      .makeTransform(SkMatrix::Translate(
          kCanvas.width() * 0.5f + kStations[2] - side * 0.5f,
          kCanvas.height() * 0.5f - side * 0.5f));
}

}  // namespace

struct MeshNormalBridge final : sketch::Sketch {
  material::EnvironmentMap studio, sunset;
  mesh::Mesh blob, ring;

  /** The G-buffer pass: the body's normals, into an offscreen surface
   *  the size of the canvas so the shader's coordinates and the drawn
   *  silhouette's agree pixel for pixel. */
  static sk_sp<SkImage> normalPass(const mesh::Mesh& body,
                                   const glm::mat4& model,
                                   const camera::Camera& view) {
    sk_sp<SkSurface> surface = SkSurfaces::Raster(SkImageInfo::MakeN32Premul(
        (int)kCanvas.width(), (int)kCanvas.height()));
    surface->getCanvas()->clear(SkColorSetARGB(255, 128, 128, 255));
    render::MeshStyle normals;
    normals.mode = render::MeshStyle::Mode::Normals;
    render::drawMesh(*surface->getCanvas(), body, model, view, kCanvas,
                     normals);
    return surface->makeImageSnapshot();
  }

  static void shadeThroughCoverage(SkCanvas& canvas, const mesh::Mesh& body,
                                   const glm::mat4& model,
                                   const camera::Camera& view,
                                   const sk_sp<SkShader>& shader) {
    canvas.saveLayer(nullptr, nullptr);
    render::MeshStyle mask;
    mask.mode = render::MeshStyle::Mode::Uv;
    render::drawMesh(canvas, body, model, view, kCanvas, mask);
    SkPaint shade;
    shade.setShader(shader);
    shade.setBlendMode(SkBlendMode::kSrcIn);
    canvas.drawPaint(shade);
    canvas.restore();
  }

  void draw(SkCanvas& canvas) const {
    camera::Camera view;
    view.eye = {0, 90, 820};
    view.target = {0, 0, 0};

    {
      const glm::mat4 model = camera::place({kStations[0], 0, 0}, 24, -10, -8);
      kit::ChromeParams params;
      params.contrast = 1.35f;
      shadeThroughCoverage(
          canvas, blob, model, view,
          material::skia::shader(
              kit::chrome(material::Texture::of(normalPass(blob, model, view)),
                          sunset, params),
              {}));
    }
    {
      const glm::mat4 model = camera::place({kStations[1], 0, -40}, 0, -30, 18);
      kit::GoldParams params;
      params.crinkle = 0.12f;
      shadeThroughCoverage(
          canvas, ring, model, view,
          material::skia::shader(
              kit::gold(material::Texture::of(normalPass(ring, model, view)),
                        studio, params),
              {}));
    }
    // THE OTHER SOURCE. No mesh, no G-buffer, no coverage pass: the map
    // is derived from the path's own coverage and the path is its own
    // stencil, so one drawPath is the whole panel.
    {
      const SkPath outline = squircle();
      kit::ChromeParams params;
      params.contrast = 1.35f;
      SkPaint shade;
      shade.setAntiAlias(true);
      shade.setShader(material::skia::shader(
          kit::chrome(material::bevelNormals(outline, kBevelPx), sunset,
                      params),
          {}));
      canvas.drawPath(outline, shade);
    }
  }

  void setup(sketch::SketchContext& ctx) override {
    ctx.canvas(kCanvas.width(), kCanvas.height());
    ctx.background({0.051f, 0.051f, 0.075f, 1});
    ctx.captureAt(1.0);
    material::skia::install();
    studio = material::EnvironmentMap::studio();
    sunset = material::EnvironmentMap::sunset();
    blob = mesh::superellipsoid({170, 150, 90}, 2.6f, 64, 48);
    ring = mesh::torus(130, 46);
    const auto caption = [&](const char* call, const char* note, float x) {
      return box()
          .column()
          .gap(4)
          .width(300)
          .absolute()
          .inset(kCanvas.width() * 0.5f + x - 150, kCanvas.height() - 92, 0, 0)
          .child(text(toU8(call), label(12.5f, kInk, 0.4f)))
          .child(text(toU8(note), label(10.5f, kDim)).width(Dim(300)));
    };
    ctx.composer.render(
        stack()
            .child(custom([this](SkCanvas& canvas, const PaintContext&) {
                     draw(canvas);
                   }).inset(0))
            .child(text(toU8("NORMAL MAPS \xc2\xb7 two sources, one recipe"),
                        label(15, kInk, 2.0f))
                       .left(30)
                       .top(20))
            .child(caption("Mode::Normals \xe2\x86\x92 kit::chrome",
                           "a superellipsoid's own normals, rasterised into "
                           "a G-buffer and read back",
                           kStations[0]))
            .child(caption("Mode::Normals \xe2\x86\x92 kit::gold",
                           "the same bridge, another recipe and another "
                           "environment",
                           kStations[1]))
            .child(caption("bevelNormals(path, 54) \xe2\x86\x92 kit::chrome",
                           "no mesh at all \xe2\x80\x94 a shoulder derived "
                           "from a flat path's coverage, under the same "
                           "recipe and the same sky",
                           kStations[2]))
            .child(text(toU8("both encode device-space normals as "
                             "rgb = n\xc2\xb7" "0.5 + 0.5, and a recipe cannot "
                             "tell which one it was handed"),
                        label(11, kDim))
                       .left(30)
                       .bottom(16)));
  }
};

SIGIL_SKETCH(MeshNormalBridge, "Kit \xc2\xb7 API",
             "two sources for one normal map \xe2\x80\x94 a mesh's own "
             "normals through Mode::Normals, and a flat path's shoulder "
             "through bevelNormals, under the same surface recipes")
