// nightingale_coxcomb.cpp — "Diagram of the Causes of Mortality in the
// Army in the East", Florence Nightingale, engraved 1858.
//
// REFERENCE
//   The two-wheel overlapping-sector polar-area plate ("the wedges";
//   later: coxcomb / rose diagram) from Notes on Matters Affecting the
//   Health … of the British Army (1858), printed as a lithograph in
//   Martineau & Nightingale, England and Her Soldiers (Smith, Elder &
//   Co., 1859). Printer's imprint: "Harrison & Sons, St. Martin's Lane."
//   Physical plate 19 x 35 cm.
//
//   Scan studied: David Rumsey Map Collection / Internet Archive item
//   dr_diagram-of-the-causes-of-mortality-in-the-army-in-the-east-10563002,
//   6996 x 3826 JPEG (507 dpi). Every geometric constant below was
//   re-derived from that scan with a pixel sampler, not eyeballed:
//   the blue-ink bounding boxes of both wheels give two independent
//   least-squares fits for the radius constant k and both centres, and
//   they agree to 5% — which is also the proof that the two wheels share
//   ONE scale (k_left = 61.0, k_right = 62.8 scan px per sqrt(rate)).
//
//   Data: Nightingale's own published figures via HistData::Nightingale
//   (R), rate = 12 * 1000 * deaths / army = "annual rate of mortality
//   per 1000". Radius law r = k*sqrt(rate) — the 1858 innovation: at a
//   fixed 30 deg wedge, only a square-root radius makes AREA
//   proportional to the number of dead.
//
// TWO THINGS THE PLATE DOES THAT THE BRIEF GOT WRONG, corrected here
// from the scan itself:
//   1. Month labels are NOT on a common label ring. Each one hugs its
//      OWN wedge's rim (APRIL 1854 sits ~160 px from the hub, JANUARY
//      1855 sits ~570 px out), with a floor so the tiny spring months
//      clear the black hub. That scalloped label ring is most of what
//      makes the plate read as engraved rather than plotted.
//   2. The lower-half labels are NOT flipped. The engraver used one
//      convention — glyph-up points radially OUTWARD, everywhere — so
//      DECEMBER, JANUARY, FEBRUARY and the left wheel's "1856" all come
//      out genuinely upside down. The brief treats "1856" as a unique
//      quirk; it is simply the rule, applied consistently.
//   Also: the two campaign annotations (BULGARIA, CRIMEA) are set
//   RADIALLY along their spoke, not tangentially like the months.
//
// Run:
//   ./build/bin/Release/ComposeSketch \
//       src/common/compose/sketch/sketches/nightingale_coxcomb.cpp \
//       --frame /tmp/nightingale_coxcomb.png --at 13.6

#include <sigilsketch/Sketch.h>

#include <sigilweave/FontContext.h>

#include <sigilcompose/Kinetic.h>
#include <sigilcompose/LayerStyles.h>
#include <sigilcompose/Material.h>
#include <sigilcompose/Patterns.h>
#include <sigilcompose/Shapes.h>

#include <include/core/SkFont.h>
#include <include/core/SkString.h>
#include <include/effects/SkRuntimeEffect.h>
#include <include/core/SkFontMgr.h>
#include <include/core/SkFontStyle.h>
#include <include/core/SkPathBuilder.h>
#include <include/core/SkTextBlob.h>
#include <include/core/SkTypeface.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <string>
#include <vector>

using namespace sigil::compose;
using namespace sigil::compose::util;
using namespace std::chrono_literals;
namespace ch = choreograph;

namespace {

// ---------------------------------------------------------------------------
// palette — sampled from the scan (patch means + the darkest/lightest
// deciles, which separate "ink dot" from "paper showing through")

constexpr SkColor4f hex(uint32_t v, float a = 1.0f) {
  return {((v >> 16) & 0xffu) / 255.0f, ((v >> 8) & 0xffu) / 255.0f,
          (v & 0xffu) / 255.0f, a};
}

constexpr SkColor4f kPaper = hex(0xf4e8e4);   // aged ivory, pink undertone
constexpr SkColor4f kInk = hex(0x241c15);     // engraver's warm black-brown
constexpr SkColor4f kInkSoft = hex(0x241c15, 0.55f);
constexpr SkColor4f kFox = hex(0xc9a688, 0.075f);

// per band: the paper-side wash and the ink dot laid over it
constexpr SkColor4f kBlueWash = hex(0xc6d1d5);
constexpr SkColor4f kBlueInk = hex(0x527a88);
constexpr SkColor4f kRoseWash = hex(0xe6d1cc);
constexpr SkColor4f kRoseInk = hex(0xa9776b);
constexpr SkColor4f kGreyWash = hex(0xdfd5ce);
constexpr SkColor4f kGreyInk = hex(0x241f19);

// ---------------------------------------------------------------------------
// geometry — canvas 1900x1032 keeps the plate's 35:19 ratio

constexpr float kW = 1900.0f;
constexpr float kH = 1032.0f;
constexpr float kK = 17.0f; // px per sqrt(rate); ONE scale for both wheels
constexpr SkPoint kC1{1397, 386}; // Diagram 1 (right) — Apr 1854..Mar 1855
constexpr SkPoint kC2{430, 384};  // Diagram 2 (left)  — Apr 1855..Mar 1856
constexpr float kR1 = 543.7f;     // kK * sqrt(1022.8), Jan 1855 disease
constexpr float kR2 = 267.5f;     // kK * sqrt(247.6),  Jun 1855 disease

// ---------------------------------------------------------------------------
// the data (HistData::Nightingale). Listed in WHEEL order: the engraver's
// seam is the June/July boundary at 12 o'clock, so the wheel runs
// July..June even though the report year runs April..March.

struct Month {
  const char *label; // outer label line
  const char *line2; // inner label line ("" = single line)
  float disease, wounds, other; // annual rate per 1000
};

// bearing = degrees clockwise from 12 o'clock; month i spans [30i, 30i+30]
const std::array<Month, 12> kD1 = {{
    {"JULY", "", 150.0f, 0.0f, 9.6f},          // Jul 1854
    {"AUGUST", "", 328.5f, 0.4f, 11.9f},       // Aug 1854
    {"SEPTEMBER", "", 312.2f, 32.1f, 27.7f},   // Sep 1854
    {"OCTOBER", "", 197.0f, 51.7f, 50.1f},     // Oct 1854
    {"NOVEMBER", "", 340.6f, 115.8f, 42.8f},   // Nov 1854
    {"DECEMBER", "", 631.5f, 41.7f, 48.0f},    // Dec 1854
    {"JANUARY", "1855", 1022.8f, 30.7f, 120.0f}, // Jan 1855 — the maximum
    {"FEBRUARY", "", 822.8f, 16.3f, 140.1f},   // Feb 1855
    {"MARCH", "1855.", 480.3f, 12.8f, 68.6f},  // Mar 1855
    {"APRIL", "1854", 1.4f, 0.0f, 7.0f},       // Apr 1854
    {"MAY", "", 6.2f, 0.0f, 4.6f},             // May 1854
    {"JUNE", "", 4.7f, 0.0f, 2.5f},            // Jun 1854
}};

const std::array<Month, 12> kD2 = {{
    {"JULY", "", 107.5f, 37.7f, 9.3f},     // Jul 1855
    {"AUGUST", "", 129.9f, 44.1f, 6.7f},   // Aug 1855
    {"SEPTEMBER", "", 47.5f, 69.4f, 5.0f}, // Sep 1855
    {"OCTOBER", "", 32.8f, 13.6f, 4.6f},   // Oct 1855
    {"NOVEMBER", "", 56.4f, 10.5f, 10.1f}, // Nov 1855
    {"DECEMBER", "", 25.3f, 5.0f, 7.8f},   // Dec 1855
    {"JANUARY", "", 11.4f, 0.5f, 13.0f},   // Jan 1856
    {"FEBRUARY", "", 6.6f, 0.0f, 5.2f},    // Feb 1856
    {"MARCH", "", 3.9f, 0.0f, 9.1f},       // Mar 1856
    {"APRIL", "1855", 177.5f, 17.9f, 21.2f}, // Apr 1855
    {"MAY", "", 171.8f, 16.6f, 12.5f},     // May 1855
    {"JUNE", "", 247.6f, 64.5f, 9.6f},     // Jun 1855 — this wheel's maximum
}};

// ---------------------------------------------------------------------------
// the legend, transcribed verbatim off the plate (period spelling
// "Preventible" kept). Continuation lines carry the hanging indent.

struct LegendLine {
  int indent;
  const char *text;
};
const std::array<LegendLine, 12> kLegendText = {{
    {0, "The Areas of the blue, red, & black wedges are each measured from"},
    {1, "the centre as the common vertex."},
    {0, "The blue wedges measured from the centre of the circle represent area"},
    {1, "for area the deaths from Preventible or Mitigable Zymotic diseases; the"},
    {1, "red wedges measured from the centre the deaths from wounds; & the"},
    {1, "black wedges measured from the centre the deaths from all other causes."},
    {0, "The black line across the red triangle in Novr. 1854 marks the boundary"},
    {1, "of the deaths from all other causes during the month."},
    {0, "In October 1854, & April 1855, the black area coincides with the red;"},
    {1, "in January & February 1855, the blue coincides with the black."},
    {0, "The entire areas may be compared by following the blue, the red & the"},
    {1, "black lines enclosing them."},
}};

// ---------------------------------------------------------------------------
// helpers

constexpr float kDeg = 3.14159265358979f / 180.0f;

/** Bearing (deg clockwise from 12 o'clock) + radius -> canvas point. */
SkPoint polar(SkPoint c, float radius, float bearingDeg) {
  return {c.x() + radius * std::sin(bearingDeg * kDeg),
          c.y() - radius * std::cos(bearingDeg * kDeg)};
}

/** A square box of radius @p r centred on @p c, in the parent's space —
 *  the frame every shapes::sector / shapes::arc wedge is inscribed in. */
Element discBox(SkPoint c, float r) {
  return box().absolute().left(c.x() - r).top(c.y() - r).width(2 * r).height(
      2 * r);
}

/** A straight spoke from the box centre out to radiusFraction. */
std::function<SkPath(SkSize)> spoke(float radiusFraction, float bearing) {
  return [radiusFraction, bearing](SkSize s) {
    SkPathBuilder p;
    const float cx = s.width() * 0.5f, cy = s.height() * 0.5f;
    const float r = std::min(cx, cy) * radiusFraction;
    p.moveTo(cx, cy);
    p.lineTo(cx + r * std::sin(bearing * kDeg), cy - r * std::cos(bearing * kDeg));
    return p.detach();
  };
}

/** Fractal LUMINANCE noise (equal channels) — plate tone and the ink-density
 *  wander inside each band. `patterns::grain()` is the library's answer and
 *  is what this should be, but its shader defines helper functions that call
 *  each other, and SkSL's inliner faults on a runtime effect minted inside a
 *  sketch dylib when the HOST raster-compiles it. Keeping main() monolithic
 *  — exactly how patterns::halftoneRamp is written — sidesteps it. */
Material lithoTone(float frequency, float seed) {
  static const sk_sp<SkRuntimeEffect> fx = [] {
    auto [effect, err] = SkRuntimeEffect::MakeForShader(SkString(R"(
      uniform float uFreq;
      uniform float uSeed;
      half4 main(float2 pos) {
        float2 q = pos * uFreq;
        float sum = 0.0, amp = 0.5, tot = 0.0;
        for (int o = 0; o < 4; ++o) {
          float2 i = floor(q), f = fract(q);
          float2 u = f * f * (3.0 - 2.0 * f);
          float2 a = fract(i * float2(123.34, 456.21) + uSeed);
          a += dot(a, a + 45.32);
          float2 b = fract((i + float2(1, 0)) * float2(123.34, 456.21) + uSeed);
          b += dot(b, b + 45.32);
          float2 c = fract((i + float2(0, 1)) * float2(123.34, 456.21) + uSeed);
          c += dot(c, c + 45.32);
          float2 d = fract((i + float2(1, 1)) * float2(123.34, 456.21) + uSeed);
          d += dot(d, d + 45.32);
          float n = mix(mix(fract(a.x * a.y), fract(b.x * b.y), u.x),
                        mix(fract(c.x * c.y), fract(d.x * d.y), u.x), u.y);
          sum += amp * n; tot += amp; q *= 2.0; amp *= 0.5;
        }
        float v = tot > 0.0 ? sum / tot : 0.5;
        return half4(half3(v), 1.0);
      })"));
    if (!effect)
      SkDebugf("lithoTone: %s\n", err.c_str());
    return effect;
  }();
  if (!fx)
    return Material::solid({0.5f, 0.5f, 0.5f, 1});
  return Material::sksl(fx, {{"uFreq", frequency}, {"uSeed", seed}});
}

sigil::weave::TextStyle type(sk_sp<SkTypeface> face, float size,
                             SkColor4f color, float tracking = 0) {
  sigil::weave::TextStyle s;
  s.shaping.typeface = std::move(face);
  s.shaping.fontSize = size;
  s.shaping.letterSpacing = tracking;
  s.paint.foreground.setColor4f(color, nullptr);
  s.paint.foreground.setAntiAlias(true);
  return s;
}

Transition ramp(float delayMs, float durMs,
                ch::EaseFn ease = ch::easeOutQuad) {
  Transition t;
  t.duration = std::chrono::milliseconds((int)durMs);
  t.delay = std::chrono::milliseconds((int)delayMs);
  t.ease = std::move(ease);
  return t;
}

} // namespace

// ===========================================================================

struct NightingaleCoxcomb : sigil::compose::sketch::Sketch {
  // --- the plate's own reading order, as a clock (seconds) ---
  static constexpr float tTitle1 = 0.0f;
  static constexpr float tTitle2 = 0.9f;
  static constexpr float tCap1 = 1.0f;
  static constexpr float tSpoke1 = 1.15f;
  static constexpr float tWedge1 = 1.35f;   // + 0.115 s per month
  static constexpr float tLabel1 = 3.10f;
  static constexpr float tCap2 = 3.70f;
  static constexpr float tSpoke2 = 3.85f;
  static constexpr float tWedge2 = 4.05f;   // + 0.100 s per month
  static constexpr float tLabel2 = 5.45f;
  static constexpr float tLeader = 6.00f;
  static constexpr float tLegend = 6.40f;   // + 0.20 s per line
  static constexpr float tNeedle1 = 9.40f;
  static constexpr float tNeedle2 = 11.50f;
  static constexpr float tNeedleEnd = 13.10f;

  // bound outputs: the two index needles and the 24 rim flashes they ring
  ch::Output<float> needle1Deg{0}, needle1A{0};
  ch::Output<float> needle2Deg{0}, needle2A{0};
  std::array<ch::Output<float>, 24> flash;

  // held so their identity (and the speckle bake) survives re-describes
  Pattern blueGrain, roseGrain, greyGrain, blotch, foxing;
  Material blueMat, roseMat, greyMat, paperMat;

  sk_sp<SkTypeface> faceDisplay, faceGrotesque, faceLabel, faceScript;

  // ------------------------------------------------------------------
  // per-glyph text on a circle. SigilCompose has no text-on-path, and
  // layouts::AlongPath throws the tangent away (it passes nullptr to
  // getPosTan), so every glyph is placed and rotated by hand.
  //
  //   tangential: glyph-up points radially outward, advance follows the
  //               clockwise tangent  -> rotation = bearing
  //   radial:     advance runs outward along the spoke
  //               -> rotation = bearing - 90
  void arcRun(std::vector<Element> &out, sketch::SketchContext &ctx,
              const sigil::weave::TextStyle &style, SkPoint centre,
              const std::string &content, float bearingDeg, float radius,
              bool radial, float tracking, float delayMs,
              const std::string &keyBase) {
    if (content.empty() || radius <= 1.0f)
      return;

    std::vector<float> widths;
    widths.reserve(content.size());
    float total = 0;
    for (size_t i = 0; i < content.size(); ++i) {
      const char c = content[i];
      float w = (c == ' ')
                    ? style.shaping.fontSize * 0.30f
                    : ctx.measure(text(std::u8string(1, (char8_t)c), style))
                          .width();
      widths.push_back(w);
      total += w;
    }
    total += tracking * (float)(content.size() - 1);

    float cum = 0;
    for (size_t i = 0; i < content.size(); ++i) {
      const float w = widths[i];
      const float along = cum + w * 0.5f - total * 0.5f;
      cum += w + tracking;
      if (content[i] == ' ')
        continue;

      SkPoint at;
      float rotate = 0;
      if (radial) {
        at = polar(centre, radius + along, bearingDeg);
        rotate = bearingDeg - 90.0f;
      } else {
        const float b = bearingDeg + (along / radius) / kDeg;
        at = polar(centre, radius, b);
        rotate = b;
      }
      out.push_back(
          text(std::u8string(1, (char8_t)content[i]), style)
              .key(keyBase + std::to_string(i))
              .absolute()
              .centerAt(at)
              .rotate(rotate)
              .transformOrigin(0.5f, 0.5f)
              .opacity(withFrom(0.0f, 1.0f,
                                ramp(delayMs + (float)i * 16.0f, 180.0f))));
    }
  }

  // ------------------------------------------------------------------
  Element wheel(sketch::SketchContext &ctx, const std::array<Month, 12> &data,
                SkPoint centre, float rMax, float startSec, float stepSec,
                float spokeSec, int flashBase, const char *tag) {
    (void)ctx;
    const SkPoint local{rMax, rMax}; // the wheel box's own centre
    auto wheelBox = discBox(centre, rMax);

    // The 12 hairline spokes. On the plate a radial line only exists where
    // BOTH neighbouring months have ink, so each spoke runs out to the
    // smaller of its two months' rims — otherwise stray hairlines shoot
    // across the empty spring quadrant.
    auto rimOf = [&](int m) {
      const Month &d = data[(m % 12 + 12) % 12];
      return kK * std::sqrt(std::max({d.disease, d.wounds, d.other, 0.1f}));
    };
    for (int i = 0; i < 12; ++i) {
      const float len = std::min(rimOf(i - 1), rimOf(i)) * 0.98f;
      if (len < 4.0f)
        continue;
      wheelBox.child(
          box()
              .absolute()
              .inset(0)
              .key(std::string(tag) + "spoke" + std::to_string(i))
              .outline(spoke(len / rMax, (float)i * 30.0f))
              .stroke(stroke(0.7f, Fill::color(kInkSoft)))
              .trim(0.0f, withFrom(0.0f, 1.0f,
                                   ramp(spokeSec * 1000.0f + (float)i * 16.0f,
                                        220.0f))));
    }

    for (int m = 0; m < 12; ++m) {
      const Month &mo = data[m];
      // bearing (0 = 12 o'clock, clockwise) -> Skia canvas angle (0 = +x)
      const float skia0 = (float)m * 30.0f - 90.0f;
      // Painter's algorithm by magnitude: biggest first, so every band
      // shows its own colour with no stacking arithmetic anywhere.
      struct Band {
        float rate;
        const Material *mat;
        const char *name;
      };
      std::array<Band, 3> bands = {{{mo.disease, &blueMat, "b"},
                                    {mo.wounds, &roseMat, "r"},
                                    {mo.other, &greyMat, "k"}}};
      std::sort(bands.begin(), bands.end(),
                [](const Band &a, const Band &b) { return a.rate > b.rate; });

      const float delay = (startSec + stepSec * (float)m) * 1000.0f;
      for (const Band &band : bands) {
        if (band.rate <= 0.0f)
          continue;
        const float r = kK * std::sqrt(band.rate);
        wheelBox.child(
            discBox(local, r)
                .key(std::string(tag) + band.name + std::to_string(m))
                .outline(shapes::sector(skia0, 30.0f))
                .fill(*band.mat)
                .stroke(stroke(1.0f, Fill::color(kInk)))
                .transformOrigin(0.5f, 0.5f)
                .scale(withFrom(0.002f, 1.0f,
                                ramp(delay, 620.0f, ch::easeOutExpo))));
      }

      // the flash the index needle rings out of each month's rim
      const float rim =
          kK * std::sqrt(std::max({mo.disease, mo.wounds, mo.other, 1.0f})) +
          10.0f;
      wheelBox.child(discBox(local, rim)
                         .key(std::string(tag) + "flash" + std::to_string(m))
                         .outline(shapes::arc(skia0 + 1.0f, 28.0f))
                         .stroke(stroke(2.4f, Fill::color(hex(0xc8a24a, 0.9f))))
                         .opacity(&flash[flashBase + m]));
    }
    return wheelBox;
  }

  // ------------------------------------------------------------------
  Element needle(SkPoint centre, float rMax, const ch::Output<float> *deg,
                 const ch::Output<float> *alpha, const char *key) {
    return discBox(centre, rMax)
        .key(key)
        .outline(spoke(1.0f, 0.0f))
        .stroke(stroke(1.4f, Fill::color(hex(0xd8b45c))))
        .background(shadow(hex(0xd8b45c, 0.5f), {0, 0}, 9))
        .transformOrigin(0.5f, 0.5f)
        .rotate(deg)
        .opacity(alpha)
        .cache(Cache::None);
  }

  // ------------------------------------------------------------------
  Element describe(sketch::SketchContext &ctx) {
    auto root = stack().fill(Fill::color(kPaper));

    // ---- paper: fractal mottle, sparse foxing, a soft vignette ------
    root.child(box().absolute().inset(0).fill(paperMat).opacity(0.17f).blend(
        SkBlendMode::kSoftLight));
    root.child(box().absolute().inset(0).fill(foxing.material()));
    root.child(box().absolute().inset(0).fill(radialGradient(
        {kW * 0.5f, kH * 0.5f}, kW * 0.72f,
        {hex(0x000000, 0.0f), hex(0x000000, 0.0f), hex(0x6b4a33, 0.085f)},
        {0.0f, 0.70f, 1.0f})));

    // ---- the reverse page showing through (custom leaf, raw Skia) ----
    root.child(custom([this](SkCanvas &canvas, const PaintContext &) {
                 if (!faceDisplay)
                   return;
                 SkFont f(faceDisplay, 46);
                 SkPaint p;
                 p.setAntiAlias(true);
                 p.setColor4f(hex(0x241c15, 0.055f), nullptr);
                 canvas.save();
                 canvas.translate(760, 118); // mirrored: the verso title
                 canvas.scale(-1, 1);
                 canvas.drawString("ENGLAND", 0, 0, f, p);
                 canvas.restore();
               })
                   .absolute()
                   .inset(0));

    // ---- the plate mark: the physical impression of the copper ------
    root.child(box()
                   .absolute()
                   .inset(26)
                   .fill(Fill::none())
                   .stroke(stroke(1.0f, Fill::color(hex(0x8a7060, 0.20f)))));
    root.child(box()
                   .absolute()
                   .inset(28)
                   .fill(Fill::none())
                   .stroke(stroke(1.0f, Fill::color(hex(0xffffff, 0.35f)))));

    // ---- the spine fold at the sheet's centre -----------------------
    root.child(box().absolute().left(938).top(0).width(24).height(kH).fill(
        linearGradient({0, 0}, {24, 0},
                       {hex(0x3a2a20, 0.0f), hex(0x3a2a20, 0.06f),
                        hex(0xffffff, 0.09f), hex(0x3a2a20, 0.0f)},
                       {0.0f, 0.42f, 0.60f, 1.0f})));

    // ---- title block -------------------------------------------------
    const auto title1 = type(faceDisplay, 47, kInk, 1.0f);
    const auto title2 = type(faceGrotesque, 30, kInk, 0.8f);

    GlyphFx t1;
    t1.effect = glyphfx::typeOn();
    t1.stagger = {.eachMs = 0, .amountMs = 620, .durationMs = 40};
    t1.progress = withFrom(0.0f, 1.0f, ramp(tTitle1 * 1000, 700, ch::easeNone));
    root.child(text(toU8("DIAGRAM of the CAUSES of MORTALITY"), title1)
                   .key("title1")
                   .glyphFx(std::move(t1))
                   .absolute()
                   .centerAt({968, 40}));

    GlyphFx t2;
    t2.effect = glyphfx::typeOn();
    t2.stagger = {.eachMs = 0, .amountMs = 340, .durationMs = 40};
    t2.progress = withFrom(0.0f, 1.0f, ramp(tTitle2 * 1000, 400, ch::easeNone));
    root.child(text(toU8("in the ARMY in the EAST."), title2)
                   .key("title2")
                   .glyphFx(std::move(t2))
                   .absolute()
                   .centerAt({952, 84}));

    // the double hairline under the title
    for (int i = 0; i < 2; ++i)
      root.child(box()
                     .absolute()
                     .left(775)
                     .top(108.0f + (float)i * 4.0f)
                     .width(368)
                     .height(1)
                     .fill(Fill::color(kInk))
                     .transformOrigin(0.0f, 0.5f)
                     .scale(withFrom(0.0f, 1.0f,
                                     ramp(tTitle2 * 1000 + 220 + (float)i * 60,
                                          420, ch::easeOutQuint))));

    // ---- the two diagram captions -----------------------------------
    const auto capNum = type(faceGrotesque, 26, kInk);
    const auto capText = type(faceGrotesque, 24, kInk, 0.6f);
    auto caption = [&](const char *num, const char *label, float cx,
                       float numX, float startSec, const char *key) {
      root.child(text(toU8(num), capNum)
                     .key(std::string(key) + "n")
                     .absolute()
                     .centerAt({numX, 34})
                     .opacity(withFrom(0.0f, 1.0f, ramp(startSec * 1000, 320))));
      root.child(text(toU8(label), capText)
                     .key(std::string(key) + "t")
                     .absolute()
                     .centerAt({cx, 78})
                     .opacity(withFrom(0.0f, 1.0f,
                                       ramp(startSec * 1000 + 90, 320))));
      root.child(box()
                     .absolute()
                     .left(cx - 150)
                     .top(94)
                     .width(300)
                     .height(1)
                     .fill(Fill::color(kInkSoft))
                     .transformOrigin(0.0f, 0.5f)
                     .scale(withFrom(0.0f, 1.0f,
                                     ramp(startSec * 1000 + 180, 380,
                                          ch::easeOutQuint))));
    };
    caption("1.", "APRIL 1854 to MARCH 1855.", 1313, 1487, tCap1, "cap1");
    caption("2.", "APRIL 1855 to MARCH 1856.", 413, 394, tCap2, "cap2");

    // ---- the wheels --------------------------------------------------
    root.child(wheel(ctx, kD1, kC1, kR1, tWedge1, 0.115f, tSpoke1, 0, "a"));
    root.child(wheel(ctx, kD2, kC2, kR2, tWedge2, 0.100f, tSpoke2, 12, "b"));

    // ---- the ring labels: each hugging its own wedge's rim ----------
    const auto labelStyle = type(faceLabel, 20, kInk, 0.4f);
    const auto smallLabel = type(faceLabel, 16, kInk, 0.2f);
    std::vector<Element> labels;

    // The floor is not decoration: twelve labels must fit the circumference
    // they sit on, so the ring cannot close tighter than 12 * (widest label)
    // / 2pi. That is why the plate sets the small left wheel in a smaller
    // face — the same constraint, solved the same way.
    auto ringLabels = [&](const std::array<Month, 12> &data, SkPoint centre,
                          float rMax, float floorR,
                          const sigil::weave::TextStyle &style, float gap,
                          float step, float startSec, const char *tag) {
      for (int m = 0; m < 12; ++m) {
        const Month &mo = data[m];
        const float rim =
            kK * std::sqrt(std::max({mo.disease, mo.wounds, mo.other, 0.5f}));
        const float base = std::max(rim + gap, floorR);
        const float bearing = (float)m * 30.0f + 15.0f;
        const float delay = (startSec + (float)m * 0.028f) * 1000.0f;
        const bool twoLines = mo.line2[0] != '\0';
        arcRun(labels, ctx, style, centre, mo.label, bearing,
               base + (twoLines ? step : 0.0f), false, 0.5f, delay,
               std::string(tag) + "L" + std::to_string(m) + "a");
        if (twoLines)
          arcRun(labels, ctx, style, centre, mo.line2, bearing, base, false,
                 0.5f, delay + 60.0f,
                 std::string(tag) + "L" + std::to_string(m) + "b");
      }
    };
    ringLabels(kD1, kC1, kR1, 172.0f, labelStyle, 26.0f, 24.0f, tLabel1, "a");
    ringLabels(kD2, kC2, kR2, 152.0f, smallLabel, 17.0f, 18.0f, tLabel2, "b");

    // the campaign annotations — set RADIALLY along their spoke
    arcRun(labels, ctx, smallLabel, kC1, "BULGARIA", 358.0f, 150.0f, true, 1.2f,
           tLabel1 * 1000 + 380, "bulg");
    arcRun(labels, ctx, smallLabel, kC1, "CRIMEA", 94.0f, 268.0f, true, 1.2f,
           tLabel1 * 1000 + 460, "crim");
    // the left wheel's year marker, upside down at 6 o'clock — which is
    // simply the outward-up rule arriving at the bottom of the circle
    arcRun(labels, ctx, smallLabel, kC2, "1856", 180.0f, 145.0f, false, 0.6f,
           tLabel2 * 1000 + 300, "y1856");

    for (Element &e : labels)
      root.child(std::move(e));

    // ---- the dashed leader between the two wheels -------------------
    PathFormat dash = stroke(1.1f, Fill::color(kInk));
    dash.dashIntervals = {7.0f, 5.0f};
    root.child(box()
                   .absolute()
                   .inset(0)
                   .key("leader")
                   .fill(Fill::none())
                   .outline([](SkSize) {
                     SkPathBuilder p;
                     p.moveTo(196, 380);
                     p.lineTo(614, 522);
                     p.lineTo(1024, 374);
                     return p.detach();
                   })
                   .stroke(dash)
                   .trim(0.0f, withFrom(0.0f, 1.0f,
                                        ramp(tLeader * 1000, 620,
                                             ch::easeOutQuad))));

    // ---- the engraved-hand legend -----------------------------------
    const auto script = type(faceScript, 27, kInk);
    for (size_t i = 0; i < kLegendText.size(); ++i) {
      GlyphFx pen;
      pen.effect = glyphfx::typeOn();
      pen.stagger = {.eachMs = 0, .amountMs = 620, .durationMs = 30};
      pen.progress = withFrom(
          0.0f, 1.0f,
          ramp(tLegend * 1000 + (float)i * 200.0f, 660, ch::easeNone));
      root.child(text(toU8(kLegendText[i].text), script)
                     .key("leg" + std::to_string(i))
                     .glyphFx(std::move(pen))
                     .absolute()
                     .left(171.0f + (float)kLegendText[i].indent * 22.0f)
                     .top(628.0f + (float)i * 30.7f));
    }

    // ---- printer's imprint ------------------------------------------
    root.child(text(toU8("Harrison & Sons, St. Martin's Lane."),
                    type(faceScript, 20, kInkSoft))
                   .key("imprint")
                   .absolute()
                   .centerAt({1712, 1004})
                   .opacity(withFrom(0.0f, 1.0f,
                                     ramp(tLegend * 1000 + 2500, 600))));

    // ---- the index needles ------------------------------------------
    root.child(needle(kC1, kR1, &needle1Deg, &needle1A, "needle1"));
    root.child(needle(kC2, kR2, &needle2Deg, &needle2A, "needle2"));

    return root;
  }

  // ------------------------------------------------------------------
  void setup(sketch::SketchContext &ctx) override {
    ctx.canvas(kW, kH);
    ctx.background(kPaper);

    auto family = [&](const char *name, SkFontStyle style) -> sk_sp<SkTypeface> {
      if (!ctx.fonts || !ctx.fonts->fontManager())
        return nullptr;
      return ctx.fonts->fontManager()->matchFamilyStyle(name, style);
    };
    faceDisplay = family("Academy Engraved LET", SkFontStyle::Normal());
    faceGrotesque = family("Copperplate", SkFontStyle::Bold());
    faceLabel = family("Copperplate", SkFontStyle::Normal());
    faceScript = family("Snell Roundhand", SkFontStyle::Normal());
    if (!faceDisplay)
      faceDisplay = family("Bodoni 72", SkFontStyle::Bold());
    if (!faceScript)
      faceScript = family("Apple Chancery", SkFontStyle::Normal());

    // The litho tint: a paper-side wash with the ink dot field over it.
    // Two speckle layers per band — a fine one for the tint itself and a
    // coarse sparse one so the ink density visibly wanders, which is what
    // separates a stone-printed tint from a flat vector fill.
    auto band = [](SkColor4f wash, SkColor4f ink, int fine, int coarse,
                   uint32_t seed, Pattern &grainOut) {
      grainOut = patterns::speckle(64, fine, 0.42f, 1.25f, {ink});
      grainOut.seed(seed);
      Pattern blot = patterns::speckle(150, coarse, 2.5f, 7.0f,
                                       {SkColor4f{ink.fR, ink.fG, ink.fB, 0.12f}});
      blot.seed(seed * 7 + 3);
      return Material::blend(
          {{Material::solid(wash), SkBlendMode::kSrc},
           {grainOut.material(), SkBlendMode::kSrcOver},
           {blot.material(), SkBlendMode::kSrcOver},
           // ink density wanders across the stone: LUMINANCE noise, so it
           // reads as light on the tint instead of hue-shifting it
           {lithoTone(0.010f, (float)seed), SkBlendMode::kSoftLight}});
    };
    blueMat = band(kBlueWash, kBlueInk, 830, 26, 11, blueGrain);
    roseMat = band(kRoseWash, kRoseInk, 380, 18, 23, roseGrain);
    greyMat = band(kGreyWash, kGreyInk, 630, 34, 37, greyGrain);

    paperMat = lithoTone(0.011f, 5.0f);
    foxing = patterns::speckle(210, 3, 1.5f, 5.5f, {kFox});
    foxing.seed(91);

    // The needles and the rim flashes they ring.
    ctx.ticker.add([this, t = 0.0](double dt) mutable {
      t += dt;
      const float s = (float)t;
      auto sweep = [&](float t0, float t1, ch::Output<float> &deg,
                       ch::Output<float> &alpha, int base) {
        if (s < t0 || s > t1 + 0.45f) {
          alpha = 0.0f;
          return;
        }
        const float u = std::clamp((s - t0) / (t1 - t0), 0.0f, 1.0f);
        deg = u * 360.0f;
        alpha = s > t1 ? std::max(0.0f, 1.0f - (s - t1) / 0.45f)
                       : std::min(1.0f, (s - t0) / 0.15f);
        for (int m = 0; m < 12; ++m) {
          const float centreB = (float)m * 30.0f + 15.0f;
          float d = std::fabs(deg.value() - centreB);
          if (d > 180.0f)
            d = 360.0f - d;
          flash[base + m] = alpha.value() * std::max(0.0f, 1.0f - d / 13.0f);
        }
      };
      sweep(tNeedle1, tNeedle2 - 0.1f, needle1Deg, needle1A, 0);
      sweep(tNeedle2, tNeedleEnd, needle2Deg, needle2A, 12);
      return true;
    });

    ctx.composer.render(describe(ctx));
  }

  void update(double, sketch::SketchContext &) override {}
};

SIGIL_SKETCH(NightingaleCoxcomb)
