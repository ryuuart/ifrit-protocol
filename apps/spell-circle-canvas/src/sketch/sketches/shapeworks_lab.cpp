// shapeworks_lab.cpp — THE SEAM: SigilGeometry, SigilMaterial and
// SigilCompose meeting inside one custom() leaf.
// =============================================================================
// Three libraries with nothing in common but this file. SigilGeometry
// makes outlines and bodies and moves them; SigilMaterial makes surfaces
// — a bevel normal map, an environment, a recipe over both; SigilCompose
// places what they draw. Neither of the first two knows compose exists,
// so a `custom()` leaf is the ENTIRE adapter between them, and this is
// the widest one in the book: it names more geometry and material
// symbols than any other sketch, which is what makes it the guard the
// hot-reload link surface is checked against.
//
//   OUTLINE   a recipe, not a shape: star → PuckerBloat → Roughen →
//             offsetBy, cooked once, then worn as gold. The bevel the
//             cooked outline casts is what the material reads, so the
//             surface follows an outline nobody drew by hand.
//   SURFACES  the literal recipes. Gold foil, brushed chrome and glass
//             as MATERIALS built ONCE in setup — a bevel normal map and
//             the procedural studio environment filling each recipe's
//             slots. Glass refracts a checker baked at setup: backdrop
//             and badge share this leaf's local space, which is what lets
//             one of them see the other.
//   WIRE      a curve carrying everything. The same closed loop is swept
//             into a steel tube, projected to a 2D outline, swept a
//             second time wider as a scrolling band, and scattered into
//             a point chain that noises, ramps and varies before the
//             sink splats it.
//
// Everything here is hot-editable — change an operator, a surface
// parameter, a camera angle, save, and the canvas follows, because a save
// re-runs setup and setup is where the outlines and the materials are
// cooked. That is also why only the wire's leaf is `Cache::None`: the two
// left-hand panels are static and ASK for the bake, and a recording would
// not have helped either of them — replaying a picture re-runs every
// shader over every pixel, and only a bake keeps the pixels.
//
// EDIT THESE FIRST
//   the outline recipe — swap Roughen for ops::Twirl{40}, raise the
//                        bloat, hand the result to kit::chrome instead.
//   ChromeParams::brushed / GoldParams::sparkle — the surfaces row.
//   the chain on the wire — .count(), .noise(), the two ramp stops.

#include <include/core/SkPathBuilder.h>
#include <include/core/SkSurface.h>
#include <sigilgeometry/mesh/Mesh.h>
#include <sigilgeometry/mesh/camera/Camera.h>
#include <sigilgeometry/mesh/curve/Curve.h>
#include <sigilgeometry/mesh/pop/Pop.h>
#include <sigilgeometry/mesh/render/Painter.h>
#include <sigilgeometry/kit/Silhouettes.h>
#include <sigilgeometry/path/Ops.h>
#include <sigilmaterial/kit/Surfaces.h>
#include <sigilmaterial/pattern/Patterns.h>
#include <sigilmaterial/pattern/Tile.h>
#include <sigilmaterial/skia/Draw.h>
#include <sigilmaterial/skia/SkiaCompiler.h>
#include <sigilmaterial/texture/EnvironmentMap.h>
#include <sigilmaterial/texture/Surface.h>
#include <sigilsketch/canvas/Sketch.h>

#include <cmath>
#include <optional>
#include <string>
#include <vector>

namespace sketch = sigil::sketch;

using namespace sigil::compose;
namespace geometry = sigil::geometry;
namespace material = sigil::material;
namespace mesh = sigil::geometry::mesh;
namespace curve = sigil::geometry::mesh::curve;
namespace ops = sigil::geometry::path::ops;
namespace shapes = sigil::geometry::shapes;
namespace mpattern = sigil::material::pattern;

namespace {

namespace pop = mesh::pop;

/** A star from the geometry kit's own generator, PLACED: the kit lays a
 *  silhouette inside the box it is handed and starts it point-up, so an
 *  outer radius is half a box and a centre is one translation. */
SkPath star(int points, float outer, float inner, SkPoint centre) {
  const SkPath path =
      shapes::star(points, inner / outer).path({2 * outer, 2 * outer});
  return path.makeTransform(
      SkMatrix::Translate(centre.fX - outer, centre.fY - outer));
}

/** WHAT THE GLASS REFRACTS: the pattern shelf's checker under two discs,
 *  so what is bent behind the badge has both a grid and a curve in it.
 *  The check is the shelf's tile sampled as a repeating shader rather
 *  than a pair of loops over cells. */
sk_sp<SkImage> bakeChecker(int w, int h) {
  sk_sp<SkSurface> surface =
      SkSurfaces::Raster(SkImageInfo::MakeN32Premul(w, h));
  SkCanvas* c = surface->getCanvas();
  SkPaint paint;
  paint.setShader(mpattern::checker(28.0f, {0.169f, 0.169f, 0.227f, 1},
                                    {0.725f, 0.745f, 0.808f, 1})
                      .texture()
                      .shader());
  c->drawRect(SkRect::MakeWH((float)w, (float)h), paint);
  paint.setShader(nullptr);
  paint.setAntiAlias(true);
  paint.setColor(0xcc4d7dff);
  c->drawCircle((float)w * 0.32f, (float)h * 0.5f, 44, paint);
  paint.setColor(0xccff7d4d);
  c->drawCircle((float)w * 0.72f, (float)h * 0.42f, 38, paint);
  return surface->makeImageSnapshot();
}

/** A closed loop of eight points orbiting the origin, bobbing with the
 *  clock — the wire everything on the right hangs from. */
std::vector<glm::vec3> loopAt(float t, float radius, float bob) {
  std::vector<glm::vec3> points;
  for (int i = 0; i < 8; ++i) {
    const float a = (float)i / 8.0f * 2.0f * (float)M_PI + t * 0.25f;
    points.emplace_back(std::cos(a) * radius,
                        std::sin(a * 2.0f + t * 0.4f) * bob,
                        std::sin(a) * radius);
  }
  return points;
}

curve::Spline3 closedLoop(std::vector<glm::vec3> points) {
  curve::Spline3 spline;
  spline.points = std::move(points);
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
  // Long/short cells in golden ratio, each followed by a dark joint, as
  // RUNS: the word is this file's idea and drawing a row of coloured
  // runs is the pattern shelf's, so the shelf's sequence tile draws it.
  const float unit =
      (float)height / (1.618f * 55.0f + 34.0f);  // F(10)/F(9) mix of the span
  std::vector<std::pair<float, sigil::material::Color>> runs;
  for (char term : word) {
    const float cell = term == 'A' ? unit * 1.618f : unit;
    runs.push_back({cell - 3.0f, term == 'A'
                                     ? sigil::material::Color{0.251f, 0.863f,
                                                              1.0f, 1}
                                     : sigil::material::Color{1.0f, 0.471f,
                                                              0.863f, 1}});
    runs.push_back({3.0f, {0.039f, 0.047f, 0.094f, 1}});
  }
  // The shelf lays its runs along +x and the band reads its texture down
  // v, so the strip is that row turned a quarter turn into the size the
  // sweep asks for.
  const sk_sp<SkImage> row = mpattern::sequence(runs).image();
  if (!row) return nullptr;
  sk_sp<SkSurface> surface =
      SkSurfaces::Raster(SkImageInfo::MakeN32Premul(width, height));
  SkCanvas* c = surface->getCanvas();
  c->clear(SkColorSetARGB(255, 10, 12, 24));
  c->rotate(90);
  c->translate(0, -(float)width);
  c->drawImageRect(row, SkRect::MakeWH((float)height, (float)width),
                   SkSamplingOptions(SkFilterMode::kLinear));
  return surface->makeImageSnapshot();
}

}  // namespace

struct ShapeworksLab : sketch::Sketch {
  // Built once per (re)load: an outline recipe, a normal map and a
  // material program are all description, and a reload re-runs setup.
  material::EnvironmentMap studio;
  std::optional<material::Material> gold, chrome, glass, cooked;
  SkPath goldPath, chromePath, glassPath, cookedPath;
  sk_sp<SkImage> backdrop;
  sk_sp<SkImage> marqueeStrip;

  Element describe() {
    // OUTLINE — a chain of path operators wearing a material. Both were
    // cooked in setup and both are static, so the panel states a bake:
    // an outline recipe and a normal map are the description's cost, not
    // the frame's, and a live paint here would re-run one shader over
    // every pixel of the badge for a picture that never changes.
    Element outlineLab =
        custom([this](SkCanvas& canvas, const PaintContext& paint) {
          canvas.save();
          canvas.translate(paint.size.width() * 0.5f,
                           paint.size.height() * 0.5f);
          if (cooked) material::skia::fill(canvas, cookedPath, *cooked);
          canvas.restore();
        })
            .inset(30, 50, 690, 350)
            .cache(Cache::Texture);

    // SURFACES — the literal recipes (materials prebuilt in setup), and
    // static for the same reason, so the same bake is asked for.
    Element materialLab =
        custom([this](SkCanvas& canvas, const PaintContext& paint) {
          (void)paint;
          if (backdrop) canvas.drawImage(backdrop, 0, 0);
          if (gold) material::skia::fill(canvas, goldPath, *gold);
          if (chrome) material::skia::fill(canvas, chromePath, *chrome);
          if (glass) material::skia::fill(canvas, glassPath, *glass);
        })
            .inset(30, 440, 710, 40)
            .cache(Cache::Texture);

    // WIRE — one curve, four sinks. A steel tube swept over it, its own
    // projection stroked on top, a wider sibling loop wearing the
    // Fibonacci band, and a point chain cooked along it.
    Element flight =
        custom([this](SkCanvas& canvas, const PaintContext& paint) {
          const SkSize viewport = paint.size;
          mesh::camera::Camera camera;
          camera.eye = {0, 620, 900};
          camera.target = {0, 0, 0};
          camera.fovYDeg = 40;

          const float t = (float)paint.elapsedSeconds;
          const std::vector<glm::vec3> loop = loopAt(t, 210, 80);
          const curve::Spline3 rail = closedLoop(loop);

          mesh::render::MeshStyle steel;
          steel.baseColor = {0.62f, 0.7f, 0.85f, 1};
          steel.specular = 0.9f;
          mesh::render::drawMesh(canvas,
                                 pop::sweep(rail, pop::profile::circle(),
                                              {.segments = 180, .scale = 7}),
                                 glm::mat4(1.0f), camera, viewport, steel);
          SkPaint wire;
          wire.setAntiAlias(true);
          wire.setStyle(SkPaint::kStroke_Style);
          wire.setStrokeWidth(1);
          wire.setColor4f({1, 1, 1, 0.25f});
          canvas.drawPath(curve::project(rail, camera, viewport, 256), wire);

          // The marquee: a wider sibling loop wearing the Fibonacci
          // band. A swept line charts (across, along) into uv; the strip
          // tiles (one aperiodic period wraps the loop) and the
          // uvTransform's translate IS the scroll.
          mesh::render::MeshStyle band;
          band.texture = marqueeStrip;
          band.tileTexture = true;
          band.baseColor = {1, 1, 1, 0.92f};
          band.lit = false;
          band.uvTransform = SkMatrix::Translate(0, t * 0.11f);
          mesh::render::drawMesh(
              canvas,
              pop::sweep(closedLoop(loopAt(t, 265, 96)),
                           pop::profile::line(),
                           {.segments = 220,
                            .scale = 30,
                            .normals = pop::SweepOptions::Normals::Frame}),
              glm::mat4(1.0f), camera, viewport, band);

          // Sparks: a point CHAIN on the same loop — the scatter, the
          // noise that drifts it, the ramp down `t` and the per-point
          // size variance are one description the runtime cooks, not
          // four passes the sketch wrote by hand.
          const mesh::Cloud sparks =
              pop::on(loop)
                  .count(220)
                  .seed(9)
                  .noise(26.0f, 0.012f)
                  .rampBy({{0.4f, 0.85f, 1.0f, 0.4f}, {1.0f, 0.5f, 0.9f, 0.4f}})
                  .vary(0.6f, 1.0f)
                  .cloud();
          mesh::points::BillboardStyle glow;
          glow.size = 11;
          glow.sizeLane = "size";
          glow.tintLane = "tint";
          mesh::points::drawBillboards(canvas, sparks, camera, viewport, glow);
        })
            .inset(600, 50, 30, 40)
            .clip()
            .cache(Cache::None);

    return stack()
        .child(std::move(outlineLab))
        .child(std::move(materialLab))
        .child(std::move(flight));
  }

  void setup(sketch::SketchContext& ctx) override {
    ctx.canvas(1280, 780);
    ctx.background({0.05f, 0.048f, 0.088f, 1});
    ctx.captureAt(2.6);

    material::skia::install();
    studio = material::EnvironmentMap::studio();

    // The recipe, cooked once. A path operator chain is a DESCRIPTION —
    // the outline it produces is a constant of this file, and so is the
    // bevel normal map that outline casts, so the material over it is
    // built here beside them. Editing any of the three re-runs setup,
    // which is what the reload loop is.
    const ops::PathOp recipe = ops::chain({
        ops::PuckerBloat{0.25f},
        ops::Roughen{3.2f, 8, 3},
        ops::offsetBy(6),
    });
    cookedPath = recipe(star(7, 152, 84, {0, 0}));
    cooked =
        material::kit::gold(material::bevelNormals(cookedPath, 9), studio, {});

    // Badge geometry lives in the surfaces leaf's LOCAL space (540 x 300);
    // bevelNormals() places each normal map at its outline's bounds, so
    // the recipe reads the normal under the pixel it shades.
    goldPath = star(8, 72, 50, {95, 150});
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

    marqueeStrip = fibonacciStrip(96, 1024);

    ctx.composer.render(describe());
  }
};

SIGIL_SKETCH(
    ShapeworksLab, "Start & fixtures",
    "the seam between three libraries in one custom() leaf \xe2\x80\x94 an "
    "operator chain worn as gold, the stock surfaces over bevel maps, and "
    "one curve swept, projected, tiled and scattered")
