// hacker_console.cpp — "DAEMON WATCH", the sci-fi streaming console.
//
//   ./build/bin/Release/ComposeSketch \
//       src/common/compose/sketch/sketches/hacker_console.cpp
//
// The console mode, end to end, as pure composition:
//
//   scrollback ..... console::LineRing + console() — seq-id keys, so every
//                    append costs ONE mount; scrolled lines keep their
//                    cached pictures (pinned by ComposeConsole test)
//   levels ......... palette-indexed line styles (info/dim/warn/alert)
//   cursor ......... the ONLY volatile node: a block bound to a blink Output
//   fade-out ....... a bg-colored gradient overlay at the top of the panel —
//                    old lines dim without a single line re-patching
//   chrome ......... SDF roundBox panel (border + glow, one shader pass,
//                    cached), scanline grain (uTime sksl, soft-light)
//   grade .......... OCIO exponent
//
// The whole scene re-renders on every append (~14 Hz) and reconciliation
// prices it at one mount — the retained spine IS the virtualizer.
//
// Headless: ComposeSketch <this> --frame console.png --at 4.0

#include <sigilsketch/Sketch.h>

#include <sigilcompose/Console.h>
#include <sigilcompose/Material.h>
#include <sigilcompose/Sdf.h>
#if defined(SIGILCOMPOSE_ENABLE_OCIO)
#include <sigilcompose/Ocio.h>
#endif

#include <include/core/SkString.h>
#include <include/effects/SkRuntimeEffect.h>

#include <cstdio>
#include <random>

using namespace sigil::compose;
using namespace sigil::compose::util;
namespace ch = choreograph;

namespace {

constexpr float W = 960, H = 600;

constexpr SkColor4f kInk{0.020f, 0.045f, 0.030f, 1};
constexpr SkColor4f kPanel{0.030f, 0.070f, 0.045f, 0.96f};
constexpr SkColor4f kGreen{0.45f, 0.98f, 0.55f, 1};
constexpr SkColor4f kGreenDim{0.30f, 0.62f, 0.38f, 1};
constexpr SkColor4f kAmber{0.99f, 0.76f, 0.28f, 1};
constexpr SkColor4f kAlert{1.00f, 0.36f, 0.30f, 1};

sigil::weave::TextStyle mono(float size, SkColor4f color) {
  sigil::weave::TextStyle s;
  s.shaping.fontSize = size;
  s.paint.foreground.setColor(color.toSkColor());
  s.paint.foreground.setAntiAlias(true);
  return s;
}

sk_sp<SkRuntimeEffect> scanEffect() {
  static const char *kSkSL = R"(
    uniform float uTime;
    half4 main(float2 p) {
      float band = 0.5 + 0.5 * sin(p.y * 1.9 - uTime * 14.0);
      half a = half(0.05 * band);
      return half4(half3(0.35, 1.0, 0.55) * a, a); // premultiplied
    }
  )";
  static auto effect = [] {
    auto [fx, err] = SkRuntimeEffect::MakeForShader(SkString(kSkSL));
    if (!fx)
      SkDebugf("scanline shader: %s\n", err.c_str());
    return fx;
  }();
  return effect;
}

/** Seeded pseudo-log: plausible daemon chatter with levels. */
struct LogGen {
  std::mt19937 rng{2077};
  int packet = 41210;
  uint64_t emit(console::LineRing &ring) {
    char buf[96];
    const int roll = (int)(rng() % 100);
    packet += (int)(rng() % 97);
    if (roll < 8) {
      std::snprintf(buf, sizeof(buf),
                    "!! WARD BREACH sector %02u — rerouting through gate %u",
                    (unsigned)(rng() % 13), (unsigned)(rng() % 7));
      return ring.append(toU8(buf), 2); // alert
    }
    if (roll < 22) {
      std::snprintf(buf, sizeof(buf),
                    " ~ sigil flux unstable: %0.2f mS, damping applied",
                    0.4 + (double)(rng() % 90) / 100.0);
      return ring.append(toU8(buf), 1); // warn
    }
    if (roll < 55) {
      std::snprintf(buf, sizeof(buf), "   trace %06x :: lattice ok · %u pts",
                    packet, (unsigned)(64 + rng() % 900));
      return ring.append(toU8(buf), 0); // dim
    }
    std::snprintf(buf, sizeof(buf),
                  " > daemon[%u] bound port %u — handshake %06x accepted",
                  (unsigned)(rng() % 9), (unsigned)(6000 + rng() % 999),
                  packet);
    return ring.append(toU8(buf));
  }
};

} // namespace

struct HackerConsole : sketch::Sketch {
  console::LineRing ring{256};
  LogGen gen;
  ch::Output<float> blink{1.0f};
  double nextAppend = 0.0;

  console::Style style() {
    console::Style s;
    s.text = mono(14, kGreen);
    s.palette = {mono(14, kGreenDim), mono(14, kAmber), mono(14, kAlert)};
    s.gap = 3;
    s.visibleLines = 22;
    s.cursorColor = kGreen;
    s.cursorWidth = 9;
    s.cursorHeight = 15;
    s.cursorOpacity = &blink;
    return s;
  }

  void setup(sketch::SketchContext &ctx) override {
    ctx.canvas(W, H);
    ctx.background(kInk);
    nextAppend = 0.0;

    ctx.ticker.add([this, t = 0.0](double dt) mutable {
      t += dt;
      blink = std::fmod(t, 0.9) < 0.55 ? 1.0f : 0.12f;
      return true;
    });

#if defined(SIGILCOMPOSE_ENABLE_OCIO)
    ctx.composer.setView(ocio::exponent(1.08f));
#endif
    for (int i = 0; i < 8; ++i)
      gen.emit(ring); // a little history at boot
    ctx.composer.render(describe());
  }

  Element describe() {
    // Panel chrome: one-pass SDF (fill + border + glow), cached between
    // layouts (geometry tier).
    Material chrome = sdf::material(
        sdf::roundBox(14), {.fill = kPanel,
                            .borderWidth = 1.5f,
                            .borderColor = {0.45f, 0.98f, 0.55f, 0.55f},
                            .glowRadius = 9,
                            .glowColor = {0.30f, 0.95f, 0.45f, 0.35f}});

    // Fade the OLDEST visible lines: a bg-colored gradient painted over the
    // panel top — zero line nodes touched, fully cached. Coordinates are the
    // OVERLAY's local space (its box is 90px tall).
    Material fade = Material::linear(
        {0, 0}, {0, 90},
        {{0.0f, {kPanel.fR, kPanel.fG, kPanel.fB, 1.0f}},
         {1.0f, {kPanel.fR, kPanel.fG, kPanel.fB, 0.0f}}});

    return stack()
        .fill(Material::linear({0, 0}, {0, H},
                               {{0.0f, {0.03f, 0.065f, 0.042f, 1}},
                                {1.0f, kInk}}))
        .child(
            // The SDF style reserves ~24px of pad inside the box (glow
            // room), so the panel's content padding matches it — children
            // sit inside the DRAWN chrome, not the layout box.
            box().column().absolute().inset(48, 42, 48, 42).fill(chrome)
                .clip().padding(34, 30)
                .child(box().row().gap(10).margin(0, 0, 0, 10)
                           .child(text(toU8("DAEMON WATCH"),
                                       mono(16, kGreen)))
                           .child(box().grow(1))
                           .child(text(toU8("uplink: flooded-causeway·7"),
                                       mono(13, kGreenDim))))
                .child(box().column().grow(1).clip()
                           .child(console::console(ring, style()))
                           .zIndex(1))
                .child(box().height(90).absolute().inset(30, 58, 30, 0)
                           .fill(fade).zIndex(2)))
        // living scanlines across everything
        .child(box().absolute().inset(0).zIndex(3)
                   .fill(Material::sksl(scanEffect()))
                   .blend(SkBlendMode::kScreen));
  }

  void update(double elapsed, sketch::SketchContext &ctx) override {
    if (elapsed < nextAppend)
      return;
    nextAppend = elapsed + 0.055 + (double)(gen.rng() % 60) / 1000.0;
    gen.emit(ring);
    ctx.composer.render(describe()); // O(1): one mount, prune does the rest
  }
};

SIGIL_SKETCH(HackerConsole)
