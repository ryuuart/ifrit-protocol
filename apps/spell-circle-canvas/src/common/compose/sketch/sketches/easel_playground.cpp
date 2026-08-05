// easel_playground.cpp — THE ARTIST SURFACE, LIVE
// =============================================================================
// SigilShape's easel:: is the layer that reads like sentences: stock
// shapes, fluent dials, one draw() at the end. This playground is its
// prototype in the hot-reload loop — every chain below is meant to be
// EDITED. Change a number, save, watch. Nothing here needs an options
// struct, a lane name, or a matrix.
//
//   left    a shape recipe: star -> bloat -> roughen -> offset, gold
//   middle  the blend tool: spiky coral thing melting into a sky dot
//   right   a wire crossing space: tube + beads + glow + particles
//
// The exact layer stays underneath (Ops/Blend/Curves/Points/Materials)
// whenever a sentence runs out of words — easel objects hand over
// their cooked values (path()/cook()/spline()) at any point.

#include <include/core/SkSurface.h>
#include <sigilshape/Easel.h>
#include <sigilsketch/Sketch.h>

#include <cmath>

using namespace sigil::compose;
namespace easel = sigil::shape::easel;
namespace shape = sigil::shape;

namespace {

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

struct EaselPlayground : sigil::compose::sketch::Sketch {
  shape::materials::Environment studio;
  sk_sp<SkImage> marqueeStrip;

  Element describe(sketch::SketchContext& ctx) {
    // LEFT — a shape recipe wearing gold. Try: .twirl(40), a bigger
    // bloat, .chrome(studio), .fill({1,0.4f,0.6f,1}).
    Element badge =
        custom([this](SkCanvas& canvas, const PaintContext& paint) {
          const float wobble =
              2.0f + 1.5f * std::sin((float)paint.elapsedSeconds * 0.8f);
          easel::shape(easel::star(7, 110, 0.55f))
              .bloat(0.25f)
              .roughen(wobble, 3)
              .offset(6)
              .gold(studio, 9)
              .draw(canvas,
                    {paint.size.width() * 0.5f, paint.size.height() * 0.5f});
        })
            .inset(30, 60, 830, 380)
            .cache(Cache::None);

    // LEFT LOW — the same recipe language, plain paint: a pond of
    // offset rings breathing.
    Element pond =
        custom([](SkCanvas& canvas, const PaintContext& paint) {
          const float t = (float)paint.elapsedSeconds;
          for (int i = 0; i < 5; ++i) {
            const float phase = t * 0.7f - (float)i * 0.55f;
            const float grow = 20.0f * (float)i + 14.0f * std::sin(phase);
            easel::shape(easel::ngon(6, 36))
                .zigzag(4, 18, true)
                .offset(grow)
                .stroke({0.4f + 0.12f * (float)i, 0.8f, 1.0f,
                         0.85f - 0.15f * (float)i},
                        2.5f)
                .draw(canvas,
                      {paint.size.width() * 0.5f, paint.size.height() * 0.5f});
          }
        })
            .inset(30, 440, 830, 40)
            .cache(Cache::None);

    // MIDDLE — the blend tool. Try: .smoothColor(), .every(24),
    // .along(a path).turning().
    Element melt =
        custom([](SkCanvas& canvas, const PaintContext& paint) {
          const float sway =
              40.0f * std::sin((float)paint.elapsedSeconds * 0.6f);
          easel::blend(easel::star(5, 66, 0.45f), easel::dot(56))
              .colors({1.0f, 0.42f, 0.30f, 1}, {0.30f, 0.62f, 1.0f, 1})
              .steps(10)
              .smooth()
              .between(
                  {paint.size.width() * 0.5f + sway, 90},
                  {paint.size.width() * 0.5f - sway, paint.size.height() - 90})
              .draw(canvas);
        })
            .inset(390, 60, 420, 40)
            .cache(Cache::None);

    // RIGHT — a wire crossing space, everything hung on it: a steel
    // tube, particles, and the TILED UNTILEABLE MARQUEE — a
    // Fibonacci-word band scrolling around a second, wider loop.
    // Nothing marquee-shaped exists in the library: it is
    // curves::ribbon (the (across, along) uv chart) + tileTexture +
    // uvTransform, the same verbs any conveyor or ticker uses.
    Element flight =
        custom([this](SkCanvas& canvas, const PaintContext& paint) {
          const SkSize viewport = paint.size;
          shape::space::Camera camera;
          camera.eye = {0, 170, 620};
          camera.target = {0, 0, 0};
          camera.fovYDeg = 40;

          const float t = (float)paint.elapsedSeconds;
          easel::Wire loop = easel::wire({});
          for (int i = 0; i < 8; ++i) {
            const float a = (float)i / 8.0f * 2.0f * (float)M_PI + t * 0.25f;
            loop.through({std::cos(a) * 210, std::sin(a * 2.0f + t * 0.4f) * 80,
                          std::sin(a) * 210});
          }
          loop.closed();

          shape::space::MeshStyle steel;
          steel.baseColor = {0.62f, 0.7f, 0.85f, 1};
          steel.specular = 0.9f;
          shape::space::drawMesh(canvas, loop.tube(7, 180), glm::mat4(1.0f),
                                 camera, viewport, steel);
          loop.draw(canvas, camera, viewport, {1, 1, 1, 0.25f}, 1);

          // The marquee: a wider sibling loop wearing the Fibonacci
          // band. ribbon() charts (across, along) into uv; the strip
          // tiles (one aperiodic period wraps the loop) and the
          // uvTransform's translate IS the scroll.
          easel::Wire orbit = easel::wire({});
          for (int i = 0; i < 8; ++i) {
            const float a = (float)i / 8.0f * 2.0f * (float)M_PI + t * 0.25f;
            orbit.through({std::cos(a) * 265,
                           std::sin(a * 2.0f + t * 0.4f) * 96,
                           std::sin(a) * 265});
          }
          orbit.closed();
          shape::space::MeshStyle band;
          band.texture = marqueeStrip;
          band.tileTexture = true;
          band.baseColor = {1, 1, 1, 0.92f};
          band.ambient = {0.95f, 0.95f, 0.95f, 1};
          band.lights = {};
          band.specular = 0;
          band.uvTransform = SkMatrix::Translate(0, t * 0.11f);
          shape::space::drawMesh(canvas, orbit.ribbon(30, 220), glm::mat4(1.0f),
                                 camera, viewport, band);

          easel::particles()
              .on(loop)
              .count(220)
              .drift(26)
              .size(11, 0.6f)
              .ramp({0.4f, 0.85f, 1.0f, 0.4f}, {1.0f, 0.5f, 0.9f, 0.4f})
              .glow(canvas, camera, viewport);
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
    studio = shape::materials::Environment::studio();
    marqueeStrip = fibonacciStrip(96, 1024);
    ctx.composer.render(describe(ctx));
  }
};

SIGIL_SKETCH(EaselPlayground)
