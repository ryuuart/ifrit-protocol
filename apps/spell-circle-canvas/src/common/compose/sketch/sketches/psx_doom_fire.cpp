// psx_doom_fire.cpp — STUDY 01: the DOOM PSX fire, 1995.
//
// Reference (all read directly, numbers taken verbatim):
//   * Fabien Sanglard, "How DOOM fire was done"
//     https://fabiensanglard.net/doom_fire_psx/
//   * Sanglard's runnable reference implementation
//     https://github.com/fabiensanglard/DoomFirePSX/blob/master/flames.html
//     — FIRE_WIDTH = 320, FIRE_HEIGHT = 168, CJS_TICKER_FPS = 27, the exact
//       37-entry `rgbs` palette, the seed row written ONCE at setup, and the
//       "black pixels need to be transparent to show DOOM logo" alpha key.
//     Reverse-engineered from the Doom 64 (N64) disassembly by Samuel
//     Villarreal (@SVKaiser); id Software / Williams, PlayStation, 1995.
//   * Cross-checks: filipedeschamps/doom-fire-algorithm (byte-identical
//     palette, a decay-only variant), lodev.org/cgtutor/fire.html and
//     hanshq.net/fire.html (the OTHER two fire lineages — both re-randomize
//     the source row every frame; DOOM's calm comes from NOT doing that).
//
// Why this is a SigilCompose stress test, not a screensaver:
//   The fire is a STATEFUL cellular automaton — every cell's next value
//   reads its neighbours' CURRENT values out of a persistent buffer.
//   Material::sksl() is the opposite shape: a pure function of
//   (position, uTime, uniforms) re-evaluated from scratch each frame, with
//   no ping-pong/feedback texture anywhere in the public API. So the state
//   machine has no home in Material at all — it lives in a CPU buffer this
//   sketch owns, stepped behind a FIXED-TIMESTEP accumulator at the
//   historical 27 Hz inside ctx.ticker.add(), rasterized into an SkBitmap
//   once per SIM tick, and blitted by a custom() + Cache::None leaf every
//   RENDER frame. The decoupling is the point: the automaton must not
//   speed up with the frame rate.
//
//   ./build/bin/Release/ComposeSketch \
//       src/common/compose/sketch/sketches/psx_doom_fire.cpp \
//       --frame /tmp/psx_doom_fire.png --at 6.0

#include <sigilsketch/Sketch.h>

#include <sigilcompose/Console.h>
#include <sigilcompose/Kinetic.h>
#include <sigilcompose/Material.h>

#include <sigilweave/ports/SystemFontManager.h>

#include <include/core/SkBitmap.h>
#include <include/core/SkCanvas.h>
#include <include/core/SkFontMgr.h>
#include <include/core/SkImage.h>
#include <include/core/SkPaint.h>
#include <include/core/SkSamplingOptions.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>

using namespace sigil::compose;
using namespace sigil::compose::util;
using namespace std::chrono_literals;
namespace ch = choreograph;

namespace {

// ---------------------------------------------------------------------------
// §5 — the palette. flames.html's `rgbs` array, index = heat value [0,36].

constexpr uint32_t kPalette[37] = {
    0x070707, 0x1F0707, 0x2F0F07, 0x470F07, 0x571707, 0x671F07, 0x771F07,
    0x8F2707, 0x9F2F07, 0xAF3F07, 0xBF4707, 0xC74707, 0xDF4F07, 0xDF5707,
    0xDF5707, 0xD75F07, 0xD75F07, 0xD7670F, 0xCF6F0F, 0xCF770F, 0xCF7F0F,
    0xCF8717, 0xC78717, 0xC78F17, 0xC7971F, 0xBF9F1F, 0xBF9F1F, 0xBFA727,
    0xBFA727, 0xBFAF2F, 0xB7AF2F, 0xB7B72F, 0xB7B737, 0xCFCF6F, 0xDFDF9F,
    0xEFEFC7, 0xFFFFFF};

constexpr SkColor4f hex(uint32_t rgb, float a = 1.0f) {
  return {(float)((rgb >> 16) & 0xFF) / 255.0f,
          (float)((rgb >> 8) & 0xFF) / 255.0f, (float)(rgb & 0xFF) / 255.0f, a};
}

// Chrome palette (§5) — the study's own UI, not the simulation.
constexpr SkColor4f kInk = hex(0x0B0B0F);
constexpr SkColor4f kPanelInk = hex(0x090909);
constexpr SkColor4f kBone = hex(0xEDE9DE);
constexpr SkColor4f kSteel = hex(0x6E7B91);
constexpr SkColor4f kKeyline = hex(0x3A3A42);
constexpr SkColor4f kAmber = hex(0xFFB000);

// §6 — geometry.
constexpr int kFireW = 320;   // FIRE_WIDTH
constexpr int kFireH = 168;   // FIRE_HEIGHT
constexpr int kBlit = 3;      // exact integer nearest-neighbour scale
constexpr float kPanelW = kFireW * kBlit;  // 960
constexpr float kPanelH = kFireH * kBlit;  // 504
constexpr int kSwatch = 24;                // 37*24 + 36*2 = 960 exactly
constexpr int kInspectCells = 34, kInspectRows = 17, kInspectZoom = 8;
constexpr int kCropX = 143, kCropY = 100;  // the crop the inspector watches

// §7 — the one timing constant from the source: CJS_TICKER_FPS = 27.
constexpr double kSimHz = 27.0;
constexpr double kSimStep = 1.0 / kSimHz;

// ---------------------------------------------------------------------------
// Type

sk_sp<SkTypeface> face(const char *family, SkFontStyle style) {
  auto mgr = sigil::weave::ports::systemFontManager();
  sk_sp<SkTypeface> f = mgr->matchFamilyStyle(family, style);
  return f ? f : mgr->matchFamilyStyle(nullptr, style);
}

sk_sp<SkTypeface> monoFace() {
  static sk_sp<SkTypeface> f = face("Menlo", SkFontStyle::Normal());
  return f;
}
sk_sp<SkTypeface> heavyFace() {
  static sk_sp<SkTypeface> f = [] {
    auto mgr = sigil::weave::ports::systemFontManager();
    sk_sp<SkTypeface> t = mgr->matchFamilyStyle(
        "Helvetica Neue",
        SkFontStyle(SkFontStyle::kBlack_Weight, SkFontStyle::kNormal_Width,
                    SkFontStyle::kUpright_Slant));
    return t ? t : face("Helvetica", SkFontStyle::Bold());
  }();
  return f;
}
sk_sp<SkTypeface> uiFace() {
  static sk_sp<SkTypeface> f = face("Helvetica Neue", SkFontStyle::Normal());
  return f;
}

sigil::weave::TextStyle type(sk_sp<SkTypeface> tf, float size, SkColor4f color,
                             float track = 0.0f) {
  sigil::weave::TextStyle s;
  s.shaping.typeface = std::move(tf);
  s.shaping.fontSize = size;
  s.shaping.letterSpacing = track;
  s.paint.foreground.setColor(color.toSkColor());
  s.paint.foreground.setAntiAlias(true);
  return s;
}

sigil::weave::TextStyle mono(float size, SkColor4f c, float track = 0.0f) {
  return type(monoFace(), size, c, track);
}
sigil::weave::TextStyle ui(float size, SkColor4f c, float track = 0.0f) {
  return type(uiFace(), size, c, track);
}

/** back.out(1.70158) — glyphfx::pop's constant. choreograph ships no
 *  easeOutBack, so the study carries its own (see LIBRARY GAPS). */
float easeOutBack(float t) {
  constexpr float s = 1.70158f;
  const float u = t - 1.0f;
  return 1.0f + (s + 1.0f) * u * u * u + s * u * u;
}

/** A tiny dark chip behind an annotation, so placard type stays legible
 *  over white-hot cells. */
Element chip(Element content, float alpha = 0.72f) {
  Element c = box().row().padding(7, 3).fill(SkColor4f{0, 0, 0, alpha});
  if (alpha > 0.0f)
    c.stroke(stroke(1.0f, Fill::color(hex(0x3A3A42, 0.8f)),
                    PathFormat::Align::Inner));
  return c.child(std::move(content));
}

} // namespace

// ---------------------------------------------------------------------------

struct PsxDoomFire : sigil::compose::sketch::Sketch {
  // --- the automaton's state: one buffer, mutated in place (§4) ---
  std::vector<uint8_t> heat;
  std::array<uint32_t, 37> lut{};   // heat → premultiplied RGBA8888 word
  SkBitmap bitmap;                  // 320×168, rewritten once per sim tick
  sk_sp<SkImage> frame;             // what custom() blits every render frame
  std::array<float, kFireH> rowMean{}; // mean heat per row — the decay curve
  uint32_t rng = 0x9E3779B9u;       // xorshift32 state, reseeded in setup()

  // --- clocks ---
  double accumulator = 0.0;         // the fixed-timestep bank
  uint64_t simSteps = 0;            // sim ticks since setup
  uint64_t drawFrames = 0;
  double elapsed = 0.0;
  double drawHz = 60.0;

  // --- bound outputs ---
  ch::Output<float> pulse{0.0f};    // swatch-36 energy strobe (τ ≈ 20 ms)
  ch::Output<float> blink{1.0f};    // console cursor square wave

  // --- console ---
  console::LineRing ring{64};
  size_t bootLine = 0;
  double nextBoot = 0.20;           // §7: boot starts at 200 ms
  bool bootDone = false;

  // =========================================================================
  // §4 — the algorithm, verbatim.

  float rand01() { // xorshift32
    rng ^= rng << 13;
    rng ^= rng >> 17;
    rng ^= rng << 5;
    return (float)(rng >> 8) * (1.0f / 16777216.0f);
  }

  void seed() {
    heat.assign((size_t)kFireW * kFireH, 0);
    for (int x = 0; x < kFireW; ++x)
      heat[(size_t)(kFireH - 1) * kFireW + x] = 36; // bottom row = max, once
  }

  void spreadFire(int src) {
    const uint8_t pixel = heat[(size_t)src];
    if (pixel == 0) {
      heat[(size_t)(src - kFireW)] = 0; // no lateral drift when already cold
      return;
    }
    // round(random()*3) — NOT uniform: P(0)=P(3)=1/6, P(1)=P(2)=1/3.
    // Mean lateral offset (1 - E[r]) = -0.5 columns: the flame's leftward
    // lean is a property of THIS distribution, not a wind variable.
    const int r = (int)std::lround(rand01() * 3.0f);
    const int dst = src - r + 1;
    const int target = dst - kFireW;
    // The JS reference lets dst wrap across the row edge (and writes past
    // index 0 harmlessly); C++ needs the bound, the wrap stays.
    if (target >= 0 && target < kFireW * kFireH)
      heat[(size_t)target] = (uint8_t)(pixel - (r & 1));
  }

  void doFire() {
    // x OUTER, y ASCENDING 1→167, single shared buffer. Ordering is
    // load-bearing: each step writes row y-1 while later steps at the same x
    // read row y, so ascending order guarantees a row is fully consumed as a
    // source before it is overwritten. Descending (or double buffering)
    // changes the flame's shape.
    for (int x = 0; x < kFireW; ++x)
      for (int y = 1; y < kFireH; ++y)
        spreadFire(y * kFireW + x);
  }

  /** heat[] → SkBitmap through the hard 37-entry LUT. No interpolation:
   *  the banding is the technique. Index 0 writes alpha 0 (the PSX
   *  "black pixels need to be transparent to show DOOM logo" key). */
  void rasterize() {
    uint32_t *px = (uint32_t *)bitmap.getPixels();
    if (!px)
      return;
    for (int y = 0; y < kFireH; ++y) {
      uint32_t sum = 0;
      const size_t base = (size_t)y * kFireW;
      for (int x = 0; x < kFireW; ++x) {
        const uint8_t v = heat[base + (size_t)x];
        px[base + (size_t)x] = lut[v];
        sum += v;
      }
      rowMean[(size_t)y] = (float)sum / (float)kFireW;
    }
    frame = bitmap.asImage(); // mutable bitmap → copy; safe to keep drawing
  }

  // =========================================================================
  // Paint programs (the escape hatch: content, not just paint, is volatile)

  PaintProgram fireProgram() {
    return [this](SkCanvas &canvas, const PaintContext &ctx) {
      if (!frame)
        return;
      // The whole per-frame cost: ONE nearest-neighbour blit. No sim work
      // happens here — that is what decouples 27 Hz from the render rate.
      canvas.drawImageRect(
          frame, SkRect::MakeIWH(kFireW, kFireH),
          SkRect::MakeWH(ctx.size.width(), ctx.size.height()),
          SkSamplingOptions(SkFilterMode::kNearest), nullptr,
          SkCanvas::kStrict_SrcRectConstraint);
    };
  }

  /** The decay curve: mean heat per buffer row, seed row (y=167) at the left
   *  falling to the cold ceiling (y=0) at the right — one column per row,
   *  tinted by the LUT entry that mean lands on, so the chart and the flame
   *  speak the same 37 colors. Accumulated in rasterize() on the SIM clock;
   *  this program only draws rects. */
  PaintProgram profileProgram() {
    return [this](SkCanvas &canvas, const PaintContext &ctx) {
      const float w = ctx.size.width(), h = ctx.size.height();
      const float step = w / (float)kFireH;
      SkPaint bar;
      bar.setAntiAlias(false);
      for (int i = 0; i < kFireH; ++i) {
        const float m = rowMean[(size_t)(kFireH - 1 - i)]; // bottom → top
        const int idx = std::clamp((int)std::lround(m), 0, 36);
        const float bh = std::max(1.0f, (m / 36.0f) * h);
        bar.setColor4f(idx == 0 ? hex(0x24242A) : hex(kPalette[idx]), nullptr);
        canvas.drawRect(
            SkRect::MakeXYWH((float)i * step, h - bh, step + 0.6f, bh), bar);
      }
      // the mean-heat ceiling the flame never crosses
      SkPaint rule;
      rule.setColor4f(hex(0x3A3A42, 0.9f), nullptr);
      rule.setStrokeWidth(1);
      canvas.drawLine(0, h - 0.5f, w, h - 0.5f, rule);
    };
  }

  /** The buffer inspector: the SAME live buffer and the SAME LUT, a
   *  different crop and a bigger integer zoom, with the cell lattice drawn
   *  on top so it reads as a byte array rather than a second flame. */
  PaintProgram inspectorProgram() {
    return [this](SkCanvas &canvas, const PaintContext &ctx) {
      if (!frame)
        return;
      const SkRect src = SkRect::MakeXYWH((float)kCropX, (float)kCropY,
                                          (float)kInspectCells,
                                          (float)kInspectRows);
      const SkRect dst = SkRect::MakeWH(ctx.size.width(), ctx.size.height());
      canvas.drawImageRect(frame, src, dst,
                           SkSamplingOptions(SkFilterMode::kNearest), nullptr,
                           SkCanvas::kStrict_SrcRectConstraint);
      SkPaint grid;
      grid.setAntiAlias(false);
      grid.setStrokeWidth(1);
      grid.setColor4f({kKeyline.fR, kKeyline.fG, kKeyline.fB, 0.55f}, nullptr);
      for (int i = 0; i <= kInspectCells; ++i) {
        const float x = (float)(i * kInspectZoom) + 0.5f;
        canvas.drawLine(x, 0, x, dst.height(), grid);
      }
      for (int j = 0; j <= kInspectRows; ++j) {
        const float y = (float)(j * kInspectZoom) + 0.5f;
        canvas.drawLine(0, y, dst.width(), y, grid);
      }
    };
  }

  // =========================================================================
  // Description

  Element eyebrow() {
    return text(toU8("STUDY 01 \xc2\xb7 CELLULAR AUTOMATON"),
                ui(12, kSteel, 2.6f))
        .opacity(withFrom(0.0f, 1.0f, {.duration = 260ms}))
        .translateY(withFrom(8.0f, 0.0f, {.duration = 260ms}));
  }

  Element title() {
    GlyphFx fx;
    fx.effect = glyphfx::rise(24);
    fx.stagger = {.eachMs = 28, .durationMs = 480}; // Kinetic.h cadence
    // Master progress spans durationMs + eachMs·(N-1) of virtual time.
    fx.progress = withFrom(0.0f, 1.0f,
                           {.duration = 872ms,
                            .ease = &ch::easeNone,
                            .delay = 120ms});
    return text(toU8("DOOM FIRE, 1995"), type(heavyFace(), 50, kBone, -0.6f))
        .key("title")
        .glyphFx(std::move(fx));
  }

  /** The logo voice: heavy, huge, wide-tracked, with a dark ring underlay so
   *  the letterforms hold their edge where a flame tongue crosses them. */
  sigil::weave::TextStyle doomType() {
    sigil::weave::TextStyle s =
        type(heavyFace(), 186, hex(0xC23A1C), 34.0f);
    s.paint.addUnderlay(sigil::weave::PaintLayer::outline(
        hex(0x2A0805).toSkColor(), 7.0f, SkPaint::kRound_Join));
    return s;
  }

  Element citation() {
    return text(toU8("id Software / Williams \xe2\x80\x94 PlayStation port "
                     "title screen \xc2\xb7 algorithm reverse-engineered "
                     "from the Doom 64 disassembly by Samuel Villarreal, "
                     "documented by Fabien Sanglard"),
                ui(11.5f, kSteel, 0.2f))
        .opacity(withFrom(0.0f, 1.0f,
                          {.duration = 320ms, .delay = 200ms}));
  }

  Element header() {
    return box()
        .column()
        .gap(5)
        .child(eyebrow())
        .child(title())
        .child(citation())
        .child(box()
                   .height(1)
                   .margin(0, 12, 0, 0)
                   .fill(kKeyline)
                   .opacity(withFrom(0.0f, 1.0f,
                                     {.duration = 400ms, .delay = 320ms})));
  }

  /** A corner tick pair — the placard's registration marks. */
  Element tick(float dx, float dy, bool left, bool top) {
    return box()
        .absolute()
        .left(left ? Dim(dx) : autoDim())
        .right(left ? autoDim() : Dim(dx))
        .top(top ? Dim(dy) : autoDim())
        .bottom(top ? autoDim() : Dim(dy))
        .width(10)
        .height(10)
        .stroke(stroke(1.0f, Fill::color(kAmber), PathFormat::Align::Inner))
        .opacity(0.55f)
        .zIndex(6);
  }

  Element firePanel() {
    return stack()
        .width(kPanelW)
        .height(kPanelH)
        .shrink(0)
        .clip()
        // ink under the flame — the fire's own alpha-0 cells reveal it and
        // whatever else sits below the custom() leaf
        .child(box().absolute().inset(0).fill(kPanelInk))
        // §8, the reveal trick: the word is never drawn "on top". It sits
        // BEHIND the flame, and the rasterizer's alpha-0 for heat 0 lets the
        // cold core show it through, breathing as the simulation runs. Its
        // cap band deliberately straddles the flame's ragged front (≈ row 60
        // of 168), so the tongues eat into it from below.
        .child(text(toU8("DOOM"), doomType())
                   .absolute()
                   .left(0)
                   .top(66)
                   .width(kPanelW)
                   .textAlign(sigil::weave::TextAlignment::kCenter)
                   .opacity(withFrom(0.0f, 1.0f,
                                     {.duration = 600ms, .delay = 380ms}))
                   .zIndex(1))
        // the automaton
        .child(custom(fireProgram())
                   .absolute()
                   .inset(0)
                   .zIndex(2)
                   .opacity(withFrom(0.0f, 1.0f,
                                     {.duration = 400ms, .delay = 480ms}))
                   .cache(Cache::None))
        // seed-row annotation: the fire's only permanent energy source
        .child(box()
                   .absolute()
                   .left(0)
                   .bottom(0)
                   .width(kPanelW)
                   .height(kBlit)
                   .fill(hex(0xFFFFFF, 0.9f))
                   .zIndex(3))
        .child(chip(text(toU8("SEED ROW  y = 167  \xc2\xb7  HEAT 36  \xc2\xb7  "
                             "WRITTEN ONCE, NEVER RE-RANDOMISED"),
                         mono(10, hex(0xEFEFC7), 1.0f)))
                   .absolute()
                   .left(10)
                   .bottom(12)
                   .opacity(withFrom(0.0f, 1.0f,
                                     {.duration = 300ms, .delay = 1000ms}))
                   .zIndex(4))
        // where the sidebar's inspector is looking
        .child(box()
                   .absolute()
                   .left((float)(kCropX * kBlit))
                   .top((float)(kCropY * kBlit))
                   .width((float)(kInspectCells * kBlit))
                   .height((float)(kInspectRows * kBlit))
                   .stroke(stroke(1.0f, Fill::color(kAmber),
                                  PathFormat::Align::Outer))
                   .opacity(withFrom(0.0f, 0.85f,
                                     {.duration = 300ms, .delay = 900ms}))
                   .zIndex(5))
        .child(chip(text(toU8("INSPECT \xe2\x86\x92"), mono(9, kAmber, 1.4f)),
                    0.8f)
                   .absolute()
                   .left((float)(kCropX * kBlit))
                   .top((float)(kCropY * kBlit) - 17.0f)
                   .opacity(withFrom(0.0f, 1.0f,
                                     {.duration = 300ms, .delay = 900ms}))
                   .zIndex(5))
        // the bezel, trim()-revealed on mount
        .child(box()
                   .absolute()
                   .inset(0)
                   .stroke(stroke(1.5f, Fill::color(kKeyline),
                                  PathFormat::Align::Inner))
                   .trim(0.0f,
                         withFrom(0.0f, 1.0f,
                                  {.duration = 500ms,
                                   .ease = &ch::easeOutCubic,
                                   .delay = 280ms}))
                   .zIndex(7))
        .child(tick(6, 6, true, true))
        .child(tick(6, 6, false, true))
        .child(tick(6, 6, true, false))
        .child(tick(6, 6, false, false))
        // the live readout rides a slot: renderSlot() on the SIM clock
        // touches this mount point only — the rest of the tree keeps its
        // caches, and describe() is never called at 27 Hz.
        .child(box()
                   .absolute()
                   .right(14)
                   .top(26)
                   .zIndex(6)
                   .child(slot("readout")))
        // the panel's own placard line
        .child(chip(text(toU8("BUFFER 320 \xc3\x97 168 CELLS  \xc2\xb7  "
                              "BLIT \xc3\x97" "3 NEAREST  \xc2\xb7  "
                              "PANEL 960 \xc3\x97 504 PX"),
                         mono(10, kSteel, 1.0f)),
                    0.0f)
                   .absolute()
                   .left(22)
                   .top(24)
                   .opacity(withFrom(0.0f, 1.0f,
                                     {.duration = 300ms, .delay = 820ms}))
                   .zIndex(6));
  }

  Element paletteStrip() {
    std::vector<Element> swatches;
    swatches.reserve(37);
    for (int i = 0; i < 37; ++i) {
      Element sw = box()
                       .width(kSwatch)
                       .height(34)
                       .shrink(0)
                       .fill(Material::solid(hex(kPalette[i])))
                       .transformOrigin(0.5f, 1.0f)
                       .scale(withFrom(0.0f, 1.0f,
                                       {.duration = 220ms,
                                        .ease = &easeOutBack}));
      if (i == 0) // the transparent one — show the key, not the color
        sw.fill(Material::solid(hex(0x070707)))
            .stroke(stroke(1.0f, Fill::color(kKeyline),
                           PathFormat::Align::Inner));
      if (i == 36) // §7: the energy strobe — one pulse per simulation tick
        sw.opacity(&pulse);
      swatches.push_back(std::move(sw));
    }
    return box()
        .column()
        .gap(6)
        .child(box()
                   .row()
                   .gap(2)
                   .staggerChildren(12ms)
                   .children(std::move(swatches)))
        .child(box()
                   .row()
                   .child(text(toU8("PALETTE \xc2\xb7 37 ENTRIES, HARD LUT "
                                    "(NO INTERPOLATION)"),
                               mono(10, kSteel, 1.2f)))
                   .child(box().grow(1))
                   .child(text(toU8("IDX 0 = ALPHA 0 (COLD CORE)"),
                               mono(10, kSteel, 1.2f)))
                   .child(box().width(18))
                   .child(text(toU8("IDX 36 = SEED"),
                               mono(10, hex(0xEFEFC7), 1.2f))));
  }

  console::Style consoleStyle() {
    console::Style s;
    s.text = mono(12, kAmber);
    s.palette = {mono(12, hex(0x8A6A22)),  // 0: dim continuation
                 mono(12, kBone)};         // 1: emphasis
    s.gap = 3;
    s.visibleLines = 16;
    s.cursorColor = kAmber;
    s.cursorWidth = 8;
    s.cursorHeight = 14;
    s.cursorOpacity = &blink;
    return s;
  }

  Element specPanel() {
    return box()
        .column()
        .grow(1)
        .padding(16, 14)
        .gap(10)
        .fill(kPanelInk)
        .stroke(stroke(1.0f, Fill::color(kKeyline), PathFormat::Align::Inner))
        .child(box()
                   .row()
                   .alignItems(Align::Center)
                   .child(text(toU8("SPEC SHEET"), mono(11, kBone, 1.6f)))
                   .child(box().grow(1))
                   .child(box()
                              .width(6)
                              .height(6)
                              .fill(kAmber)
                              .opacity(&pulse)
                              .margin(0, 0, 6, 0))
                   .child(text(toU8("27 Hz"), mono(11, kSteel, 1.2f))))
        .child(box().height(1).fill(kKeyline))
        .child(console::console(ring, consoleStyle()))
        .child(box().grow(1))
        .child(box().height(1).fill(kKeyline))
        // The decay curve: what the buffer looks like as DATA.
        .child(box()
                   .column()
                   .gap(5)
                   .child(box()
                              .row()
                              .child(text(toU8("MEAN HEAT / ROW"),
                                          mono(11, kBone, 1.6f)))
                              .child(box().grow(1))
                              .child(text(toU8("0 \xe2\x80\x93 36"),
                                          mono(10, kSteel, 1.0f))))
                   .child(custom(profileProgram())
                              .height(52)
                              .cache(Cache::None)
                              .opacity(withFrom(0.0f, 1.0f,
                                                {.duration = 400ms,
                                                 .delay = 1150ms})))
                   .child(box()
                              .row()
                              .child(text(toU8("y=167 SEED"), mono(9, kSteel)))
                              .child(box().grow(1))
                              .child(text(toU8("y=0 TOP"), mono(9, kSteel)))))
        .child(box().height(1).fill(kKeyline))
        .child(slot("stats"));
  }

  Element statRow(const char *label, std::string value, SkColor4f color) {
    return box()
        .row()
        .child(text(toU8(label), mono(11, kSteel, 0.8f)))
        .child(box().grow(1))
        .child(text(toU8(value), mono(11, color, 0.8f)));
  }

  Element inspectorPanel() {
    char caption[96];
    std::snprintf(caption, sizeof caption,
                  "RAW BUFFER \xe2\x80\x94 %d\xc3\x97%d CELLS, %d\xc3\x97 NO "
                  "FILTER",
                  kInspectCells, kInspectRows, kInspectZoom);
    return box()
        .column()
        .gap(7)
        .child(box()
                   .row()
                   .child(text(toU8("BUFFER INSPECTOR"), mono(11, kBone, 1.6f)))
                   .child(box().grow(1))
                   .child(text(toU8(std::string("@ ") +
                                    std::to_string(kCropX) + "," +
                                    std::to_string(kCropY)),
                               mono(11, kSteel, 0.8f))))
        .child(box()
                   .width((float)(kInspectCells * kInspectZoom))
                   .height((float)(kInspectRows * kInspectZoom))
                   .shrink(0)
                   .fill(kPanelInk)
                   .stroke(stroke(1.0f, Fill::color(kKeyline),
                                  PathFormat::Align::Outer))
                   .opacity(withFrom(0.0f, 1.0f,
                                     {.duration = 300ms, .delay = 700ms}))
                   .scale(withFrom(0.94f, 1.0f,
                                   {.duration = 300ms,
                                    .ease = &easeOutBack,
                                    .delay = 700ms}))
                   .child(custom(inspectorProgram())
                              .absolute()
                              .inset(0)
                              .cache(Cache::None)))
        .child(text(toU8(caption), mono(9.5f, kSteel, 1.0f)));
  }

  Element describe() {
    return box()
        .column()
        .padding(32, 26)
        .gap(16)
        .fill(kInk)
        .child(header())
        .child(box()
                   .row()
                   .gap(26)
                   .grow(1)
                   .alignItems(Align::Stretch)
                   .child(box()
                              .column()
                              .width(kPanelW)
                              .shrink(0)
                              .gap(10)
                              .child(firePanel())
                              .child(paletteStrip()))
                   .child(box()
                              .column()
                              .grow(1)
                              .gap(14)
                              .child(specPanel())
                              .child(inspectorPanel())));
  }

  // The two slots, re-rendered on the SIMULATION's clock (27 Hz) — never
  // the whole tree.
  Element readout() {
    char buf[64];
    std::snprintf(buf, sizeof buf, "STEP %06llu",
                  (unsigned long long)simSteps);
    return box()
        .row()
        .gap(8)
        .alignItems(Align::Center)
        .child(text(toU8(buf), mono(11, hex(0xEFEFC7), 1.2f)))
        .child(box().width(6).height(6).fill(kAmber).opacity(&pulse));
  }

  Element stats() {
    char rate[32], draw[32], ratio[32];
    std::snprintf(rate, sizeof rate, "%.2f Hz",
                  elapsed > 0.5 ? (double)simSteps / elapsed : kSimHz);
    std::snprintf(draw, sizeof draw, "%.1f Hz", drawHz);
    std::snprintf(ratio, sizeof ratio, "%.2f\xc3\x97", drawHz / kSimHz);
    return box()
        .column()
        .gap(4)
        .child(statRow("SIM STEP", std::to_string(simSteps), kBone))
        .child(statRow("SIM RATE", rate, kAmber))
        .child(statRow("DRAW RATE", draw, kBone))
        .child(statRow("DRAW / SIM", ratio, kSteel))
        .child(statRow("CELLS / STEP",
                       std::to_string(kFireW * (kFireH - 1)), kSteel));
  }

  // =========================================================================

  void setup(sketch::SketchContext &ctx) override {
    ctx.canvas(1360, 760);
    ctx.background(kInk);

    rng = 0x9E3779B9u;
    accumulator = 0.0;
    simSteps = 0;
    drawFrames = 0;
    elapsed = 0.0;
    drawHz = 60.0;
    bootLine = 0;
    nextBoot = 0.20;
    bootDone = false;
    ring.clear();

    // The LUT, built once: premultiplied RGBA8888 words. Entry 0 is fully
    // transparent — the alpha key the PSX code used to show the logo.
    for (int i = 0; i < 37; ++i) {
      const uint32_t rgb = kPalette[i];
      const uint32_t r = (rgb >> 16) & 0xFF, g = (rgb >> 8) & 0xFF,
                     b = rgb & 0xFF;
      lut[(size_t)i] = i == 0 ? 0u
                              : (r | (g << 8) | (b << 16) | (0xFFu << 24));
    }

    bitmap.allocPixels(SkImageInfo::Make(kFireW, kFireH, kRGBA_8888_SkColorType,
                                         kPremul_SkAlphaType));
    seed();
    rasterize();

    // ---- the fixed-timestep accumulator (§7/§10) -------------------------
    // Ticker hands us a continuous dt; there is no "fixed update" helper, so
    // the accumulator is bespoke sketch code. Everything the automaton does
    // — step, rasterize, strobe — happens on THIS clock, at 27 Hz, whatever
    // the render rate is.
    // NOTE: SketchContext is a per-call VALUE the host rebuilds every frame —
    // capturing it by reference would dangle. Capture the Composer, which is
    // host-owned and stable across the sketch's life.
    Composer &composer = ctx.composer;
    ctx.ticker.add([this, &composer](double dt) {
      elapsed += dt;
      ++drawFrames;
      if (dt > 0.0)
        drawHz = drawHz * 0.9 + (1.0 / dt) * 0.1;

      accumulator += dt;
      bool stepped = false;
      int guard = 0;
      while (accumulator >= kSimStep && guard++ < 6) {
        accumulator -= kSimStep;
        doFire();
        ++simSteps;
        stepped = true;
      }
      if (stepped) {
        rasterize();
        // Slots only: the readout and the stat block follow the SIM clock;
        // describe() is not called, and no other cache is touched.
        composer.renderSlot("readout", readout());
        composer.renderSlot("stats", stats());
      }

      // §7 energy strobe: exp decay (τ ≈ 20 ms), reset to 1 on every tick.
      float p = pulse.value() * (float)std::exp(-dt / 0.02);
      if (stepped)
        p = 1.0f;
      pulse = 0.22f + 0.78f * std::clamp(p, 0.0f, 1.0f);

      // Console boot: append on the DATA path (one mount per line, O(1)
      // reconciliation — the seq-id keys keep every retained line's cache).
      static const char *kBoot[] = {
          "> FIRE_WIDTH   = 320",
          "> FIRE_HEIGHT  = 168",
          "> TICK         = 27 Hz (fixed)",
          "> PALETTE      = 37 entries",
          "> spreadFire(src):",
          ">   r = round(rand()*3)  // 0..3",
          ">   dst = src - r + 1",
          ">   heat[dst-W] = heat[src] - (r&1)",
          "> seed: row H-1 = 36, once",
      };
      constexpr size_t kBootCount = sizeof(kBoot) / sizeof(kBoot[0]);
      if (bootLine < kBootCount && elapsed >= nextBoot) {
        const size_t indented = kBoot[bootLine][2] == ' ' ? 0 : SIZE_MAX;
        ring.append(toU8(kBoot[bootLine]), indented);
        ++bootLine;
        nextBoot += 0.11; // §7: staggerChildren(110ms) as data
        composer.render(describe());
        if (bootLine == kBootCount)
          bootDone = true;
      }
      // Cursor: square wave, 500 on / 500 off, once the boot lines land.
      blink = !bootDone ? 1.0f
                        : (std::fmod(elapsed - nextBoot, 1.0) < 0.5 ? 1.0f
                                                                    : 0.0f);
      return true;
    });

    ctx.composer.render(describe());
    ctx.composer.renderSlot("readout", readout());
    ctx.composer.renderSlot("stats", stats());
  }

  void update(double, sketch::SketchContext &) override {}
};

SIGIL_SKETCH(PsxDoomFire)
