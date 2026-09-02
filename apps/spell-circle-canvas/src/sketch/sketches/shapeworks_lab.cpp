// shapeworks_lab.cpp — SIGILGEOMETRY AND SIGILMATERIAL, LIVE
// =============================================================================
// A lab, not a study: there is no external artwork behind this one. What it
// exercises is the geometry and material vocabularies themselves, and
// everything here is hot-editable — change a blend key, a surface
// parameter, a camera angle, save, and the canvas follows.
//
//   LEFT    the Illustrator blend tool. One star morphs into a circle
//           whose target rotates with the clock; the correspondence
//           (arc-length resampling + cyclic alignment) re-solves every
//           frame. OKLab keeps the red-to-blue run out of the mud.
//   TOP R   Skia-3D. An extruded star and a torus spin through
//           render::drawMesh — per-vertex Blinn, painter sort, all CPU,
//           all inside this one custom() leaf.
//   BOT R   the literal surfaces. Gold foil / brushed chrome / glass
//           badges as MATERIALS built ONCE in setup() — a bevel normal
//           map and the procedural studio environment filling each
//           recipe's slots — and repainted per frame for free: a
//           material resolves once and its program is cached. Glass
//           refracts a checker baked at setup — backdrop and badge
//           share this leaf's local space.
//
// Every panel is a custom() leaf with Cache::None: drawing straight to the
// canvas, re-recorded each frame, because everything here moves. A real
// scene would cache the surfaces row, whose materials never change once
// setup() has built them. geometry:: and material:: draw through Skia and
// know nothing about compose, so the custom() leaves are the entire
// adapter between them.

#include <include/core/SkPathBuilder.h>
#include <include/core/SkSurface.h>
#include <sigilgeometry/mesh/Mesh.h>
#include <sigilgeometry/mesh/camera/Camera.h>
#include <sigilgeometry/mesh/render/Painter.h>
#include <sigilgeometry/path/blend/Blend.h>
#include <sigilmaterial/kit/Surfaces.h>
#include <sigilmaterial/skia/Draw.h>
#include <sigilmaterial/skia/SkiaCompiler.h>
#include <sigilmaterial/texture/Surface.h>
#include <sigilsketch/canvas/Sketch.h>

#include <cmath>
#include <optional>

namespace sketch = sigil::sketch;

using namespace sigil::compose;
namespace geometry = sigil::geometry;
namespace material = sigil::material;

namespace {

SkPath star(int points, float outer, float inner, SkPoint center,
            float rotationDeg) {
  SkPathBuilder b;
  const float step = (float)M_PI / (float)points;
  const float rot = rotationDeg * (float)M_PI / 180.0f;
  for (int i = 0; i < points * 2; ++i) {
    const float r = i % 2 == 0 ? outer : inner;
    const float a = rot + step * (float)i;
    const SkPoint p = {center.fX + r * std::cos(a),
                       center.fY + r * std::sin(a)};
    i == 0 ? (void)b.moveTo(p) : (void)b.lineTo(p);
  }
  b.close();
  return b.detach();
}

sk_sp<SkImage> bakeChecker(int w, int h) {
  sk_sp<SkSurface> surface =
      SkSurfaces::Raster(SkImageInfo::MakeN32Premul(w, h));
  SkCanvas* c = surface->getCanvas();
  SkPaint paint;
  for (int y = 0; y * 28 < h; ++y)
    for (int x = 0; x * 28 < w; ++x) {
      paint.setColor((x + y) % 2 ? 0xff2b2b3a : 0xffb9bece);
      c->drawRect(SkRect::MakeXYWH((float)x * 28, (float)y * 28, 28, 28),
                  paint);
    }
  paint.setAntiAlias(true);
  paint.setColor(0xcc4d7dff);
  c->drawCircle((float)w * 0.32f, (float)h * 0.5f, 44, paint);
  paint.setColor(0xccff7d4d);
  c->drawCircle((float)w * 0.72f, (float)h * 0.42f, 38, paint);
  return surface->makeImageSnapshot();
}

}  // namespace

struct ShapeworksLab : sketch::Sketch {
  // Built once per (re)load; repainting a material-filled path is cheap.
  material::Environment studio;
  std::optional<material::Material> gold, chrome, glass;
  SkPath goldPath, chromePath, glassPath;
  sk_sp<SkImage> backdrop;
  geometry::mesh::Mesh starMesh;
  geometry::mesh::Mesh ringMesh;

  Element describe(sketch::SketchContext& ctx) {
    // LEFT — the blend tool, re-keyed every frame.
    Element blendLab =
        custom([this](SkCanvas& canvas, const PaintContext& paint) {
          const float w = paint.size.width(), h = paint.size.height();
          const float spin = (float)paint.elapsedSeconds * 24.0f;
          geometry::path::blend::Key from{
              star(5, 64, 26, {w * 0.5f, 80}, -90 + spin),
              {1.0f, 0.42f, 0.30f, 1}};
          geometry::path::blend::Key to{SkPath::Circle(w * 0.5f, h - 90, 58),
                                        {0.30f, 0.62f, 1.0f, 1}};
          geometry::path::blend::Options options;
          options.steps = 9;
          options.smoothOutlines = true;
          geometry::path::blend::draw(
              canvas, geometry::path::blend::make(from, to, options));
        })
            .inset(40, 60, 660, 60)
            .cache(Cache::None);

    // TOP RIGHT — meshes through the painter pipeline.
    Element meshLab =
        custom([this](SkCanvas& canvas, const PaintContext& paint) {
          const SkSize viewport = paint.size;
          geometry::mesh::camera::Camera camera;
          camera.eye = {0, 90, 560};
          camera.target = {0, 0, 0};
          camera.fovYDeg = 38;
          const float t = (float)paint.elapsedSeconds;
          geometry::mesh::render::MeshStyle steel;
          steel.baseColor = {0.75f, 0.78f, 0.86f, 1};
          steel.specular = 0.8f;
          geometry::mesh::render::drawMesh(
              canvas, starMesh,
              geometry::mesh::camera::place({-130, 0, 0}, t * 40.0f, -16),
              camera, viewport, steel);
          geometry::mesh::render::MeshStyle bronze = steel;
          bronze.baseColor = {0.85f, 0.55f, 0.3f, 1};
          geometry::mesh::render::drawMesh(
              canvas, ringMesh,
              geometry::mesh::camera::place({150, 0, -40}, 0, t * 31.0f, 14),
              camera, viewport, bronze);
        })
            .inset(620, 60, 40, 420)
            .cache(Cache::None);

    // BOTTOM RIGHT — the literal surfaces (materials prebuilt in setup).
    Element materialLab =
        custom([this](SkCanvas& canvas, const PaintContext& paint) {
          (void)paint;
          if (backdrop) canvas.drawImage(backdrop, 0, 0);
          if (gold) material::skia::fill(canvas, goldPath, *gold);
          if (chrome) material::skia::fill(canvas, chromePath, *chrome);
          if (glass) material::skia::fill(canvas, glassPath, *glass);
        })
            .inset(620, 400, 40, 60)
            .cache(Cache::None);

    return stack()
        .child(std::move(blendLab))
        .child(std::move(meshLab))
        .child(std::move(materialLab));
  }

  void setup(sketch::SketchContext& ctx) override {
    ctx.canvas(1200, 760);
    ctx.background({0.055f, 0.05f, 0.09f, 1});
    ctx.captureAt(3.4);

    material::skia::install();
    studio = material::Environment::studio();

    // Badge geometry lives in the surfaces leaf's LOCAL space (540 x 300);
    // bevelNormals() places each normal map at its outline's bounds, so
    // the recipe reads the normal under the pixel it shades.
    goldPath = star(8, 72, 50, {95, 150}, -90);
    chromePath = SkPath::Circle(270, 150, 74);
    glassPath = SkPath::Circle(445, 150, 78);
    backdrop = bakeChecker(540, 300);
    {
      material::kit::GoldParams params;
      params.crinkle = 0.4f;
      params.sparkle = 0.7f;
      gold = material::kit::gold(material::bevelNormals(goldPath, 9), studio,
                                 params);
    }
    {
      material::kit::ChromeParams params;
      params.brushed = 0.6f;
      params.roughness = 0.2f;
      // studio, not sunset: a flat face reflects whatever sits dead
      // ahead on the equirect, and the sunset parks its sun there.
      chrome = material::kit::chrome(material::bevelNormals(chromePath, 12),
                                     studio, params);
    }
    glass = material::kit::glass(material::bevelNormals(glassPath, 14), studio,
                                 material::Texture::of(backdrop));

    starMesh =
        geometry::mesh::extrude(star(5, 92, 42, {0, 0}, -90), {.depth = 36});
    ringMesh = geometry::mesh::torus(96, 34);

    ctx.composer.render(describe(ctx));
  }
};

SIGIL_SKETCH(
    ShapeworksLab, "Kit",
    "SigilGeometry and SigilMaterial live \xe2\x80\x94 the Illustrator "
    "blend, spinning meshes, and gold/chrome/glass surfaces built once as "
    "materials, all hot-editable")
