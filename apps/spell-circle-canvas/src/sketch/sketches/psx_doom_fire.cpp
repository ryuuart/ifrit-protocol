// psx_doom_fire.cpp — the DOOM PSX fire, 1995, drawn by a pen.
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
// WHY A PEN DRAWS IT
//   The fire is a STATEFUL cellular automaton: every cell's next value
//   reads its neighbours' CURRENT values out of a persistent buffer, and
//   the buffer is stepped at the historical 27 Hz whatever rate the host
//   draws at. An immediate-mode loop is the shape of that — the buffer is
//   the sketch's own member, `ctx.ticker.addFixed(27, …)` owns the sim
//   clock, and every frame is one nearest-neighbour blit of the bitmap
//   the last tick rasterized, at `noSmooth()` so a cell is three canvas
//   pixels of one colour rather than a bilinear smear.
//
//   The interpolant `addFixed` publishes is the strobe: the energy pip
//   beside the step counter is `decay(alpha · step, 20 ms)`, which is 1
//   the instant the automaton advances and the same shape at every draw
//   rate. The one thing kept between frames as a TREE is the header —
//   its title cascades in glyph by glyph — and that rides in as a guest
//   through `pen.element`.
//
//   ./build/bin/Release/Sketchbook.app/Contents/MacOS/Sketchbook \
//       src/sketch/sketches/psx_doom_fire.cpp \
//       --frame /tmp/psx_doom_fire.png

#include <include/core/SkBitmap.h>
#include <include/core/SkImage.h>
#include <sigilcompose/core/Core.h>
#include <sigilcompose/draw/Draw.h>
#include <sigilcompose/kit/Kinetic.h>
#include <sigilcompose/typography/Typography.h>
#include <sigilcore/compute/Noise.h>
#include <sigilmotion/Animation.h>
#include <sigilmotion/schedule/Cascade.h>
#include <sigilsketch/draw/Draw.h>
#include <sigilweave/ports/SystemFontManager.h>
#include <sigilweave/style/Type.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <iterator>
#include <string>
#include <vector>
#include <sigilweave/kit/PaintLayers.h>

namespace sketch = sigil::sketch;
namespace compose = sigil::compose;
namespace motion = sigil::motion;
namespace weave = sigil::weave;

using namespace sigil::draw;
using namespace std::chrono_literals;
using compose::hex;
namespace ch = choreograph;

namespace {

// ---------------------------------------------------------------------------
// The palette: flames.html's `rgbs` array, index = heat value [0,36].

constexpr uint32_t kPalette[37] = {
    0x070707, 0x1F0707, 0x2F0F07, 0x470F07, 0x571707, 0x671F07, 0x771F07,
    0x8F2707, 0x9F2F07, 0xAF3F07, 0xBF4707, 0xC74707, 0xDF4F07, 0xDF5707,
    0xDF5707, 0xD75F07, 0xD75F07, 0xD7670F, 0xCF6F0F, 0xCF770F, 0xCF7F0F,
    0xCF8717, 0xC78717, 0xC78F17, 0xC7971F, 0xBF9F1F, 0xBF9F1F, 0xBFA727,
    0xBFA727, 0xBFAF2F, 0xB7AF2F, 0xB7B72F, 0xB7B737, 0xCFCF6F, 0xDFDF9F,
    0xEFEFC7, 0xFFFFFF};

// Chrome palette — the study's own UI, not the simulation.
constexpr SkColor4f kInk = hex(0x0B0B0F);
constexpr SkColor4f kPanelInk = hex(0x090909);
constexpr SkColor4f kBone = hex(0xEDE9DE);
constexpr SkColor4f kSteel = hex(0x6E7B91);
constexpr SkColor4f kKeyline = hex(0x3A3A42);
constexpr SkColor4f kAmber = hex(0xFFB000);

// Geometry. The buffer dimensions are the source's; everything else is
// this study's layout, sized so the blit stays an integer scale.
constexpr int kFireW = 320;  // FIRE_WIDTH
constexpr int kFireH = 168;  // FIRE_HEIGHT
constexpr int kBlit = 3;     // exact integer nearest-neighbour scale
constexpr float kPanelW = kFireW * kBlit;  // 960
constexpr float kPanelH = kFireH * kBlit;  // 504
constexpr int kSwatch = 24;                // 37*24 + 36*2 = 960 exactly
constexpr int kInspectCells = 34, kInspectRows = 17, kInspectZoom = 8;
constexpr int kCropX = 143, kCropY = 100;  // the crop the inspector watches

// The one timing constant taken from the source: CJS_TICKER_FPS = 27.
constexpr double kSimHz = 27.0;
constexpr double kSimStep = 1.0 / kSimHz;

// The page. A pen lays its own page out, so every distance the tree used
// to negotiate is a number here.
constexpr float kCanvasW = 1360, kCanvasH = 760;
constexpr float kPadX = 32, kPadY = 26;
constexpr float kHeaderH = 116;
constexpr float kBodyY = kPadY + kHeaderH + 16;      // 158
constexpr float kStripY = kBodyY + kPanelH + 10;     // 672
constexpr float kSideX = kPadX + kPanelW + 26;       // 1018
constexpr float kSideW = kCanvasW - kPadX - kSideX;  // 310
constexpr float kInspectH = 176;
constexpr float kSpecH = kCanvasH - kPadY - kBodyY - 12 - kInspectH;
constexpr float kInspectY = kBodyY + kSpecH + 12;

// ---------------------------------------------------------------------------
// Type


sk_sp<SkTypeface> monoFace() {
  return weave::ports::face({"Menlo"}, SkFontStyle::Normal());
}
sk_sp<SkTypeface> heavyFace() {
  return weave::ports::face({"Helvetica Neue", "Helvetica"},
                            SkFontStyle::kBlack_Weight);
}
sk_sp<SkTypeface> uiFace() {
  return weave::ports::face({"Helvetica Neue"}, SkFontStyle::Normal());
}

/** The pen's two registers. A pen carries one type and one fill, so a
 *  register is set rather than described: face and size on the type,
 *  colour on the fill, as p5 colours text. */
void mono(Pen& pen, float size, SkColor4f c, float track = 0.0f) {
  pen.textFont(weave::Type{.face = monoFace(), .size = size, .track = track});
  pen.fill(c);
}
/** The same register as a TextStyle, for the lines that ride in as a
 *  retained tree. */
weave::TextStyle uiStyle(float size, SkColor4f c, float track = 0.0f) {
  return weave::textStyle(
      {.face = uiFace(), .size = size, .color = c, .track = track});
}

SkColor4f fade(SkColor4f c, float a) { return {c.fR, c.fG, c.fB, c.fA * a}; }

/** A PART'S ENTRANCE, as time arithmetic: 0 before @p delayMs, 1 after
 *  @p delayMs + @p durationMs, and the curve between. What a described
 *  tree spells as `animate(from(0).to(1), {duration, delay})` is this in
 *  a loop, because a loop has the clock in its hand. */
float cue(double ms, float delayMs, float durationMs,
          const ch::EaseFn& ease = nullptr) {
  const float u =
      std::clamp((float)((ms - (double)delayMs) / (double)durationMs), 0.0f,
                 1.0f);
  return ease ? ease(u) : u;
}

}  // namespace

// ---------------------------------------------------------------------------

struct PsxDoomFire final : sketch::DrawSketch {
  // --- the automaton's state: one buffer, mutated in place ---
  std::vector<uint8_t> heat;
  std::array<uint32_t, 37> lut{};  // heat → premultiplied RGBA8888 word
  SkBitmap bitmap;                 // 320×168, rewritten once per sim tick
  sk_sp<SkImage> frame;            // what the pen blits every render frame
  std::array<float, kFireH> rowMean{};  // mean heat per row — the decay curve
  uint32_t rng = 0x9E3779B9u;           // xorshift32 state, reseeded in setup()

  // --- clocks ---
  bool stepped = false;   // the automaton advanced since the last picture
  uint64_t simSteps = 0;  // sim ticks since setup

  /** THE FIXED STEP'S LEFTOVER FRACTION, published by `addFixed`. The
   *  automaton is discrete — there is nothing to interpolate between two
   *  heat buffers — so what this drives is the energy strobe: the age of
   *  the last tick, in steps, which is the one thing on this canvas that
   *  says how the sim clock and the draw clock stand to each other. */
  ch::Output<float> alpha{0.0f};

  // The palette strip's entrance ladder, resolved once: 37 swatches on a
  // 12 ms spread, read back per swatch instead of restated as i·12.
  motion::Cascade swatchCascade;
  static constexpr float kSwatchSpanMs = 220.0f + 12.0f * 36.0f;

  // =========================================================================
  // The algorithm, verbatim.

  float rand01() { return sigil::core::noise::xorshiftUnitNext(rng); }

  void seed() {
    heat.assign((size_t)kFireW * kFireH, 0);
    for (int x = 0; x < kFireW; ++x)
      heat[(size_t)(kFireH - 1) * kFireW + x] = 36;  // bottom row = max, once
  }

  void spreadFire(int src) {
    const uint8_t pixel = heat[(size_t)src];
    if (pixel == 0) {
      heat[(size_t)(src - kFireW)] = 0;  // no lateral drift when already cold
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
      heat[(size_t)target] = (uint8_t)(pixel - ((unsigned)r & 1u));
  }

  void doFire() {
    // x OUTER, y ASCENDING 1→167, single shared buffer. Ordering is
    // load-bearing: each step writes row y-1 while later steps at the same x
    // read row y, so ascending order guarantees a row is fully consumed as a
    // source before it is overwritten. Descending (or double buffering)
    // changes the flame's shape.
    for (int x = 0; x < kFireW; ++x)
      for (int y = 1; y < kFireH; ++y) spreadFire(y * kFireW + x);
  }

  /** heat[] → SkBitmap through the hard 37-entry LUT. No interpolation:
   *  the banding is the technique. Index 0 writes alpha 0 (the PSX
   *  "black pixels need to be transparent to show DOOM logo" key). */
  void rasterize() {
    uint32_t* px = (uint32_t*)bitmap.getPixels();
    if (!px) return;
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
    frame = bitmap.asImage();  // mutable bitmap → copy; safe to keep drawing
  }

  // =========================================================================
  // The one retained guest: the header, whose title cascades in glyph by
  // glyph. Kinetic type is a described tree's business — the shaping, the
  // per-cluster schedule and the caches all live in it — so it rides in
  // through pen.element and is reconciled every frame.

  compose::Element header() {
    static constexpr char kTitle[] = "DOOM FIRE, 1995";
    const motion::Spread cascade{.eachMs = 28, .durationMs = 480};
    // A space is a gap the flow leaves rather than a glyph, so for this
    // ASCII line the unit count is its non-space character count, and the
    // progress lasts exactly the cascade's own span: the last glyph lands
    // as the master arrives at 1.
    const auto units =
        (uint32_t)std::count_if(std::begin(kTitle), std::end(kTitle) - 1,
                                [](char c) { return c != ' '; });
    const auto span =
        std::chrono::milliseconds(std::lround(cascade.spanMs(units)));
    return compose::box()
        .column()
        .gap(5)
        .child(compose::text(compose::toU8("CELLULAR AUTOMATON"),
                             uiStyle(12, kSteel, 2.6f))
                   .opacity(motion::animate(motion::from(0.0f).to(1.0f),
                                             {.duration = 260ms}))
                   .translateY(motion::animate(motion::from(8.0f).to(0.0f),
                                                {.duration = 260ms})))
        .child(compose::text(compose::toU8(kTitle),
                             weave::textStyle({.face = heavyFace(),
                                               .size = 50,
                                               .color = kBone,
                                               .track = -0.6f}))
                   .key("title")
                   .fx({.effect = compose::fx::rise(24),
                        .stagger = cascade,
                        .progress = motion::animate(
                            motion::from(0.0f).to(1.0f),
                            {.duration = span,
                             .ease = &ch::easeNone,
                             .delay = 120ms})}))
        .child(compose::text(
                   compose::toU8(
                       "id Software / Williams \xe2\x80\x94 PlayStation port "
                       "title screen \xc2\xb7 algorithm reverse-engineered "
                       "from the Doom 64 disassembly by Samuel Villarreal, "
                       "documented by Fabien Sanglard "
                       "\xc2\xb7 fabiensanglard.net/doom_fire_psx "
                       "\xc2\xb7 DoomFirePSX/flames.html"),
                   uiStyle(11.5f, kSteel, 0.2f))
                   .opacity(motion::animate(
                       motion::from(0.0f).to(1.0f),
                       {.duration = 320ms, .delay = 200ms})));
  }

  /** The logo voice: heavy, huge, wide-tracked, with a dark ring underlay
   *  so the letterforms hold their edge where a flame tongue crosses
   *  them. It sits BEHIND the fire and is never drawn on top: the
   *  rasterizer's alpha-0 for heat 0 lets the cold core show it through,
   *  breathing as the simulation runs. */
  compose::Element doomWord() {
    weave::TextStyle s = weave::textStyle({.face = heavyFace(),
                                           .size = 186,
                                           .color = hex(0xC23A1C),
                                           .track = 34.0f});
    s.paint.addUnderlay(sigil::weave::kit::outline(
        hex(0x2A0805).toSkColor(), 7.0f, SkPaint::kRound_Join));
    return compose::text(compose::toU8("DOOM"), std::move(s))
        .width(kPanelW)
        .textAlign(weave::TextAlignment::kCenter)
        .opacity(motion::animate(motion::from(0.0f).to(1.0f),
                                  {.duration = 600ms, .delay = 380ms}));
  }

  // =========================================================================
  // The page, in pen verbs.

  /** A hairline. */
  void rule(Pen& pen, float x, float y, float w, float a = 1.0f) {
    pen.noStroke();
    pen.fill(fade(kKeyline, a));
    pen.rect(x, y, w, 1);
  }

  /** A dark chip behind an annotation, so placard type stays legible over
   *  white-hot cells. The pen measures the line it is about to set, which
   *  is what sizes the chip. */
  void chip(Pen& pen, const char* s, float x, float y, float size,
            SkColor4f color, float track, float ground, float a) {
    mono(pen, size, color, track);
    const float w = pen.textWidth(s) + 14.0f;
    const float h = size + 12.0f;
    if (ground > 0.0f) {
      pen.fill(SkColor4f{0, 0, 0, ground * a});
      pen.stroke(fade(hex(0x3A3A42, 0.8f), a));
      pen.strokeWeight(1);
      pen.rect(x, y, w, h);
      pen.noStroke();
    }
    mono(pen, size, fade(color, a), track);
    pen.textAlign(LEFT, TOP);
    pen.text(s, x + 7, y + 5);
  }

  /** A corner tick — the placard's registration mark. */
  void tick(Pen& pen, float x, float y, float a) {
    pen.fill(SkColor4f{0, 0, 0, 0.6f * a});
    pen.stroke(fade(kAmber, 0.8f * a));
    pen.strokeWeight(1);
    pen.rect(x, y, 11, 11);
    pen.noStroke();
  }

  void firePanel(Pen& pen, double ms) {
    const float x = kPadX, y = kBodyY;
    // ink under the flame — the fire's own alpha-0 cells reveal it
    pen.noStroke();
    pen.fill(kPanelInk);
    pen.rect(x, y, kPanelW, kPanelH);

    // the word, behind everything the automaton draws
    pen.element(doomWord(), SkRect::MakeXYWH(x, y + 80, kPanelW, 220));

    // THE WHOLE PER-FRAME COST OF THE SUBJECT: one nearest-neighbour blit
    // of the bitmap the last sim tick rasterized. No sim work happens on a
    // draw frame — that is what decouples 27 Hz from the render rate.
    if (frame) {
      const float a = cue(ms, 480, 400);
      pen.noSmooth();
      pen.push();
      pen.fill(SkColor4f{1, 1, 1, a});
      pen.image(frame, x, y, kPanelW, kPanelH);
      pen.pop();
      pen.smooth();
    }

    // the seed row: the fire's only permanent energy source
    pen.fill(hex(0xFFFFFF, 0.9f));
    pen.rect(x, y + kPanelH - kBlit, kPanelW, kBlit);
    chip(pen,
         "SEED ROW  y = 167  \xc2\xb7  HEAT 36  \xc2\xb7  "
         "WRITTEN ONCE, NEVER RE-RANDOMISED",
         x + 10, y + kPanelH - 12 - 22, 10, hex(0xEFEFC7), 1.0f, 0.72f,
         cue(ms, 1000, 300));

    // where the sidebar's inspector is looking
    const float ia = cue(ms, 900, 300);
    pen.noFill();
    pen.stroke(fade(kAmber, 0.85f * ia));
    pen.strokeWeight(1);
    pen.rect(x + (float)(kCropX * kBlit) - 0.5f,
             y + (float)(kCropY * kBlit) - 0.5f,
             (float)(kInspectCells * kBlit) + 1.0f,
             (float)(kInspectRows * kBlit) + 1.0f);
    pen.noStroke();
    chip(pen, "INSPECT \xe2\x86\x92", x + (float)(kCropX * kBlit),
         y + (float)(kCropY * kBlit) - 22.0f, 9, kAmber, 1.4f, 0.8f, ia);

    // the panel's own placard line, and the live step counter beside it
    chip(pen,
         "BUFFER 320 \xc3\x97 168 CELLS  \xc2\xb7  BLIT \xc3\x97"
         "3 NEAREST  \xc2\xb7  PANEL 960 \xc3\x97 504 PX",
         x + 22, y + 24, 10, kSteel, 1.0f, 0.0f, cue(ms, 820, 300));

    char step[64];
    std::snprintf(step, sizeof step, "STEP %06llu",
                  (unsigned long long)simSteps);
    mono(pen, 11, hex(0xEFEFC7), 1.2f);
    pen.textAlign(RIGHT, TOP);
    pen.text(step, x + kPanelW - 28, y + 26);
    pen.textAlign(LEFT, TOP);
    pen.fill(fade(kAmber, strobe()));
    pen.rect(x + kPanelW - 20, y + 30, 6, 6);

    // the bezel, revealed on mount, and its four registration marks
    const float ba = cue(ms, 280, 500, &ch::easeOutCubic);
    pen.noFill();
    pen.stroke(fade(kKeyline, ba));
    pen.strokeWeight(1.5f);
    pen.rect(x + 0.75f, y + 0.75f, kPanelW - 1.5f, kPanelH - 1.5f);
    pen.noStroke();
    tick(pen, x + 6, y + 6, ba);
    tick(pen, x + kPanelW - 17, y + 6, ba);
    tick(pen, x + 6, y + kPanelH - 17, ba);
    tick(pen, x + kPanelW - 17, y + kPanelH - 17, ba);
  }

  /** The energy strobe: 1 the instant the automaton advanced, decaying on
   *  a 20 ms time constant. The age comes from the fixed step's own
   *  leftover fraction, so the pip is the same shape at every draw rate
   *  and a still of it is a claim about the piece. */
  float strobe() const {
    return 0.22f +
           0.78f * motion::decay(alpha.value() * (float)kSimStep, 0.02f);
  }

  void paletteStrip(Pen& pen, double ms) {
    const float master = std::clamp((float)ms / kSwatchSpanMs, 0.0f, 1.0f);
    pen.noStroke();
    for (int i = 0; i < 37; ++i) {
      const float u = swatchCascade.localTime(master, (uint32_t)i, 0);
      const float s = ch::easeOutBack(u, 1.70158f);
      const float x = kPadX + (float)i * (float)(kSwatch + 2);
      const float bottom = kStripY + 34.0f;
      pen.push();
      pen.translate(x + kSwatch * 0.5f, bottom);  // origin (0.5, 1)
      pen.scale(s);
      pen.fill(hex(kPalette[i]));
      if (i == 36) pen.fill(hex(kPalette[36], strobe()));
      pen.rect(-kSwatch * 0.5f, -34, kSwatch, 34);
      if (i == 0) {  // the transparent one — show the key, not the colour
        pen.noFill();
        pen.stroke(kKeyline);
        pen.strokeWeight(1);
        pen.rect(-kSwatch * 0.5f + 0.5f, -33.5f, kSwatch - 1, 33);
        pen.noStroke();
      }
      pen.pop();
    }
    const float y = kStripY + 34 + 6;
    mono(pen, 10, kSteel, 1.2f);
    pen.textAlign(LEFT, TOP);
    pen.text("\xe2\x86\x91 IDX 0 \xc2\xb7 ALPHA 0 (THE COLD CORE)", kPadX, y);
    pen.textAlign(CENTER, TOP);
    pen.text("PALETTE \xe2\x80\x94 37 ENTRIES, HARD LUT, NO INTERPOLATION",
             kPadX + kPanelW * 0.5f, y);
    mono(pen, 10, hex(0xEFEFC7), 1.2f);
    pen.textAlign(RIGHT, TOP);
    pen.text("IDX 36 \xc2\xb7 SEED \xe2\x86\x91", kPadX + kPanelW, y);
    pen.textAlign(LEFT, TOP);
  }

  /** The boot console: nine lines of the algorithm, landing 110 ms apart,
   *  with a square-wave caret after them. */
  void bootFeed(Pen& pen, double seconds, float x, float y) {
    static const char* kBoot[] = {
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
    constexpr int kCount = (int)(sizeof(kBoot) / sizeof(kBoot[0]));
    constexpr double kFirst = 0.20, kEach = 0.11;
    const int shown = std::clamp(
        (int)std::floor((seconds - kFirst) / kEach) + 1, 0, kCount);
    pen.textAlign(LEFT, TOP);
    float cursor = y;
    for (int i = 0; i < shown; ++i) {
      // A continuation line is dimmer: the level is read off the line, as
      // it was when the console was a feed of styled records.
      const bool dim = kBoot[i][2] == ' ';
      mono(pen, 11.5f, dim ? hex(0x8A6A22) : kAmber);
      pen.text(kBoot[i], x, cursor);
      cursor += 11.5f + 4.0f;
    }
    const double done = kFirst + kEach * (kCount - 1);
    const bool blink = seconds < done || motion::phase(seconds - done, 1.0) < 0.5f;
    if (blink) {
      pen.noStroke();
      pen.fill(kAmber);
      pen.rect(x, cursor + 1, 8, 14);
    }
  }

  /** The decay curve: mean heat per buffer row, seed row (y=167) at the
   *  left falling to the cold ceiling (y=0) at the right — one column per
   *  row, tinted by the LUT entry that mean lands on, so the chart and
   *  the flame speak the same 37 colours. Accumulated in rasterize() on
   *  the SIM clock; this only draws rects. */
  void profile(Pen& pen, float x, float y, float w, float h, float a) {
    pen.noSmooth();  // a one-column bar is a pixel column, not a smear
    pen.noStroke();
    const float step = w / (float)kFireH;
    for (int i = 0; i < kFireH; ++i) {
      const float m = rowMean[(size_t)(kFireH - 1 - i)];  // bottom → top
      const int idx = std::clamp((int)std::lround(m), 0, 36);
      const float bh = std::max(1.0f, (m / 36.0f) * h);
      pen.fill(fade(idx == 0 ? hex(0x24242A) : hex(kPalette[idx]), a));
      pen.rect(x + (float)i * step, y + h - bh, step + 0.6f, bh);
    }
    pen.smooth();
    // the mean-heat ceiling the flame never crosses
    pen.stroke(fade(hex(0x3A3A42, 0.9f), a));
    pen.strokeWeight(1);
    pen.line(x, y + h - 0.5f, x + w, y + h - 0.5f);
    pen.noStroke();
  }

  void statRow(Pen& pen, float x, float w, float y, const char* label,
               const std::string& value, SkColor4f color) {
    mono(pen, 10.5f, kSteel, 0.8f);
    pen.textAlign(LEFT, TOP);
    pen.text(label, x, y);
    mono(pen, 10.5f, color, 0.8f);
    pen.textAlign(RIGHT, TOP);
    pen.text(value, x + w, y);
    pen.textAlign(LEFT, TOP);
  }

  void specPanel(sketch::DrawContext& ctx, double seconds, double drawHz) {
    Pen& pen = ctx.pen;
    const float x = kSideX, y = kBodyY, w = kSideW, h = kSpecH;
    pen.noStroke();
    pen.fill(kPanelInk);
    pen.rect(x, y, w, h);
    pen.noFill();
    pen.stroke(kKeyline);
    pen.strokeWeight(1);
    pen.rect(x + 0.5f, y + 0.5f, w - 1, h - 1);
    pen.noStroke();

    const float cx = x + 15, cw = w - 30;
    float cursor = y + 12;
    mono(pen, 11, kBone, 1.6f);
    pen.textAlign(LEFT, TOP);
    pen.text("SPEC SHEET", cx, cursor);
    mono(pen, 11, kSteel, 1.2f);
    const float rateW = pen.textWidth("27 Hz");
    pen.textAlign(RIGHT, TOP);
    pen.text("27 Hz", cx + cw, cursor);
    pen.textAlign(LEFT, TOP);
    pen.fill(fade(kAmber, strobe()));
    pen.rect(cx + cw - rateW - 12, cursor + 3, 6, 6);
    cursor += 19;
    rule(pen, cx, cursor, cw);
    cursor += 9;

    bootFeed(pen, seconds, cx, cursor);

    // the chart, the stats and the rules under them are anchored to the
    // panel's foot rather than to the feed, so a line landing does not
    // shift them
    float foot = y + h - 12;
    const float statH = 5 * (10.5f + 3.0f);
    foot -= statH;
    char rate[32], drawn[32], ratio[32];
    // Both rates are read off this run's own execution, so a capture
    // taken for a diff carries the rates the sheet declares instead.
    const double simRate = ctx.measured(
        seconds > 0.5 ? (double)simSteps / seconds : kSimHz, kSimHz);
    const double drawRate = ctx.measured(drawHz, 60.0);
    std::snprintf(rate, sizeof rate, "%.2f Hz", simRate);
    std::snprintf(drawn, sizeof drawn, "%.1f Hz", drawRate);
    std::snprintf(ratio, sizeof ratio, "%.2f\xc3\x97", drawRate / kSimHz);
    float sy = foot;
    statRow(pen, cx, cw, sy, "SIM STEP", std::to_string(simSteps), kBone);
    sy += 13.5f;
    statRow(pen, cx, cw, sy, "SIM RATE", rate, kAmber);
    sy += 13.5f;
    statRow(pen, cx, cw, sy, "DRAW RATE", drawn, kBone);
    sy += 13.5f;
    statRow(pen, cx, cw, sy, "DRAW / SIM", ratio, kSteel);
    sy += 13.5f;
    statRow(pen, cx, cw, sy, "CELLS / STEP",
            std::to_string(kFireW * (kFireH - 1)), kSteel);

    foot -= 9;
    rule(pen, cx, foot, cw);
    foot -= 44 + 6;
    profile(pen, cx, foot, cw, 44, cue(seconds * 1000.0, 1150, 400));
    foot -= 5 + 11;
    mono(pen, 11, kBone, 1.6f);
    pen.text("MEAN HEAT / ROW", cx, foot);
    mono(pen, 9.5f, kSteel, 1.0f);
    pen.textAlign(RIGHT, TOP);
    pen.text("y=167 \xe2\x86\x92 y=0", cx + cw, foot + 1.5f);
    pen.textAlign(LEFT, TOP);
    foot -= 9;
    rule(pen, cx, foot, cw);
  }

  /** The buffer inspector: the SAME live image and the SAME LUT, a
   *  different crop and a bigger integer zoom, with the cell lattice
   *  drawn on top so it reads as a byte array rather than a second
   *  flame. */
  void inspectorPanel(Pen& pen, double ms) {
    const float x = kSideX, y = kInspectY, w = kSideW;
    mono(pen, 11, kBone, 1.6f);
    pen.textAlign(LEFT, TOP);
    pen.text("BUFFER INSPECTOR", x, y);
    mono(pen, 11, kSteel, 0.8f);
    pen.textAlign(RIGHT, TOP);
    pen.text(std::string("@ ") + std::to_string(kCropX) + "," +
                 std::to_string(kCropY),
             x + w, y);
    pen.textAlign(LEFT, TOP);

    const float bx = x, by = y + 20;
    const float bw = (float)(kInspectCells * kInspectZoom);
    const float bh = (float)(kInspectRows * kInspectZoom);
    const float a = cue(ms, 700, 300);
    const float s = 0.94f + 0.06f * ch::easeOutBack(cue(ms, 700, 300), 1.70158f);
    pen.push();
    pen.translate(bx + bw * 0.5f, by + bh * 0.5f);
    pen.scale(s);
    pen.translate(-bw * 0.5f, -bh * 0.5f);
    pen.noStroke();
    pen.fill(fade(kPanelInk, a));
    pen.rect(0, 0, bw, bh);
    if (frame) {
      // The crop and the lattice over it are the BUFFER, and the buffer
      // is the box: a clip holds both inside it, so the grid ends where
      // the bytes end and the keyline below is drawn outside the mask.
      // One noSmooth covers the pair — a blown-up crop is blocks, and the
      // lattice is a pixel grid too — and the pop puts smoothing back.
      pen.push();
      pen.clip([&] { pen.rect(0, 0, bw, bh); });
      pen.noSmooth();
      pen.fill(SkColor4f{1, 1, 1, a});
      pen.image(frame, 0, 0, bw, bh, (float)kCropX, (float)kCropY,
                (float)kInspectCells, (float)kInspectRows);
      pen.stroke(fade({kKeyline.fR, kKeyline.fG, kKeyline.fB, 0.55f}, a));
      pen.strokeWeight(1);
      for (int i = 0; i <= kInspectCells; ++i)
        pen.line((float)(i * kInspectZoom) + 0.5f, 0,
                 (float)(i * kInspectZoom) + 0.5f, bh);
      for (int j = 0; j <= kInspectRows; ++j)
        pen.line(0, (float)(j * kInspectZoom) + 0.5f, bw,
                 (float)(j * kInspectZoom) + 0.5f);
      pen.pop();
    }
    pen.noFill();
    pen.stroke(fade(kKeyline, a));
    pen.strokeWeight(1);
    pen.rect(-0.5f, -0.5f, bw + 1, bh + 1);
    pen.noStroke();
    pen.pop();

    char caption[96];
    std::snprintf(caption, sizeof caption,
                  "RAW BUFFER \xe2\x80\x94 %d\xc3\x97%d CELLS, %d\xc3\x97 NO "
                  "FILTER",
                  kInspectCells, kInspectRows, kInspectZoom);
    mono(pen, 9.5f, kSteel, 1.0f);
    pen.text(caption, bx, by + bh + 7);
  }

  // =========================================================================

  void setup(sketch::DrawContext& ctx) override {
    ctx.canvas(kCanvasW, kCanvasH);
    ctx.background(kInk);
    ctx.captureAt(6.0);
    // The canvas a pen keeps IS the plate, so this is a floor on the
    // pixels it is formed with: at 2, one fire cell is six device pixels
    // and the one blit this study is about is never resampled.
    ctx.oversample(2);

    rng = 0x9E3779B9u;
    stepped = false;
    simSteps = 0;

    // The LUT, built once: premultiplied RGBA8888 words. Entry 0 is fully
    // transparent — the alpha key the PSX code used to show the logo.
    for (int i = 0; i < 37; ++i) {
      const uint32_t rgb = kPalette[i];
      const uint32_t r = (rgb >> 16u) & 0xFFu, g = (rgb >> 8u) & 0xFFu,
                     b = rgb & 0xFFu;
      lut[(size_t)i] =
          i == 0 ? 0u : (r | (g << 8u) | (b << 16u) | (0xFFu << 24u));
    }

    bitmap.allocPixels(SkImageInfo::Make(kFireW, kFireH, kRGBA_8888_SkColorType,
                                         kPremul_SkAlphaType));
    seed();
    rasterize();

    swatchCascade.build({.eachMs = 12, .durationMs = 220}, 37, 1);

    // ---- the fixed-timestep clock ----------------------------------------
    // Everything the automaton does happens on THIS clock, at 27 Hz,
    // whatever the render rate is — which is the whole subject. The step
    // count comes from accumulated time rather than from a float
    // accumulator compared against a step size: an accumulator drifts over
    // a long run, so the same simulated moment lands on either side of a
    // boundary depending on how fast the host drew, and a captured frame
    // is then a function of the machine.
    ctx.ticker.addFixed(
        kSimHz,
        [this] {
          doFire();
          ++simSteps;
          stepped = true;
          return true;
        },
        6, &alpha);

    ctx.pen.noStroke();
    ctx.pen.textAlign(LEFT, TOP);
  }

  void draw(sketch::DrawContext& ctx) override {
    Pen& pen = ctx.pen;
    // The picture follows the SIM clock: the automaton stepped or it did
    // not, and when it did the buffer is rasterized once for however many
    // draw frames come before the next step.
    if (stepped) {
      stepped = false;
      rasterize();
    }
    const double ms = pen.millis();
    pen.background(kInk);
    pen.element(header(), SkRect::MakeXYWH(kPadX, kPadY, kCanvasW - 2 * kPadX,
                                           kHeaderH));
    rule(pen, kPadX, kPadY + kHeaderH - 4, kCanvasW - 2 * kPadX - 12,
         cue(ms, 320, 400));
    firePanel(pen, ms);
    paletteStrip(pen, ms);
    specPanel(ctx, ms / 1000.0, pen.frameRate());
    inspectorPanel(pen, ms);
  }
};

SIGIL_SKETCH(
    PsxDoomFire, "Study \xc2\xb7 Game UI",
    "The DOOM PlayStation title flame (1995) \xe2\x80\x94 an automaton at a "
    "fixed 27 Hz")
