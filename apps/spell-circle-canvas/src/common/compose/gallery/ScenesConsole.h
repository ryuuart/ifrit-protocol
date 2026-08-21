#pragma once
// A streaming terminal feed built entirely out of composition, in the
// science-fiction console idiom: green phosphor, scanlines, a blinking block
// cursor and a log that scrolls forever.
//
//   scrollback ..... feed::TextRing + feed(). Rows are keyed by sequence id,
//                    so an append reconciles as ONE mount and every row
//                    already on screen keeps its cached picture. The
//                    ComposeFeed tests pin that property.
//   levels ......... row styles named in a weave::StyleSet (the base voice
//                    plus dim / warn / alert)
//   typing ......... each row enters on its own settling fx::typeOn track:
//                    the glyphs appear left to right, the track settles, and
//                    the row caches like every row above it. A glyph
//                    entrance costs the feed nothing once it is over.
//   cursor ......... the ONLY node volatile forever: a block bound to a
//                    blink Output, appended after the rows
//   fade-out ....... a ground-coloured gradient laid over the top of the
//                    panel, so the oldest visible rows dim without any row
//                    node being re-patched
//   chrome ......... an SDF roundBox panel giving fill, border and glow in
//                    one shader pass, plus a time-driven scanline overlay
//   grade .......... an OCIO exponent, when the build has OpenColorIO
//
// The whole scene is re-rendered on every append — several times a second,
// from the ticker lambda in setup() — and reconciliation still prices each
// one at a single mount. The retained instance tree is what makes that work;
// there is no separate virtualizer.

#include <sigilcompose/Feed.h>
#include <sigilcompose/Material.h>
#include <sigilcompose/Sdf.h>
#include <sigilcompose/TextFx.h>

#include "GalleryCore.h"
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
  static const char* kSkSL = R"(
    uniform float uTime;
    half4 main(float2 p) {
      float band = 0.5 + 0.5 * sin(p.y * 1.9 - uTime * 14.0);
      half a = half(0.05 * band);
      return half4(half3(0.35, 1.0, 0.55) * a, a); // premultiplied
    }
  )";
  static auto effect = [] {
    auto [fx, err] = SkRuntimeEffect::MakeForShader(SkString(kSkSL));
    if (!fx) SkDebugf("scanline shader: %s\n", err.c_str());
    return fx;
  }();
  return effect;
}

/** Seeded pseudo-log: plausible daemon chatter with levels. */
struct LogGen {
  std::mt19937 rng{2077};
  int packet = 41210;
  uint64_t emitRow(sigil::compose::feed::TextRing& ring) {
    char buf[96];
    const int roll = (int)(rng() % 100);
    packet += (int)(rng() % 97);
    if (roll < 8) {
      std::snprintf(buf, sizeof(buf),
                    "!! WARD BREACH sector %02u \xe2\x80\x94 rerouting "
                    "through gate %u",
                    (unsigned)(rng() % 13), (unsigned)(rng() % 7));
      return ring.append({toU8(buf), "alert"});
    }
    if (roll < 22) {
      std::snprintf(buf, sizeof(buf),
                    " ~ sigil flux unstable: %0.2f mS, damping applied",
                    0.4 + (double)(rng() % 90) / 100.0);
      return ring.append({toU8(buf), "warn"});
    }
    if (roll < 55) {
      std::snprintf(buf, sizeof(buf),
                    "   trace %06x :: lattice ok \xc2\xb7 %u pts", packet,
                    (unsigned)(64 + rng() % 900));
      return ring.append({toU8(buf), "dim"});
    }
    std::snprintf(buf, sizeof(buf),
                  " > daemon[%u] bound port %u \xe2\x80\x94 handshake "
                  "%06x accepted",
                  (unsigned)(rng() % 9), (unsigned)(6000 + rng() % 999),
                  packet);
    return ring.append({toU8(buf)});
  }
};

}  // namespace daemon_console

struct DaemonConsoleScene final : Scene {
  sigil::compose::feed::TextRing ring{256};
  daemon_console::LogGen gen;
  choreograph::Output<float> blink{1.0f};
  double nextAppend = 0.0;

  const char* name() const override { return "daemon console"; }

  sigil::compose::feed::TextOptions options() {
    namespace dc = daemon_console;
    sigil::compose::feed::TextOptions o;
    o.styles.base(dc::mono(14, dc::kGreen))
        .set("dim", dc::mono(14, dc::kGreenDim))
        .set("warn", dc::mono(14, dc::kAmber))
        .set("alert", dc::mono(14, dc::kAlert));
    o.window.gap = 3;
    // Tuned to the panel: at the style's 17 px line pitch this is what the
    // well holds once the padding has cleared the drawn chrome. Setting it
    // lower leaves empty panel under the cursor while the feed scrolls
    // anyway, which reads as a terminal that has lost its own history.
    o.window.visible = 25;
    // The boot history cascades in; each later append is the only new mount
    // in its patch, so a live row starts typing the instant it arrives.
    o.window.entrance = {.eachMs = 26};
    return o;
  }

  void setup(Composer& composer, sigil::motion::Ticker& ticker) override {
    namespace dc = daemon_console;
    blink = 1.0f;
    nextAppend = 0.0;
    ring.clear();  // scenes re-activate; seq ids stay monotonic
    gen = dc::LogGen{};

#if defined(SIGILCOMPOSE_ENABLE_OCIO)
    composer.setView(ocio::exponent(1.08f));
#endif

    for (int i = 0; i < 8; ++i) gen.emitRow(ring);  // a little history at boot

    // The data-side feed: blink is the only value bound forever; appends
    // re-render and reconciliation prices each one at a single mount (O(1)).
    ticker.add([this, &composer, t = 0.0](double dt) mutable {
      t += dt;
      blink = std::fmod(t, 0.9) < 0.55 ? 1.0f : 0.12f;
      if (t >= nextAppend) {
        nextAppend = t + 0.055 + (double)(gen.rng() % 60) / 1000.0;
        gen.emitRow(ring);
        composer.render(describe());
      }
      return true;
    });

    composer.render(describe());
  }

  /** One log row: the line in the style it names, typed on by a track that
   *  SETTLES — while it runs the row paints live, and the frame it reaches
   *  the end the row goes back to being a cached static leaf. */
  static Element row(const sigil::compose::feed::TextRow& r,
                     const sigil::weave::StyleSet& styles) {
    return text(r.text, styles[r.style])
        .fx({.effect = fx::typeOn(),
             .stagger = {.eachMs = 7, .durationMs = 40},
             .progress = animate(from(0.0f).to(1.0f),
                                 {300ms, &choreograph::easeNone})});
  }

  Element describe() {
    namespace dc = daemon_console;
    namespace feed = sigil::compose::feed;

    // Panel chrome: one-pass SDF (fill + border + glow), cached between
    // layouts (geometry tier).
    Material chrome = sdf::material(
        sdf::roundBox(14), {.fill = dc::kPanel,
                            .borderWidth = 1.5f,
                            .borderColor = {0.45f, 0.98f, 0.55f, 0.55f},
                            .glowRadius = 9,
                            .glowColor = {0.30f, 0.95f, 0.45f, 0.35f}});

    // Fade the OLDEST visible rows: a bg-colored gradient painted over the
    // panel top -- zero row nodes touched, fully cached. Coordinates are the
    // OVERLAY's local space (its box is 90px tall).
    Material fade = Material::linear(
        {0, 0}, {0, 90},
        {{0.0f, {dc::kPanel.fR, dc::kPanel.fG, dc::kPanel.fB, 1.0f}},
         {1.0f, {dc::kPanel.fR, dc::kPanel.fG, dc::kPanel.fB, 0.0f}}});

    const feed::TextOptions o = options();
    // The cursor is chrome, not a row: it goes after the rows as an ordinary
    // child of the column the feed returns.
    Element well = feed::feed(ring, o.window, [&](const feed::TextRow& r) {
      return row(r, o.styles);
    });
    well.child(box()
                   .width(9)
                   .height(15)
                   .fill(Fill::color(dc::kGreen))
                   .opacity(&blink)
                   .key("caret"));

    return stack()
        .fill(Material::linear(
            {0, 0}, {0, dc::kH},
            {{0.0f, {0.03f, 0.065f, 0.042f, 1}}, {1.0f, dc::kInk}}))
        .child(
            // The SDF material reserves space inside the box for its glow, so
            // the DRAWN border sits well inside the node's own edge. The
            // content padding has to clear that drawn border rather than meet
            // the box edge; too little and the border rule cuts through the
            // header's cap-height and the right-hand text is clipped by the
            // corner arc.
            box()
                .column()
                .inset(44, 40, 44, 40)
                .fill(chrome)
                .clip()
                .padding(46, 44)
                .child(box()
                           .row()
                           .gap(10)
                           .margin(0, 0, 0, 10)
                           .child(text(toU8("DAEMON WATCH"),
                                       dc::mono(16, dc::kGreen)))
                           .child(box().grow(1))
                           .child(text(toU8("uplink: flooded-causeway"
                                            "\xc2\xb7"
                                            "7"),
                                       dc::mono(13, dc::kGreenDim))))
                .child(box()
                           .column()
                           .grow(1)
                           .clip()
                           .child(std::move(well))
                           .zIndex(1))
                .child(
                    box().height(90).inset(30, 58, 30, 0).fill(fade).zIndex(2)))
        // living scanlines across everything
        .child(box()
                   .inset(0)
                   .zIndex(3)
                   .fill(Material::sksl(dc::scanEffect()))
                   .blend(SkBlendMode::kScreen));
  }
};

}  // namespace compose_gallery
