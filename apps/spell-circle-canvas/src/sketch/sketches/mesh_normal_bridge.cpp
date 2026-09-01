// mesh_normal_bridge.cpp — A 3D BODY SHADED BY A 2D SURFACE RECIPE.
// =============================================================================
// The material layer shades a NORMAL MAP: hand it a picture whose pixels
// encode surface directions and an environment to look those directions
// up in, and it returns a shader. Where that normal map comes from is
// not its business — a bevel cast by an outline is one source, and the
// one this sheet uses is the other: a mesh rasterised in
// `MeshStyle::Mode::Normals`, which writes the interpolated surface
// normal per pixel instead of a lit colour.
//
// THE THREE PASSES, per body:
//   1. NORMALS. The mesh is drawn into an offscreen surface in Normals
//      mode. The clear colour is the FLAT normal (0,0,1) encoded, so
//      pixels outside the silhouette read as facing the viewer rather
//      than as garbage.
//   2. RECIPE. `kit::chrome` / `kit::gold` over that map and an
//      environment. Nothing in the recipe knows it is looking at a mesh.
//   3. COVERAGE. The mesh is rasterised a second time in any opaque mode
//      to lay down its silhouette, and the shader is painted over it
//      through kSrcIn — so the recipe reaches exactly the body's pixels.
//
// WHY CURVATURE, NOT FLATNESS. Both bodies here curve in every
// direction. An extruded cap would present one normal over its whole
// face and sample the environment at one point, which is a flat colour
// and says nothing about the bridge.
//
// EDIT THESE FIRST
//   the superellipsoid's exponent — rounder bodies sweep more of the
//   environment across the silhouette.
//   the environments               — studio and sunset are differently
//   coloured skies, and chrome shows the difference hardest.

#include <include/core/SkColor.h>
#include <include/core/SkSurface.h>
#include <sigilgeometry/mesh/Mesh.h>
#include <sigilgeometry/mesh/camera/Camera.h>
#include <sigilgeometry/mesh/render/Painter.h>
#include <sigilmaterial/kit/Surfaces.h>
#include <sigilmaterial/skia/Draw.h>
#include <sigilmaterial/skia/SkiaCompiler.h>
#include <sigilmaterial/texture/Surface.h>
#include <sigilsketch/canvas/Sketch.h>

namespace sketch = sigil::sketch;

using namespace sigil::compose;
namespace mesh = sigil::geometry::mesh;
namespace camera = sigil::geometry::mesh::camera;
namespace render = sigil::geometry::mesh::render;
namespace material = sigil::material;
namespace kit = sigil::material::kit;

namespace {

constexpr SkSize kCanvas = {1240, 720};

}  // namespace

struct MeshNormalBridge final : sketch::Sketch {
  material::Environment studio, sunset;
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
      const glm::mat4 model = camera::place({-280, 0, 0}, 24, -10, -8);
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
      const glm::mat4 model = camera::place({280, 0, -40}, 0, -30, 18);
      kit::GoldParams params;
      params.crinkle = 0.12f;
      shadeThroughCoverage(
          canvas, ring, model, view,
          material::skia::shader(
              kit::gold(material::Texture::of(normalPass(ring, model, view)),
                        studio, params),
              {}));
    }
  }

  void setup(sketch::SketchContext& ctx) override {
    ctx.canvas(kCanvas.width(), kCanvas.height());
    ctx.background({0.051f, 0.051f, 0.075f, 1});
    ctx.captureAt(1.0);
    material::skia::install();
    studio = material::Environment::studio();
    sunset = material::Environment::sunset();
    blob = mesh::superellipsoid({170, 150, 90}, 2.6f, 64, 48);
    ring = mesh::torus(130, 46);
    ctx.composer.render(custom([this](SkCanvas& canvas, const PaintContext&) {
                          draw(canvas);
                        }).inset(0));
  }
};

SIGIL_SKETCH(MeshNormalBridge, "Kit · API",
             "MeshStyle::Mode::Normals into a surface recipe — a mesh's "
             "own normals become the map chrome and gold are shaded from")
