// transit.cpp — "NIGHT NETWORK", a living metro map.
//
//   ./build/bin/Release/ComposeSketch \
//       src/common/compose/sketch/sketches/transit.cpp
//
// The "components are LINES, not boxes" leg, end to end:
//
//   rails ........... rail(anchors, routers::octilinear) — ordered station
//                     keys with normalized anchors; the metro 45° convention
//   draw-on ......... trim(0, &reveal) per line, Hold-staggered Choreograph
//                     ramps: each line DRAWS ITSELF across the city
//   stations ........ SDF circle chrome (one shader: fill + border + glow),
//                     geometry-dependent → cached between layouts
//   interchange ..... SDF star, bound glow breathing
//   ground / grade .. Material ramp + film grain + OCIO exponent grade
//
// Everything is a value: zero custom() lambdas, zero absolute endpoint
// coordinates (move a station box and its lines follow).
//
// Headless: ComposeSketch <this> --frame transit.png --at 2.8

#include <sigilsketch/Sketch.h>

#include <sigilcompose/Material.h>
#include <sigilcompose/Routers.h>
#include <sigilcompose/Sdf.h>
#if defined(SIGILCOMPOSE_ENABLE_OCIO)
#include <sigilcompose/Ocio.h>
#endif

#include <include/core/SkString.h>
#include <include/effects/SkRuntimeEffect.h>

using namespace sigil::compose;
using namespace sigil::compose::util;
namespace ch = choreograph;
using namespace std::chrono_literals;

namespace {

constexpr float W = 960, H = 640;

constexpr SkColor4f kInk{0.035f, 0.040f, 0.070f, 1};
constexpr SkColor4f kInkHigh{0.075f, 0.085f, 0.140f, 1};
constexpr SkColor4f kBone{0.94f, 0.92f, 0.87f, 1};
constexpr SkColor4f kAsh{0.55f, 0.56f, 0.63f, 1};
constexpr SkColor4f kEmber{0.99f, 0.53f, 0.19f, 1};
constexpr SkColor4f kCyan{0.36f, 0.86f, 0.96f, 1};
constexpr SkColor4f kViolet{0.71f, 0.51f, 0.99f, 1};

sigil::weave::TextStyle type(float size, SkColor4f color, float tracking = 0) {
  sigil::weave::TextStyle s;
  s.shaping.fontSize = size;
  s.shaping.letterSpacing = tracking;
  s.paint.foreground.setColor(color.toSkColor());
  s.paint.foreground.setAntiAlias(true);
  return s;
}

/** A station: a keyed box wearing one-pass SDF chrome (fill+border+glow). */
Element station(const char *key, float x, float y, SkColor4f accent,
                float size = 20) {
  const float h = size * 0.5f;
  return box().key(key).width(size).height(size)
      .inset(x - h, y - h, W - x - h, H - y - h).absolute()
      .fill(sdf::material(sdf::circle(),
                          {.fill = kInkHigh,
                           .borderWidth = 2.5f,
                           .borderColor = kBone,
                           .glowRadius = 5,
                           .glowColor = accent}));
}

/** A transit line: octilinear rail through its stations, drawing on. */
Element line(std::vector<Anchor> stops, SkColor4f color,
             const choreograph::Output<float> *reveal) {
  PathFormat stroke;
  stroke.width = 6;
  stroke.strokeFill = Fill::color(color);
  return rail(std::move(stops), routers::octilinear(14))
      .absolute().inset(0)
      .trim(0.0f, reveal)
      .foreground(stroke);
}

Element label(const char *text8, float x, float y, SkColor4f c = kAsh) {
  return text(toU8(text8), type(13, c, 1.5f))
      .absolute().inset(x, y, 0, 0);
}

} // namespace

struct Transit : sketch::Sketch {
  ch::Output<float> emberReveal{0}, cyanReveal{0}, violetReveal{0};
  ch::Output<float> hubGlow{0};

  void setup(sketch::SketchContext &ctx) override {
    ctx.canvas(W, H);
    ctx.background(kInk);

    auto &tl = ctx.ticker.timeline();
    auto drawOn = [&](ch::Output<float> &r, float delay) {
      r = 0.0f;
      tl.apply(&r).then<ch::Hold>(0.0f, delay).then<ch::RampTo>(
          1.0f, 1.4f, &ch::easeInOutQuad);
    };
    drawOn(emberReveal, 0.15f);
    drawOn(cyanReveal, 0.55f);
    drawOn(violetReveal, 0.95f);

    ctx.ticker.add([this, t = 0.0](double dt) mutable {
      t += dt;
      hubGlow = 6.0f + 4.0f * (float)std::sin(t * 2.1);
      return true;
    });

#if defined(SIGILCOMPOSE_ENABLE_OCIO)
    ctx.composer.setView(ocio::exponent(1.10f));
#endif
    ctx.composer.render(describe());
  }

  Element describe() {
    // The interchange hub wears a breathing SDF star (bound glow).
    Element hub =
        box().key("hub").width(34).height(34)
            .inset(480 - 17, 300 - 17, W - 480 - 17, H - 300 - 17).absolute()
            .fill(sdf::material(sdf::star(8, 3.2f),
                                {.fill = kBone,
                                 .borderWidth = 2,
                                 .borderColor = kInk,
                                 .glowRadius = 10,
                                 .glowColor = kEmber})
                      .uniform("uGlowR", &hubGlow))
            .zIndex(3);

    return stack()
        .fill(Material::linear({0, 0}, {0, H},
                               {{0.0f, kInkHigh}, {0.5f, kInk}, {1.0f, kInk}}))
        // rails beneath stations
        .child(line({{"nw"}, {"n1"}, {"hub"}, {"s1"}, {"se"}}, kEmber,
                    &emberReveal).zIndex(1))
        .child(line({{"w1"}, {"hub"}, {"e1"}, {"e2"}}, kCyan, &cyanReveal)
                   .zIndex(1))
        .child(line({{"sw"}, {"s1"}, {"hub"}, {"n1"}, {"ne"}}, kViolet,
                    &violetReveal).zIndex(1))
        // stations above rails
        .child(station("nw", 150, 120, kEmber).zIndex(2))
        .child(station("n1", 360, 170, kViolet).zIndex(2))
        .child(station("ne", 800, 130, kViolet).zIndex(2))
        .child(station("w1", 130, 330, kCyan).zIndex(2))
        .child(hub)
        .child(station("e1", 690, 340, kCyan).zIndex(2))
        .child(station("e2", 850, 420, kCyan).zIndex(2))
        .child(station("sw", 190, 520, kViolet).zIndex(2))
        .child(station("s1", 400, 450, kEmber).zIndex(2))
        .child(station("se", 760, 520, kEmber).zIndex(2))
        // a few labels + the title block
        .child(label("EMBER GATE", 496, 268, kBone).zIndex(4))
        .child(label("north quay", 372, 148).zIndex(4))
        .child(label("saltmarsh", 200, 528).zIndex(4))
        .child(label("the flooded causeway", 660, 548).zIndex(4))
        .child(box().column().absolute().inset(40, 40, 0, 0).zIndex(4)
                   .child(text(toU8("NIGHT NETWORK"), type(34, kBone, 2)))
                   .child(text(toU8("three lines · one interchange"),
                               type(15, kAsh, 1))
                              .margin(0, 6, 0, 0)));
  }

  void update(double, sketch::SketchContext &) override {}
};

SIGIL_SKETCH(Transit)
