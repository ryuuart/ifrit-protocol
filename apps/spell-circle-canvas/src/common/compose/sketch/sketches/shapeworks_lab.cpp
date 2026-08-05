// shapeworks_lab.cpp — SIGILSHAPE, LIVE: three of the library's moves
// =============================================================================
// A lab, not a study: there is no external artwork behind this one. What it
// exercises is src/common/shape itself, and everything here is
// hot-editable — change a blend key, a material parameter, a camera angle,
// save, and the canvas follows.
//
//   LEFT    the Illustrator blend tool. One star morphs into a circle
//           whose target rotates with the clock; the correspondence
//           (arc-length resampling + cyclic alignment) re-solves every
//           frame. OKLab keeps the red-to-blue run out of the mud.
//   TOP R   Skia-3D. An extruded star and a torus spin through
//           space::drawMesh — per-vertex Blinn, painter sort, all CPU,
//           all inside this one custom() leaf.
//   BOT R   the literal materials. Gold foil / brushed chrome / glass
//           badges, shaders built ONCE in setup() (bevel normal maps +
//           the procedural studio/sunset environments) and repainted
//           per frame for free. Glass refracts a checker baked at
//           setup — backdrop and badge share this leaf's local space.
//
// Every panel is a custom() leaf with Cache::None: drawing straight to the
// canvas, re-recorded each frame, because everything here moves. A real
// scene would cache the materials row, whose shaders never change once
// setup() has built them. shape:: draws through Skia and knows nothing
// about compose, so the custom() leaves are the entire adapter between
// them.

#include <sigilsketch/Sketch.h>

#include <sigilshape/Blend.h>
#include <sigilshape/Materials.h>
#include <sigilshape/Mesh.h>
#include <sigilshape/Space.h>

#include <include/core/SkPathBuilder.h>
#include <include/core/SkSurface.h>

#include <cmath>

using namespace sigil::compose;
namespace shape = sigil::shape;

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
  SkCanvas *c = surface->getCanvas();
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

} // namespace

struct ShapeworksLab : sigil::compose::sketch::Sketch {
  // Built once per (re)load; repainting a shader-filled path is cheap.
  shape::materials::Environment studio;
  shape::materials::Environment sunset;
  sk_sp<SkShader> gold, chrome, glass;
  SkPath goldPath, chromePath, glassPath;
  sk_sp<SkImage> backdrop;
  shape::Mesh starMesh;
  shape::Mesh ringMesh;

  Element describe(sketch::SketchContext &ctx) {
    // LEFT — the blend tool, re-keyed every frame.
    Element blendLab =
        custom([this](SkCanvas &canvas, const PaintContext &paint) {
          const float w = paint.size.width(), h = paint.size.height();
          const float spin = (float)paint.elapsedSeconds * 24.0f;
          shape::blend::Key from{
              star(5, 64, 26, {w * 0.5f, 80}, -90 + spin),
              {1.0f, 0.42f, 0.30f, 1}};
          shape::blend::Key to{SkPath::Circle(w * 0.5f, h - 90, 58),
                               {0.30f, 0.62f, 1.0f, 1}};
          shape::blend::Options options;
          options.steps = 9;
          options.smoothOutlines = true;
          shape::blend::draw(canvas,
                             shape::blend::make(from, to, options));
        })
            .inset(40, 60, 660, 60)
            .cache(Cache::None);

    // TOP RIGHT — meshes through the painter pipeline.
    Element meshLab =
        custom([this](SkCanvas &canvas, const PaintContext &paint) {
          const SkSize viewport = paint.size;
          shape::space::Camera camera;
          camera.eye = {0, 90, 560};
          camera.target = {0, 0, 0};
          camera.fovYDeg = 38;
          const float t = (float)paint.elapsedSeconds;
          shape::space::MeshStyle steel;
          steel.baseColor = {0.75f, 0.78f, 0.86f, 1};
          steel.specular = 0.8f;
          shape::space::drawMesh(
              canvas, starMesh,
              shape::space::place({-130, 0, 0}, t * 40.0f, -16), camera,
              viewport, steel);
          shape::space::MeshStyle bronze = steel;
          bronze.baseColor = {0.85f, 0.55f, 0.3f, 1};
          shape::space::drawMesh(
              canvas, ringMesh,
              shape::space::place({150, 0, -40}, 0, t * 31.0f, 14),
              camera, viewport, bronze);
        })
            .inset(620, 60, 40, 420)
            .cache(Cache::None);

    // BOTTOM RIGHT — the literal materials (shaders prebuilt in setup).
    Element materialLab =
        custom([this](SkCanvas &canvas, const PaintContext &paint) {
          (void)paint;
          if (backdrop)
            canvas.drawImage(backdrop, 0, 0);
          auto badge = [&](const SkPath &path, const sk_sp<SkShader> &m) {
            if (!m)
              return;
            SkPaint fill;
            fill.setAntiAlias(true);
            fill.setShader(m);
            canvas.save();
            canvas.clipPath(path, true);
            canvas.drawPaint(fill);
            canvas.restore();
          };
          badge(goldPath, gold);
          badge(chromePath, chrome);
          badge(glassPath, glass);
        })
            .inset(620, 400, 40, 60)
            .cache(Cache::None);

    return stack()
        .child(std::move(blendLab))
        .child(std::move(meshLab))
        .child(std::move(materialLab));
  }

  void setup(sketch::SketchContext &ctx) override {
    ctx.canvas(1200, 760);
    ctx.background({0.055f, 0.05f, 0.09f, 1});
    ctx.captureAt(3.4);

    studio = shape::materials::Environment::studio();
    sunset = shape::materials::Environment::sunset();

    // Badge geometry lives in the material leaf's LOCAL space (540 x 300).
    goldPath = star(8, 72, 50, {95, 150}, -90);
    chromePath = SkPath::Circle(270, 150, 74);
    glassPath = SkPath::Circle(445, 150, 78);
    backdrop = bakeChecker(540, 300);

    auto normalsFor = [](const SkPath &path, float bevel) {
      SkRect b = path.computeTightBounds();
      b.outset(bevel + 2, bevel + 2);
      return std::pair(shape::materials::bevelNormals(path, b.roundOut(),
                                                      bevel),
                       b.roundOut());
    };
    {
      auto [normals, bounds] = normalsFor(goldPath, 9);
      shape::materials::GoldParams params;
      params.crinkle = 0.4f;
      params.sparkle = 0.7f;
      gold = shape::materials::gold(
          normals, studio,
          {(float)bounds.left(), (float)bounds.top()}, params);
    }
    {
      auto [normals, bounds] = normalsFor(chromePath, 12);
      shape::materials::ChromeParams params;
      params.brushed = 0.6f;
      params.roughness = 0.2f;
      // studio, not sunset: a flat face reflects whatever sits dead
      // ahead on the equirect, and the sunset parks its sun there.
      chrome = shape::materials::chrome(
          normals, studio,
          {(float)bounds.left(), (float)bounds.top()}, params);
    }
    {
      auto [normals, bounds] = normalsFor(glassPath, 14);
      glass = shape::materials::glass(
          normals, studio, backdrop,
          {(float)bounds.left(), (float)bounds.top()});
    }

    starMesh = shape::mesh::extrude(star(5, 92, 42, {0, 0}, -90),
                                    {.depth = 36});
    ringMesh = shape::mesh::torus(96, 34);

    ctx.composer.render(describe(ctx));
  }
};

SIGIL_SKETCH(ShapeworksLab)
