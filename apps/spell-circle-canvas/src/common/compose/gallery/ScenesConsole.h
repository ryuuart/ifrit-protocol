#pragma once
// The DAEMON WATCH console (console()/LineRing, REVIEW.md #6.2), ported
// from sketch/sketches/hacker_console.cpp: the streaming-console mode as
// pure composition --
//
//   scrollback ..... sigil::compose::console::LineRing + console() -- seq-id keys, so every
//                    append costs ONE mount; scrolled lines keep their
//                    cached pictures (pinned by ComposeConsole test)
//   levels ......... palette-indexed line styles (info/dim/warn/alert)
//   cursor ......... the ONLY volatile node: a block bound to a blink Output
//   fade-out ....... a bg-colored gradient overlay at the top of the panel --
//                    old lines dim without a single line re-patching
//   chrome ......... SDF roundBox panel (border + glow, one shader pass,
//                    cached), scanline grain (uTime sksl, kScreen)
//   grade .......... OCIO exponent (when the build has OCIO)
//
// The whole scene re-renders on every append (~14 Hz, from the setup()
// ticker lambda) and reconciliation prices it at one mount -- the retained
// spine IS the virtualizer.

#include "GalleryCore.h"

#include <sigilcompose/Console.h>
#include <sigilcompose/Material.h>
#include <sigilcompose/Sdf.h>
#if defined(SIGILCOMPOSE_ENABLE_OCIO)
#include <sigilcompose/Ocio.h>
#endif

#include <include/core/SkString.h>
#include <include/effects/SkRuntimeEffect.h>

#include <cmath>
#include <cstdio>
#include <random>

namespace compose_gallery {

namespace daemon_console {

constexpr float kW = kSceneSize.fWidth, kH = kSceneSize.fHeight;

constexpr SkColor4f kInk{0.020f, 0.045f, 0.030f, 1};
constexpr SkColor4f kPanel{0.030f, 0.070f, 0.045f, 0.96f};
constexpr SkColor4f kGreen{0.45f, 0.98f, 0.55f, 1};
constexpr SkColor4f kGreenDim{0.30f, 0.62f, 0.38f, 1};
constexpr SkColor4f kAmber{0.99f, 0.76f, 0.28f, 1};
constexpr SkColor4f kAlert{1.00f, 0.36f, 0.30f, 1};

inline sigil::weave::TextStyle mono(float size, SkColor4f color) {
  sigil::weave::TextStyle s;
  s.shaping.fontSize = size;
  s.paint.foreground.setColor(color.toSkColor());
  s.paint.foreground.setAntiAlias(true);
  return s;
}

inline sk_sp<SkRuntimeEffect> scanEffect() {
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
  uint64_t emitLine(sigil::compose::console::LineRing &ring) {
    char buf[96];
    const int roll = (int)(rng() % 100);
    packet += (int)(rng() % 97);
    if (roll < 8) {
      std::snprintf(buf, sizeof(buf),
                    "!! WARD BREACH sector %02u \xe2\x80\x94 rerouting "
                    "through gate %u",
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
      std::snprintf(buf, sizeof(buf),
                    "   trace %06x :: lattice ok \xc2\xb7 %u pts", packet,
                    (unsigned)(64 + rng() % 900));
      return ring.append(toU8(buf), 0); // dim
    }
    std::snprintf(buf, sizeof(buf),
                  " > daemon[%u] bound port %u \xe2\x80\x94 handshake "
                  "%06x accepted",
                  (unsigned)(rng() % 9), (unsigned)(6000 + rng() % 999),
                  packet);
    return ring.append(toU8(buf));
  }
};

} // namespace daemon_console

struct DaemonConsoleScene final : Scene {
  sigil::compose::console::LineRing ring{256};
  daemon_console::LogGen gen;
  choreograph::Output<float> blink{1.0f};
  double nextAppend = 0.0;

  const char *name() const override { return "daemon console"; }

  sigil::compose::console::Style style() {
    namespace dc = daemon_console;
    sigil::compose::console::Style s;
    s.text = dc::mono(14, dc::kGreen);
    s.palette = {dc::mono(14, dc::kGreenDim), dc::mono(14, dc::kAmber),
                 dc::mono(14, dc::kAlert)};
    s.gap = 3;
    // 22 in the 960x600 sketch. At 24 the feed left four lines of empty
    // panel under the cursor and scrolled anyway, which reads as a feed
    // that has lost its own history. 25 is what the well fits at 17px
    // pitch once the padding clears the chrome.
    s.visibleLines = 25;
    s.cursorColor = dc::kGreen;
    s.cursorWidth = 9;
    s.cursorHeight = 15;
    s.cursorOpacity = &blink;
    return s;
  }

  void setup(Composer &composer, sigil::motion::Ticker &ticker) override {
    namespace dc = daemon_console;
    blink = 1.0f;
    nextAppend = 0.0;
    ring.clear(); // scenes re-activate; seq ids stay monotonic
    gen = dc::LogGen{};

#if defined(SIGILCOMPOSE_ENABLE_OCIO)
    composer.setView(ocio::exponent(1.08f));
#endif

    for (int i = 0; i < 8; ++i)
      gen.emitLine(ring); // a little history at boot

    // The data-side feed: blink is the only bound Output; appends re-render
    // and reconciliation prices each one at a single mount (O(1)).
    ticker.add([this, &composer, t = 0.0](double dt) mutable {
      t += dt;
      blink = std::fmod(t, 0.9) < 0.55 ? 1.0f : 0.12f;
      if (t >= nextAppend) {
        nextAppend = t + 0.055 + (double)(gen.rng() % 60) / 1000.0;
        gen.emitLine(ring);
        composer.render(describe());
      }
      return true;
    });

    composer.render(describe());
  }

  Element describe() {
    namespace dc = daemon_console;

    // Panel chrome: one-pass SDF (fill + border + glow), cached between
    // layouts (geometry tier).
    Material chrome = sdf::material(
        sdf::roundBox(14), {.fill = dc::kPanel,
                            .borderWidth = 1.5f,
                            .borderColor = {0.45f, 0.98f, 0.55f, 0.55f},
                            .glowRadius = 9,
                            .glowColor = {0.30f, 0.95f, 0.45f, 0.35f}});

    // Fade the OLDEST visible lines: a bg-colored gradient painted over the
    // panel top -- zero line nodes touched, fully cached. Coordinates are the
    // OVERLAY's local space (its box is 90px tall).
    Material fade = Material::linear(
        {0, 0}, {0, 90},
        {{0.0f, {dc::kPanel.fR, dc::kPanel.fG, dc::kPanel.fB, 1.0f}},
         {1.0f, {dc::kPanel.fR, dc::kPanel.fG, dc::kPanel.fB, 0.0f}}});

    return stack()
        .fill(Material::linear({0, 0}, {0, dc::kH},
                               {{0.0f, {0.03f, 0.065f, 0.042f, 1}},
                                {1.0f, dc::kInk}}))
        .child(
            // The SDF style reserves pad inside the box for the glow, and
            // the DRAWN border lands 30px in, not the ~24 this comment used
            // to claim: at padding(34, 30) the rule ran straight through the
            // cap-height of DAEMON WATCH and clipped the uplink's last
            // glyph on the corner arc. The content padding has to CLEAR the
            // drawn chrome, not meet it.
            box().column().inset(44, 40, 44, 40).fill(chrome)
                .clip().padding(46, 44)
                .child(box().row().gap(10).margin(0, 0, 0, 10)
                           .child(text(toU8("DAEMON WATCH"),
                                       dc::mono(16, dc::kGreen)))
                           .child(box().grow(1))
                           .child(text(toU8("uplink: flooded-causeway"
                                            "\xc2\xb7" "7"),
                                       dc::mono(13, dc::kGreenDim))))
                .child(box().column().grow(1).clip()
                           .child(sigil::compose::console::console(ring, style()))
                           .zIndex(1))
                .child(box().height(90).inset(30, 58, 30, 0)
                           .fill(fade).zIndex(2)))
        // living scanlines across everything
        .child(box().inset(0).zIndex(3)
                   .fill(Material::sksl(dc::scanEffect()))
                   .blend(SkBlendMode::kScreen));
  }
};

} // namespace compose_gallery
