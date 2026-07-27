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

#include <sigilsketch/Sketch.h>

#include <sigilshape/Easel.h>

#include <cmath>

using namespace sigil::compose;
namespace easel = sigil::shape::easel;
namespace shape = sigil::shape;

struct EaselPlayground : sigil::compose::sketch::Sketch {
  shape::materials::Environment studio;

  Element describe(sketch::SketchContext &ctx) {
    // LEFT — a shape recipe wearing gold. Try: .twirl(40), a bigger
    // bloat, .chrome(studio), .fill({1,0.4f,0.6f,1}).
    Element badge =
        custom([this](SkCanvas &canvas, const PaintContext &paint) {
          const float wobble =
              2.0f + 1.5f * std::sin((float)paint.elapsedSeconds * 0.8f);
          easel::shape(easel::star(7, 110, 0.55f))
              .bloat(0.25f)
              .roughen(wobble, 3)
              .offset(6)
              .gold(studio, 9)
              .draw(canvas, {paint.size.width() * 0.5f,
                             paint.size.height() * 0.5f});
        })
            .inset(30, 60, 830, 380)
            .cache(Cache::None);

    // LEFT LOW — the same recipe language, plain paint: a pond of
    // offset rings breathing.
    Element pond =
        custom([](SkCanvas &canvas, const PaintContext &paint) {
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
                .draw(canvas, {paint.size.width() * 0.5f,
                               paint.size.height() * 0.5f});
          }
        })
            .inset(30, 440, 830, 40)
            .cache(Cache::None);

    // MIDDLE — the blend tool. Try: .smoothColor(), .every(24),
    // .along(a path).turning().
    Element melt =
        custom([](SkCanvas &canvas, const PaintContext &paint) {
          const float sway =
              40.0f * std::sin((float)paint.elapsedSeconds * 0.6f);
          easel::blend(easel::star(5, 66, 0.45f), easel::dot(56))
              .colors({1.0f, 0.42f, 0.30f, 1}, {0.30f, 0.62f, 1.0f, 1})
              .steps(10)
              .smooth()
              .between({paint.size.width() * 0.5f + sway, 90},
                       {paint.size.width() * 0.5f - sway,
                        paint.size.height() - 90})
              .draw(canvas);
        })
            .inset(390, 60, 420, 40)
            .cache(Cache::None);

    // RIGHT — a wire crossing space, everything hung on it.
    Element flight =
        custom([](SkCanvas &canvas, const PaintContext &paint) {
          const SkSize viewport = paint.size;
          shape::space::Camera camera;
          camera.eye = {0, 170, 620};
          camera.target = {0, 0, 0};
          camera.fovYDeg = 40;

          const float t = (float)paint.elapsedSeconds;
          easel::Wire loop = easel::wire({});
          for (int i = 0; i < 8; ++i) {
            const float a =
                (float)i / 8.0f * 2.0f * (float)M_PI + t * 0.25f;
            loop.through({std::cos(a) * 210,
                          std::sin(a * 2.0f + t * 0.4f) * 80,
                          std::sin(a) * 210});
          }
          loop.closed();

          shape::space::MeshStyle steel;
          steel.baseColor = {0.62f, 0.7f, 0.85f, 1};
          steel.specular = 0.9f;
          shape::space::drawMesh(canvas, loop.tube(7, 180), SkM44(),
                                 camera, viewport, steel);
          loop.draw(canvas, camera, viewport, {1, 1, 1, 0.25f}, 1);

          easel::particles()
              .on(loop)
              .count(220)
              .drift(26)
              .size(11, 0.6f)
              .ramp({0.4f, 0.85f, 1.0f, 0.4f}, {1.0f, 0.5f, 0.9f, 0.4f})
              .glow(canvas, camera, viewport);
        })
            .inset(840, 60, 30, 40)
            .cache(Cache::None);

    return stack()
        .child(std::move(badge))
        .child(std::move(pond))
        .child(std::move(melt))
        .child(std::move(flight));
  }

  void setup(sketch::SketchContext &ctx) override {
    ctx.canvas(1240, 760);
    ctx.background({0.045f, 0.045f, 0.085f, 1});
    ctx.captureAt(2.6);
    studio = shape::materials::Environment::studio();
    ctx.composer.render(describe(ctx));
  }
};

SIGIL_SKETCH(EaselPlayground)
