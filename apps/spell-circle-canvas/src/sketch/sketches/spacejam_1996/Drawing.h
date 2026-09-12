#pragma once

#include <include/core/SkCanvas.h>
#include <include/core/SkFontMgr.h>
#include <include/core/SkPathBuilder.h>
#include <include/core/SkPicture.h>
#include <include/core/SkString.h>
#include <include/effects/SkRuntimeEffect.h>
#include <sigilcompose/brush/Adaptors.h>
#include <sigilcompose/brush/Decorations.h>
#include <sigilcompose/core/Paint.h>
#include <sigilcompose/core/Pattern.h>
#include <sigilcompose/kit/Frame.h>
#include <sigilcompose/kit/Layouts.h>
#include <sigilgeometry/kit/Generators.h>
#include <sigilmaterial/kit/Patterns.h>
#include <sigilmaterial/skia/Color.h>
#include <sigilmaterial/skia/Effect.h>
#include <sigilmaterial/skia/Paint.h>
#include <sigilmeasure/check/Check.h>
#include <sigilmotion/bind/Bind.h>
#include <sigilsketch/canvas/Sketch.h>
#include <sigilsketch/kit/Page.h>
#include <sigilsketch/kit/Rows.h>
#include <sigilsketch/kit/Theme.h>
#include <sigilweave/ports/SystemFontManager.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <string>
#include <vector>

namespace sketch = sigil::sketch;
namespace mskia = sigil::material::skia;
namespace motion = sigil::motion;
namespace shapes = sigil::geometry::shapes;
namespace weave = sigil::weave;
namespace measure = sigil::measure;

using namespace sigil::compose;
using namespace std::chrono_literals;
namespace ch = choreograph;

namespace sj {

// ---------------------------------------------------------------------------
// Scale. Native page pixels × 2 = canvas pixels. Divide anything here by 2
// to recover the 1996 number.

constexpr float kScale = 2.0f;
constexpr float S(float pagePx) { return pagePx * kScale; }

// ---------------------------------------------------------------------------
// Colour: the AUTHORING quantisation — RGB555, not the web cube

/** snap5: c8 -> round(c8·31/255) -> (i<<3)|(i>>2). Every distinct component
 *  in all twelve nav palettes is a member of this 32-value set; only two of
 *  them are members of the 216-colour cube. */
inline float snap5f(float v) {
  int i = (int)std::lround(std::clamp(v, 0.0f, 1.0f) * 31.0f);
  return (float)(((uint32_t)i << 3u) | ((uint32_t)i >> 2u)) / 255.0f;
}
/** A nav-art colour, snapped to the grid the shipped art lives on. */
inline SkColor4f C5(uint32_t rgb, float a = 1.0f) noexcept {
  return {snap5f((float)((rgb >> 16u) & 0xffu) / 255.0f),
          snap5f((float)((rgb >> 8u) & 0xffu) / 255.0f),
          snap5f((float)(rgb & 0xffu) / 255.0f), a};
}
/** Raw, unsnapped — for the greyscale star tile, whose 8-bit greyscale
 *  palette does not sit on the 5-bit grid, and for the body text. Neither
 *  is nav art. */

// Straight out of the shipped HTML:
// <body bgcolor="#000000" text="#ff0000" link="#ff4c4c" ...>
constexpr SkColor4f kPageBlack = hexColor(0x000000);
constexpr SkColor4f kBodyText = hexColor(0xFF0000);

// The label treatment, pixel-sampled and identical on all twelve GIFs
// THE LABELS ARE NOT ALL YELLOW. The shipped art sets STELLAR SOUVENIRS,
// LUNAR TUNES and PRESS BOX SHUTTLE in WHITE and SITE MAP in yellow, and
// that split is the page's only typographic variation — flattened to one
// colour, twelve buttons read as one button repeated.
const SkColor4f kLabel = C5(0xFFFF00);
const SkColor4f kLabelWhite = C5(0xFFFFFF);
const SkColor4f kLabelInk = C5(0x080800);

// ---------------------------------------------------------------------------
// Type. Two live pieces of text on this page (the © line, and nothing else);
// twelve pieces of BAKED lettering, approximated with Impact.

inline sk_sp<SkTypeface> display() {
  return weave::ports::face({"Impact", "Arial Black"},
                            SkFontStyle::kNormal_Weight);
}
inline sk_sp<SkTypeface> serif() {
  return weave::ports::face({"Times New Roman", "Times"},
                            SkFontStyle::kNormal_Weight);
}

inline sigil::weave::TextStyle ty(const sk_sp<SkTypeface>& tf, float size,
                                  SkColor4f color, float track = 0) {
  sigil::weave::TextStyle s;
  s.shaping.typeface = tf;
  s.shaping.fontSize = size;
  s.shaping.letterSpacing = track;
  s.paint.foreground.setColor4f(color, nullptr);
  s.paint.foreground.setAntiAlias(true);
  return s;
}

inline std::u8string U(const char* s) { return toUtf8(s); }

/** The label outline, spelled with echo() because there is no glyph stroke.
 *  Eight re-stamps at ±r plus one at (2r, 2r): the shipped art carries a
 *  1 px black outline all round PLUS a 1 px offset shadow down-right, and
 *  stamping the run nine times is the closest honest reproduction of that
 *  with a fill-only text node. */
inline Element& outlineText(Element& e, float r) {
  const float d[8][2] = {{-1, 0},  {1, 0},  {0, -1}, {0, 1},
                         {-1, -1}, {1, -1}, {-1, 1}, {1, 1}};
  for (auto& v : d) e.echo({v[0] * r, v[1] * r}, kLabelInk);
  e.echo({2 * r, 2 * r}, kLabelInk);  // the offset shadow
  return e;
}

// ---------------------------------------------------------------------------
// Geometry sugar

inline Element rect(float x, float y, float w, float h) {
  return box()
      .left(Dimension(x))
      .top(Dimension(y))
      .width(Dimension(w))
      .height(Dimension(h));
}

/** A shaded sphere: a circle-outlined box of 2r centred on c. Every planet
 *  here is flat-shaded with a hard limb — two stops and a dark edge. */
inline Element sphere(SkPoint c, float r, mskia::Paint m) {
  return kit::disc(c, r).shape(shapes::circle()).fill(std::move(m));
}

// ---------------------------------------------------------------------------
// The one animated thing on the page: a sphere with rotating seams.
//
// Monolithic SkSL: everything inside one main(), with no user-defined
// functions and no uniform-guarded loops or breaks. A sketch dylib's shader
// source is compiled by the Skia the HOST links, and across that boundary
// those two constructs fault with no diagnostic.
//
// Two builds of one source: `uSpin` as a constant (Planet B-Ball, static)
// and `uTime` + quantizeTime(10) (fastbreak.gif, live, stepping at the
// GIF's own 100 ms frame delay).

inline sk_sp<SkRuntimeEffect> ballEffect(bool live) {
  const std::string decl =
      live ? "uniform float uTime;\n" : "uniform float uSpin;\n";
  const std::string var = live ? "uTime" : "uSpin";
  const std::string src = decl + R"(
uniform float2 uResolution;
uniform float4 uHi;
uniform float4 uLo;
uniform float4 uSeam;
uniform float  uSeamW;

half4 main(float2 xy) {
  float2 p = xy / max(uResolution, float2(1.0, 1.0)) * 2.0 - 1.0;
  float r2 = dot(p, p);
  float z  = sqrt(max(1.0 - r2, 0.0));
  float3 P = float3(p.x, p.y, z);

  // tilt the spin axis toward the viewer (the GIF's ball is not upright)
  float3 Q = float3(P.x, P.y * 0.940 - P.z * 0.342, P.y * 0.342 + P.z * 0.940);

  float ang = )" + var + R"( * 10.4719755;   // 600 deg/s = 60 deg per GIF frame
  float cs = cos(ang), sn = sin(ang);
  float3 R = float3(Q.x * cs - Q.z * sn, Q.y, Q.x * sn + Q.z * cs);

  // five seam planes: two meridians, one equator, two tilted side seams
  float d1 = abs(R.x);
  float d2 = abs(R.z);
  float d3 = abs(R.y * 0.985 + R.x * 0.174);
  float d4 = abs(R.y * 0.966 - R.z * 0.259);
  float d5 = abs(R.x * 0.707 + R.z * 0.707);
  float dm = min(min(min(d1, d2), min(d3, d4)), d5);
  float seam = 1.0 - smoothstep(uSeamW * 0.45, uSeamW, dm);

  float lit  = clamp(dot(normalize(float3(-0.42, -0.58, 0.70)), P), 0.0, 1.0);
  float3 body = mix(float3(uLo.rgb), float3(uHi.rgb), lit * lit);
  body *= mix(0.52, 1.0, smoothstep(0.0, 0.55, z));      // hard limb
  float3 col = mix(body, float3(uSeam.rgb), seam);

  float a = 1.0 - smoothstep(0.965, 1.0, r2);
  return half4(half3(col * a), half(a));
}
)";
  auto [effect, error] = SkRuntimeEffect::MakeForShader(SkString(src.c_str()));
  if (!effect) SkDebugf("spacejam ballEffect: %s\n", error.c_str());
  return effect;
}

inline mskia::Paint ballMaterial(bool live, SkColor4f hi, SkColor4f lo,
                                 SkColor4f seam, float seamW) {
  sk_sp<SkRuntimeEffect> fx = ballEffect(live);
  if (!fx) return mskia::Paint::solid(hi);
  mskia::Paint m = mskia::Paint::sksl(fx, {{"uSeamW", seamW}});
  m.uniform("uHi", hi);
  m.uniform("uLo", lo);
  m.uniform("uSeam", seam);
  if (live)
    m.quantizeTime(10.0f);  // fastbreak.gif: six frames, duration=100 on each
  else
    m.uniform("uSpin", 0.083f);  // one frozen frame
  return m;
}

// ---------------------------------------------------------------------------
// Gas-giant banding — torn wavy streaks, drawn as an overlay() so it paints
// OVER the body fill and UNDER the sphere-shading child.

inline float hash1(uint32_t n) {
  n = (n ^ 61u) ^ (n >> 16u);
  n *= 9u;
  n ^= n >> 4u;
  n *= 0x27d4eb2du;
  n ^= n >> 15u;
  return (float)(n & 0xffffffu) / (float)0xffffff;
}

struct Bands {
  std::vector<SkColor4f> inks;
  int count = 6;
  uint32_t seed = 1;
  float thick = 0.11f;    // fraction of the box height
  float wobble = 0.055f;  // vertical excursion
  float tear = 0.55f;     // how much the thickness pinches along x
  float bow = 0.20f;      // limb curvature
  float tilt = 0.0f;      // streaks running off the horizontal

  void paint(SkCanvas& canvas, const PaintContext& ctx) const {
    const float w = ctx.size.width(), h = ctx.size.height();
    if (w <= 0 || h <= 0 || inks.empty()) return;
    canvas.save();
    canvas.clipPath(ctx.outline, true);
    SkPaint p;
    p.setAntiAlias(true);
    for (int b = 0; b < count; ++b) {
      const uint32_t s = seed * 131u + (uint32_t)b * 7919u;
      const float yc = h * (0.10f + 0.80f * ((float)b + 0.5f) / (float)count +
                            (hash1(s) - 0.5f) * 0.05f);
      const float t = h * thick * (0.55f + 0.9f * hash1(s + 1u));
      const float a1 = h * wobble * (0.5f + hash1(s + 2u));
      const float a2 = h * wobble * 0.45f * hash1(s + 3u);
      const float f1 = 0.9f + 1.1f * hash1(s + 4u);
      const float f2 = 2.2f + 1.8f * hash1(s + 5u);
      const float ph1 = hash1(s + 6u) * 6.2831853f;
      const float ph2 = hash1(s + 7u) * 6.2831853f;
      const float tf = 1.4f + 1.6f * hash1(s + 8u);
      const float tp = hash1(s + 9u) * 6.2831853f;
      const float lat = (yc - h * 0.5f) / (h * 0.5f);

      SkPathBuilder top, bot;
      const int N = 72;
      for (int i = 0; i <= N; ++i) {
        const float u = (float)i / (float)N;
        const float x = u * w;
        const float dx = (x - w * 0.5f) / (w * 0.5f);
        const float wave = a1 * std::sin(f1 * u * 6.2831853f + ph1) +
                           a2 * std::sin(f2 * u * 6.2831853f + ph2);
        const float bend = bow * h * lat * dx * dx + tilt * h * dx * 0.5f;
        const float pinch = std::max(
            0.06f,
            1.0f - tear * (0.5f + 0.5f * std::sin(tf * u * 6.2831853f + tp)));
        const float y = yc + wave + bend;
        const float half = t * 0.5f * pinch;
        if (i == 0) {
          top.moveTo(x, y - half);
          bot.moveTo(x, y + half);
        } else {
          top.lineTo(x, y - half);
          bot.lineTo(x, y + half);
        }
      }
      SkPath band = top.detach();
      SkPath under = bot.detach();
      // walk the bottom edge back to close the ribbon
      SkPathBuilder closed;
      closed.addPath(band);
      const int pts = under.countPoints();
      for (int i = pts - 1; i >= 0; --i) closed.lineTo(under.getPoint(i));
      closed.close();
      p.setColor4f(inks[(size_t)b % inks.size()], nullptr);
      canvas.drawPath(closed.detach(), p);
    }
    canvas.restore();
  }
};

// ---------------------------------------------------------------------------
// The star tile — ONE 111×111 GIF, repeated on a visible lattice.
//
// 33 measured local maxima at L >= 45 (x, y, peak), pixel-sampled off
// bg_stars.gif. NOT scattered across the canvas: the same 33 stars repeat
// every 111 page px, and that repetition is part of how the page looks.

struct Star {
  int x, y, peak;
};
/** THE FIELD, AS A CONSTANT. Not a function-local static holding a
 *  vector: such a static has a dynamic initialiser and registers its
 *  destructor with the process, and a hot-reloaded sketch's dylib is
 *  unloaded out from under both. */
inline constexpr auto kStarField = std::to_array<Star>(
    {{96, 64, 253}, {69, 9, 244},  {59, 101, 238},  {44, 43, 235},
     {9, 104, 233}, {3, 66, 222},  {89, 79, 219},   {40, 105, 209},
     {14, 48, 205}, {16, 12, 194}, {50, 65, 188},   {52, 30, 161},
     {15, 85, 153}, {28, 52, 150}, {102, 102, 145}, {73, 87, 143},
     {38, 24, 140}, {94, 22, 138}, {107, 64, 133},  {43, 13, 130},
     {11, 32, 129}, {85, 45, 128}, {32, 84, 127},   {61, 36, 125},
     {13, 5, 120},  {107, 1, 112}, {98, 46, 112},   {22, 70, 110},
     {86, 21, 109}, {68, 68, 97},  {48, 80, 97},    {40, 0, 72},
     {107, 110, 68}});
/** Anisotropy test at r = 7 said these carry axial (+) diffraction spikes. */
inline bool axialSpike(int x, int y) {
  return (x == 94 && y == 22) || (x == 107 && y == 64) ||
         (x == 86 && y == 21) || (x == 48 && y == 80);
}
/** ...and these two, diagonal (×) ones. */
inline bool diagSpike(int x, int y) {
  return (x == 14 && y == 48) || (x == 16 && y == 12);
}

inline Element starTile() {
  const float T = S(111.0f);
  Element tile = box().width(Dimension(T)).height(Dimension(T));

  // Three very faint lens-flare ghosts, at the sampled centres and radii
  // (page px, 15-26). They are what stops the field reading as pure noise.
  // At this luminance the view transform's round collapses them to one or
  // two levels, which is exactly what an 8-bit screen did to them.
  const float ring[3][3] = {{14, 16, 26}, {17, 52, 19}, {80, 74, 15}};
  for (auto& g : ring)
    tile.child(kit::disc(SkPoint{S(g[0]), S(g[1])}, S(g[2]))
                   .fill(mskia::Paint::glowUnit({0.5f, 0.5f}, 1.0f,
                                                {{0.0f, {1, 1, 1, 0.0f}},
                                                 {0.74f, {1, 1, 1, 0.0f}},
                                                 {0.89f, {1, 1, 1, 0.030f}},
                                                 {1.0f, {1, 1, 1, 0.0f}}}))
                   .blend(SkBlendMode::kPlus));

  int bright = 0;
  for (const Star& s : kStarField) {
    const float L = (float)s.peak / 255.0f;
    // Sampled off the tile: half-intensity radius 2-4 px for the brightest
    // stars, visible extent about 3x that. Most of the tile sits below L16
    // and only 128 of its 12,321 pixels reach L128 — it is far darker than
    // it looks, and the density on the page comes from repetition, not from
    // brightness. So the falloff has to be steep: a linear ramp over the
    // full extent, summed over every repeat of the tile, turns the field
    // into a grey haze the artefact does not have.
    const float hr = 0.85f + 2.6f * L * L;
    const float R = S(2.7f * hr);
    tile.child(kit::disc(SkPoint{S((float)s.x), S((float)s.y)}, R)
                   .fill(mskia::Paint::glowUnit({0.5f, 0.5f}, 1.0f,
                                                {{0.0f, {L, L, L, 1.0f}},
                                                 {0.24f, {L, L, L, 0.66f}},
                                                 {0.44f, {L, L, L, 0.26f}},
                                                 {0.70f, {L, L, L, 0.055f}},
                                                 {1.0f, {L, L, L, 0.0f}}}))
                   .blend(SkBlendMode::kPlus));

    // Spikes: thin tapered lobes, and on this tile they are the dominant
    // visual, not the glows. Four read as axial (+) crosses and two as
    // diagonal (x) ones on the r=7 anisotropy test; the brightest
    // half-dozen carry a faint 8-point star the test cannot separate
    // because both axes are equally bright.
    const bool eight = (bright++ < 6);
    if (eight || axialSpike(s.x, s.y) || diagSpike(s.x, s.y)) {
      const int pts = eight ? 8 : 4;
      const float waist = eight ? 0.15f : 0.12f;
      const float len = S(eight ? 4.8f + 6.6f * L : 4.2f + 6.0f * L);
      Element sp = kit::disc(SkPoint{S((float)s.x), S((float)s.y)}, len)
                       .shape(shapes::star(pts, 0.035f, waist))
                       .fill(Fill::color({1, 1, 1, 0.38f + 0.42f * L}))
                       .blend(SkBlendMode::kPlus);
      if (diagSpike(s.x, s.y)) sp.rotate(45);
      tile.child(std::move(sp));
    }
  }
  return tile;
}

// ---------------------------------------------------------------------------
// The twelve nav GIFs, regenerated. Local coordinates are the image box
// (SCALE applied), so these drop straight into the measured page rects.

/** A nav label. The shipped ones are baked pixel art, set MUCH narrower than
 *  anything on the system: "SITE MAP" is eight glyphs in 37 px at a 10 px
 *  cap (0.46 advance-to-cap, against Impact's 0.62). So this sizes by the
 *  measured cap band first, condenses with scaleX down to a 0.70 floor —
 *  which is what a 1996 art director did by hand — and only then gives up
 *  cap height. `intrinsicSize()` is doing the work `<img width=>` did for the
 *  browser: the run has to fit its box before the table sees it. */
inline Element navLabel(sigil::weave::FontContext& fonts, const char* s,
                        float x, float y, float w, float capPx,
                        SkColor4f ink = kLabel) {
  const float track = 0.4f * kScale;
  auto styleAt = [&](float sz) { return ty(display(), sz, ink, track); };
  float size = capPx / 0.72f;  // Impact cap height ~0.72 em
  SkSize m = intrinsicSize(text(U(s), styleAt(size)), fonts);
  float sx = 1.0f;
  if (m.width() > w && m.width() > 1) {
    sx = w / m.width();
    if (sx < 0.70f) {  // past the condensing floor, give up cap height
      size *= sx / 0.70f;
      m = intrinsicSize(text(U(s), styleAt(size)), fonts);
      sx = (m.width() > w && m.width() > 1) ? w / m.width() : 1.0f;
    }
  }
  Element t = text(U(s), styleAt(size));
  outlineText(t, kScale);
  // scaleX is PAINT-only, so a condensed run still MEASURES at its natural
  // width and wraps against the image box. Pinning the node to that natural
  // width is what keeps it one line; the artBox's clip() takes the
  // overhang, and the paint-time condense brings it back inside.
  t.left(Dimension(x)).top(Dimension(y)).width(Dimension(m.width() + 4.0f));
  if (sx < 0.999f) t.scaleX(sx).transformOrigin(0.0f, 0.5f);
  return t;
}

/** A ring seen edge-on: an annulus on a squashed, rotated box. */
inline Element ring(SkPoint c, float rx, float ry, float rotDeg,
                    float innerRatio, mskia::Paint m) {
  return rect(c.fX - rx, c.fY - ry, rx * 2, ry * 2)
      .shape(shapes::annulus(innerRatio))
      .fill(std::move(m))
      .rotate(rotDeg);
}

inline Element artBox(float w, float h) {
  return stack().width(Dimension(w)).height(Dimension(h)).clip(true);
}

// --- p-souvenirs.gif, 83x83 — the CENTRED glow, and half of the controlled
// --- comparison against the eight off-centre spheres.

}  // namespace sj
