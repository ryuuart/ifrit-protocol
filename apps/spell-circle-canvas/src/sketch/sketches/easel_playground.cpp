// easel_playground.cpp — THREE LIBRARIES, ONE SENTENCE EACH
// =============================================================================
// SigilGeometry makes outlines and moves them (path ops, blends, splines,
// point clouds); SigilMaterial makes surfaces (a bevel normal map, an
// environment, a recipe over both); SigilCompose places what they draw.
// This playground is the seam between the three in the hot-reload loop —
// every chain below is meant to be EDITED. Change a number, save, watch.
//
//   left    a badge: star -> bloat -> roughen -> offset, filled with gold
//   middle  the blend tool: spiky coral thing melting into a sky dot
//   right   a wire crossing space: tube + marquee + glow + particles
//
// Nothing here goes through a façade. The outline is a chain of path
// operators; the gold is kit::gold over bevelNormals(); the draw is
// skia::fill. When a sentence runs out of words the exact layer is what
// you were already holding.

#include <include/core/SkPathBuilder.h>
#include <include/core/SkSurface.h>
#include <sigilgeometry/mesh/camera/Camera.h>
#include <sigilgeometry/mesh/curve/Curve.h>
#include <sigilgeometry/mesh/pop/Points.h>
#include <sigilgeometry/mesh/render/Painter.h>
#include <sigilgeometry/path/Ops.h>
#include <sigilgeometry/path/blend/Blend.h>
#include <sigilmaterial/kit/Surfaces.h>
#include <sigilmaterial/skia/Draw.h>
#include <sigilmaterial/skia/SkiaCompiler.h>
#include <sigilmaterial/texture/Surface.h>
#include <sigilsketch/canvas/Sketch.h>

#include <cmath>

namespace sketch = sigil::sketch;

using namespace sigil::compose;
namespace geometry = sigil::geometry;
namespace material = sigil::material;

namespace {

// ---------------------------------------------------------------------------
// Stock outlines, centred on the origin — placed by translating the canvas.

SkPath dot(float radius) { return SkPath::Circle(0, 0, radius); }

SkPath ngon(int sides, float radius, float rotationDeg = -90) {
  SkPathBuilder b;
  sides = std::max(sides, 3);
  const float rot = rotationDeg * (float)M_PI / 180.0f;
  for (int i = 0; i < sides; ++i) {
    const float a = rot + (float)i / (float)sides * 2.0f * (float)M_PI;
    const SkPoint p = {radius * std::cos(a), radius * std::sin(a)};
    i == 0 ? (void)b.moveTo(p) : (void)b.lineTo(p);
  }
  b.close();
  return b.detach();
}

SkPath star(int points, float radius, float innerRatio = 0.45f,
            float rotationDeg = -90) {
  SkPathBuilder b;
  points = std::max(points, 3);
  const float rot = rotationDeg * (float)M_PI / 180.0f;
  for (int i = 0; i < points * 2; ++i) {
    const float r = i % 2 == 0 ? radius : radius * innerRatio;
    const float a = rot + (float)i / (float)(points * 2) * 2.0f * (float)M_PI;
    const SkPoint p = {r * std::cos(a), r * std::sin(a)};
    i == 0 ? (void)b.moveTo(p) : (void)b.lineTo(p);
  }
  b.close();
  return b.detach();
}

/** A closed loop of eight points orbiting the origin, bobbing with the
 *  clock — the wire everything on the right hangs from. */
geometry::mesh::curve::Spline3 loopAt(float t, float radius, float bob) {
  geometry::mesh::curve::Spline3 spline;
  for (int i = 0; i < 8; ++i) {
    const float a = (float)i / 8.0f * 2.0f * (float)M_PI + t * 0.25f;
    spline.points.emplace_back(std::cos(a) * radius,
                               std::sin(a * 2.0f + t * 0.4f) * bob,
                               std::sin(a) * radius);
  }
  spline.closed = true;
  return spline;
}

// The tiled untileable strip: a Fibonacci word (A→AB, B→A — the golden
// substitution) rendered as long/short cells. Aperiodic within its
// span: no subsegment repeats, yet ONE span wraps cleanly, which is
// exactly what a marquee riding a closed loop needs. Baked with the
// word along Y so ribbon v (= distance along the curve) reads it.
sk_sp<SkImage> fibonacciStrip(int width, int height) {
  std::string word = "A";
  while ((int)word.size() < 96) {
    std::string next;
    for (char c : word) next += (c == 'A') ? "AB" : "A";
    word = next;
  }
  sk_sp<SkSurface> surface =
      SkSurfaces::Raster(SkImageInfo::MakeN32Premul(width, height));
  SkCanvas* c = surface->getCanvas();
  c->clear(SkColorSetARGB(255, 10, 12, 24));
  SkPaint paint;
  paint.setAntiAlias(true);
  // Long/short cell heights in golden ratio; colors alternate by term.
  const float unit =
      (float)height / (1.618f * 55.0f + 34.0f);  // F(10)/F(9) mix of the span
  float y = 0;
  for (char term : word) {
    const float cell = term == 'A' ? unit * 1.618f : unit;
    if (y > (float)height) break;
    paint.setColor(term == 'A' ? SkColorSetARGB(255, 64, 220, 255)
                               : SkColorSetARGB(255, 255, 120, 220));
    c->drawRect(SkRect::MakeXYWH(8, y + 1.5f, (float)width - 16, cell - 3.0f),
                paint);
    y += cell;
  }
  return surface->makeImageSnapshot();
}

}  // namespace

struct EaselPlayground : sketch::Sketch {
  material::Environment studio;
  sk_sp<SkImage> marqueeStrip;

  Element describe(sketch::SketchContext& ctx) {
    // LEFT — an outline recipe wearing gold. The recipe is a chain of
    // path operators; the gold is a material over the bevel the cooked
    // outline casts. Try: ops::Twirl{40}, a bigger bloat, kit::chrome.
    Element badge =
        custom([this](SkCanvas& canvas, const PaintContext& paint) {
          const float wobble =
              2.0f + 1.5f * std::sin((float)paint.elapsedSeconds * 0.8f);
          const geometry::path::ops::PathOp recipe =
              geometry::path::ops::chain({
                  geometry::path::ops::PuckerBloat{0.25f},
                  geometry::path::ops::Roughen{wobble, 8, 3},
                  geometry::path::ops::offsetBy(6),
              });
          const SkPath outline = recipe(star(7, 110, 0.55f));
          material::kit::GoldParams gold;
          canvas.save();
          canvas.translate(paint.size.width() * 0.5f,
                           paint.size.height() * 0.5f);
          material::skia::fill(
              canvas, outline,
              material::kit::gold(material::bevelNormals(outline, 9), studio,
                                  gold));
          canvas.restore();
        })
            .inset(30, 60, 830, 380)
            .cache(Cache::None);

    // LEFT LOW — the same operator language, plain paint: a pond of
    // offset rings breathing.
    Element pond =
        custom([](SkCanvas& canvas, const PaintContext& paint) {
          const float t = (float)paint.elapsedSeconds;
          canvas.save();
          canvas.translate(paint.size.width() * 0.5f,
                           paint.size.height() * 0.5f);
          for (int i = 0; i < 5; ++i) {
            const float phase = t * 0.7f - (float)i * 0.55f;
            const float grow = 20.0f * (float)i + 14.0f * std::sin(phase);
            const SkPath ring = geometry::path::ops::offset(
                geometry::path::ops::Zigzag{4, 18, true}(ngon(6, 36)), grow);
            SkPaint stroke;
            stroke.setAntiAlias(true);
            stroke.setStyle(SkPaint::kStroke_Style);
            stroke.setStrokeWidth(2.5f);
            stroke.setColor4f({0.4f + 0.12f * (float)i, 0.8f, 1.0f,
                               0.85f - 0.15f * (float)i});
            canvas.drawPath(ring, stroke);
          }
          canvas.restore();
        })
            .inset(30, 440, 830, 40)
            .cache(Cache::None);

    // MIDDLE — the blend tool. Try: Spacing::SmoothColor, a spine with
    // Orientation::AlignToPath.
    Element melt =
        custom([](SkCanvas& canvas, const PaintContext& paint) {
          const float sway =
              40.0f * std::sin((float)paint.elapsedSeconds * 0.6f);
          const SkPoint top = {paint.size.width() * 0.5f + sway, 90};
          const SkPoint bottom = {paint.size.width() * 0.5f - sway,
                                  paint.size.height() - 90};
          geometry::path::blend::Key from{
              star(5, 66, 0.45f)
                  .makeTransform(SkMatrix::Translate(top.fX, top.fY)),
              {1.0f, 0.42f, 0.30f, 1}};
          geometry::path::blend::Key to{
              dot(56).makeTransform(SkMatrix::Translate(bottom.fX, bottom.fY)),
              {0.30f, 0.62f, 1.0f, 1}};
          geometry::path::blend::Options options;
          options.steps = 10;
          options.smoothOutlines = true;
          geometry::path::blend::draw(
              canvas, geometry::path::blend::make(from, to, options));
        })
            .inset(390, 60, 420, 40)
            .cache(Cache::None);

    // RIGHT — a wire crossing space, everything hung on it: a steel
    // tube, particles, and the TILED UNTILEABLE MARQUEE — a
    // Fibonacci-word band scrolling around a second, wider loop.
    // Nothing marquee-shaped exists in the library: it is
    // a line profile swept (the (across, along) uv chart) + tileTexture +
    // uvTransform, the same verbs any conveyor or ticker uses.
    Element flight =
        custom([this](SkCanvas& canvas, const PaintContext& paint) {
          const SkSize viewport = paint.size;
          geometry::mesh::camera::Camera camera;
          camera.eye = {0, 170, 620};
          camera.target = {0, 0, 0};
          camera.fovYDeg = 40;

          const float t = (float)paint.elapsedSeconds;
          const geometry::mesh::curve::Spline3 loop = loopAt(t, 210, 80);

          geometry::mesh::render::MeshStyle steel;
          steel.baseColor = {0.62f, 0.7f, 0.85f, 1};
          steel.specular = 0.9f;
          geometry::mesh::render::drawMesh(
              canvas,
              geometry::mesh::curve::sweep(
                  loop, geometry::mesh::curve::profile::circle(),
                  {.segments = 180, .scale = 7}),
              glm::mat4(1.0f), camera, viewport, steel);
          SkPaint wire;
          wire.setAntiAlias(true);
          wire.setStyle(SkPaint::kStroke_Style);
          wire.setStrokeWidth(1);
          wire.setColor4f({1, 1, 1, 0.25f});
          canvas.drawPath(
              geometry::mesh::curve::project(loop, camera, viewport, 256),
              wire);

          // The marquee: a wider sibling loop wearing the Fibonacci
          // band. A swept line charts (across, along) into uv; the strip
          // tiles (one aperiodic period wraps the loop) and the
          // uvTransform's translate IS the scroll.
          const geometry::mesh::curve::Spline3 orbit = loopAt(t, 265, 96);
          geometry::mesh::render::MeshStyle band;
          band.texture = marqueeStrip;
          band.tileTexture = true;
          band.baseColor = {1, 1, 1, 0.92f};
          band.ambient = {0.95f, 0.95f, 0.95f, 1};
          band.lights = {};
          band.specular = 0;
          band.uvTransform = SkMatrix::Translate(0, t * 0.11f);
          geometry::mesh::render::drawMesh(
              canvas,
              geometry::mesh::curve::sweep(
                  orbit, geometry::mesh::curve::profile::line(),
                  {.segments = 220,
                   .scale = 30,
                   .normals =
                       geometry::mesh::curve::SweepOptions::Normals::Frame}),
              glm::mat4(1.0f), camera, viewport, band);

          // Particles: points on the wire, drifted by noise, tinted
          // along the "t" lane the spline scatter writes, sized by a
          // lane of our own.
          geometry::mesh::Cloud sparks =
              geometry::mesh::points::onSpline(loop, 220);
          geometry::mesh::points::displaceNoise(sparks, 26, 0.012f, 9);
          const std::vector<float>* along = sparks.scalarIf("t");
          std::vector<glm::vec4>& tint = sparks.color("tint");
          std::vector<float>& size = sparks.scalar("size", 1);
          const glm::vec4 cool = {0.4f, 0.85f, 1.0f, 0.4f};
          const glm::vec4 warm = {1.0f, 0.5f, 0.9f, 0.4f};
          for (size_t i = 0; i < sparks.size(); ++i) {
            const float f =
                along ? (*along)[i] : (float)i / (float)(sparks.size() - 1);
            tint[i] = cool + (warm - cool) * f;
            size[i] = 1.0f + 0.6f * std::sin(f * 37.0f);
          }
          geometry::mesh::points::BillboardStyle glow;
          glow.size = 11;
          glow.sizeLane = "size";
          glow.tintLane = "tint";
          geometry::mesh::points::drawBillboards(canvas, sparks, camera,
                                                 viewport, glow);
        })
            .inset(840, 60, 30, 40)
            .clip()
            .cache(Cache::None);

    return stack()
        .child(std::move(badge))
        .child(std::move(pond))
        .child(std::move(melt))
        .child(std::move(flight));
  }

  void setup(sketch::SketchContext& ctx) override {
    ctx.canvas(1240, 760);
    ctx.background({0.045f, 0.045f, 0.085f, 1});
    ctx.captureAt(2.6);
    material::skia::install();
    studio = material::Environment::studio();
    marqueeStrip = fibonacciStrip(96, 1024);
    ctx.composer.render(describe(ctx));
  }
};

SIGIL_SKETCH(
    EaselPlayground, "Kit",
    "Geometry operators, materials and compose elements read straight: "
    "a gold badge from path ops and a bevel, a blend, a wire with a "
    "marquee and particles")
