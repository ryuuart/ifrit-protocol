// spacejam_1996.cpp — www.spacejam.com, the Warner Bros. Online home page
// for *Space Jam* (Joe Pytka / Warner Bros., 15 Nov 1996), as a 28.8 kbps
// modem and an 8-bit display delivered it.
//
// REFERENCE — the page is still live and still unmodified, moved in 2021 to
// https://www.spacejam.com/1996/ when the sequel took the root:
//   · spacejam_1996_index.html — the literal shipped HTML (every geometry
//     number below is <img width/height> or a table attribute out of it)
//   · img/bg_stars.gif + the twelve img/p-*.gif nav buttons + fast.gif /
//     fastbreak.gif / break.gif, all fetched from the live server; palettes
//     pixel-sampled, GIF headers parsed for size / interlace / frame delays
//   · spacejam_1996_render_640x800.png — the page through headless Chrome
//     at a 640 px viewport, which is what this capture is diffed against
//   · CSS 2.1 §17.5.2.2 (automatic table layout) — the rule that produces
//     the twelve planet positions; Lynda Weinman, *Designing Web Graphics*
//     (1996) for the 216-colour cube; W3C's 1997 HTTP-performance note for
//     Netscape's 4-connection HTTP/1.0 default.
//
// RECONSTRUCTED, not traced: the twelve GIFs are regenerated procedurally
// from measured disc centres, radii and palettes. They are NOT pixel copies
// and do not pretend to be. The label face is baked pixel art in the
// original; Impact is an approximation of it. The Press Box shuttle and the
// SPACE JAM logotype are the two deliberate approximations: a lozenge-and-
// fins ship, and a heavy grotesque over a real rainbow swirl.
//
// THE 1996 STATE, not today's. The live HTML has the Fast Break row
// commented out and a 2021 Privacy/Terms/Accessibility/AdChoices row added.
// All three Fast Break GIFs are still on the server and still download, so
// the row is restored here and the 2021 links are dropped. Everything below
// the row therefore sits 40 px (80 canvas px) lower than the page you get
// if you load it now.
//
// WHAT THIS PAGE IS, AND WHY IT IS A DIFFERENT PROBLEM. It is a DOCUMENT,
// not a panel: no chrome, no bevel, no border, not one stroked rectangle.
// Three things carry it instead.
//
//  1. THE LAYOUT IS PRODUCED BY AN ALGORITHM. `layouts::Table` is the HTML
//     auto table, and this page is set with it: five columns, five rows, a
//     colspan=2, a colspan=3 rowspan=2 and a rowspan=2, per-cell
//     align/valign, and the proportional surplus distribution that is why
//     every planet lands on a FRACTIONAL pixel. Nothing is hand-placed —
//     each <TD> names the cells it claims on the child that fills it, with
//     `.cells()` and `.cellAlign()`. `reportGrid()` prints the columns,
//     rows and image rects the table resolves next to the numbers headless
//     Chrome measures, so the agreement is checkable on every run rather
//     than asserted here.
//
//  2. THE DISPLAY CONSTRAINT IS APPLIED TO THE FINISHED FRAME. Two
//     quantisations, in the two places they actually happened. Nothing on
//     this page is web-safe — of the 32 distinct component values in the
//     twelve nav GIFs exactly two (0x00, 0xFF) are in the 216 cube, while
//     all of them are members of {(i<<3)|(i>>2)}, the RGB555→888 expansion.
//     The art was authored at 15 bit and reduced to GIF; the web-safe cube
//     was a property of the USER'S SCREEN. So `C5()` snaps every authored
//     colour to the 5-bit grid at describe time, and `Composer::setView()`
//     rounds the whole finished frame to the 216-colour cube — a snap and
//     not a dither, for the reason stated at `viewEffect()`.
//
//  3. TIME IS A TRANSPORT PROPERTY. The sixteen files total 59,689 bytes,
//     fed through four connections at 3.0 KB/s effective — a schedule
//     simulated from the real byte counts rather than staged by hand, which
//     is why it is nothing like intuition. The starfield is requested FIRST
//     and, being large and sharing the line, lands TENTH: for the first
//     eleven seconds of real modem time the page is flat black with planets
//     floating on it. The logotype, the biggest file, finishes dead last,
//     seconds after everything else. `kSpeedup` compresses all of it into
//     sketch time, and the page reloads a few seconds after the last byte.
//
// FOUR FINDINGS FROM THE FILES, honoured rather than smoothed over:
//   · None of the sixteen GIFs is interlaced (every image-descriptor flag
//     is zero), so the reveal is NOT the remembered four-pass one — it is
//     a hard top-down band, one scanline row at a time, quantised here to
//     the 1996 pixel grid.
//   · bg_stars.gif is 8-bit GREYSCALE (all 256 entries r==g==b — a
//     different authoring path from the nav art) and does NOT tile
//     seamlessly. The 111 px lattice is visible in the real page and is
//     reproduced: ONE tile, repeated, edge stars cut. This is the case
//     where instancing is the wrong answer — the stars are not scattered
//     over the page, they are one `Pattern::tile` bake shown many times.
//   · Only ONE thing on this page ever moves: fastbreak.gif, 40×40, six
//     frames, `duration=100` on every one. `Paint::quantizeTime(10.0f)`
//     is that GIF's frame rate, exactly. Nothing else drifts, twinkles or
//     parallaxes, and the restraint is the reference.
//   · The page has NO DOCTYPE, so it renders in quirks mode — which is why
//     a cell containing `<br>×n` then an image measures exactly
//     n·18 + imageH, with no line-box strut descent added under the image.
//     Adding the 4 px of descent a standards-mode box would carry puts
//     every row below the first 4 px low against the reference render;
//     with none, all twelve images land. (Netscape 3 had no standards mode
//     at all, so quirks IS the 1996 behaviour.)
//
// ONE PROPERTY OF THE FINISHED FRAME, which is what makes it a study of
// this page and not a moving picture of it: exactly one thing moves after
// the load — the basketball, stepping at 10 Hz. The rest of the tree
// settles to cached pictures, so the page really does go static once
// loaded, the way the artefact does. The view transform is a pure
// per-pixel round with no lattice and no time in it, so it adds nothing
// that moves either.
//
// AND ONE THING THE TABLE ALGORITHM ITSELF ASKS FOR: column surplus is
// distributed PROPORTIONALLY across the columns, but a rowspan's height
// deficit is NOT. Chrome gives the whole of the logotype's overflow to the
// LAST row it spans and leaves the first untouched; distributing it
// proportionally instead makes the second row far too tall and drags every
// image below it down. Both branches are in `layouts::Table::solve()`.

#include <include/core/SkCanvas.h>
#include <include/core/SkFontMgr.h>
#include <include/core/SkPathBuilder.h>
#include <include/core/SkPicture.h>
#include <include/core/SkString.h>
#include <include/effects/SkRuntimeEffect.h>
#include <sigilcompose/brush/Decorations.h>
#include <sigilcompose/core/Paint.h>
#include <sigilmotion/bind/Bind.h>
#include <sigilgeometry/kit/Generators.h>
#include <sigilmaterial/skia/Effect.h>
#include <sigilmaterial/skia/Paint.h>
#include <sigilcompose/core/Pattern.h>
#include <sigilmaterial/kit/Patterns.h>
#include <sigilcompose/kit/Frame.h>
#include <sigilcompose/kit/Layouts.h>
#include <sigilcompose/brush/Adaptors.h>
#include <sigilsketch/canvas/Sketch.h>
#include <sigilweave/ports/SystemFontManager.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <string>
#include <vector>

namespace sketch = sigil::sketch;
namespace mskia = sigil::material::skia;
namespace motion = sigil::motion;
namespace shapes = sigil::geometry::shapes;
namespace weave = sigil::weave;

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
constexpr SkColor4f kPageBlack = hex(0x000000);
constexpr SkColor4f kBodyText = hex(0xFF0000);

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

inline sk_sp<SkTypeface> face(const char* family, int weight,
                              const char* fallback = nullptr) {
  auto mgr = sigil::weave::ports::systemFontManager();
  sk_sp<SkTypeface> f = mgr->matchFamilyStyle(
      family, SkFontStyle(weight, SkFontStyle::kNormal_Width,
                          SkFontStyle::kUpright_Slant));
  if (!f && fallback)
    f = mgr->matchFamilyStyle(fallback,
                              SkFontStyle(weight, SkFontStyle::kNormal_Width,
                                          SkFontStyle::kUpright_Slant));
  if (!f) f = mgr->matchFamilyStyle(nullptr, SkFontStyle::Normal());
  return f;
}
inline const sk_sp<SkTypeface>& display() {  // the baked-label approximation
  static sk_sp<SkTypeface> f =
      face("Impact", SkFontStyle::kNormal_Weight, "Arial Black");
  return f;
}
inline const sk_sp<SkTypeface>& serif() {  // the browser default, Times
  static sk_sp<SkTypeface> f =
      face("Times New Roman", SkFontStyle::kNormal_Weight, "Times");
  return f;
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

inline std::u8string U(const char* s) { return toU8(s); }

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
  return box().left(Dim(x)).top(Dim(y)).width(Dim(w)).height(Dim(h));
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
  static sk_sp<SkRuntimeEffect> cached[2];
  const int idx = live ? 1 : 0;
  if (cached[idx]) return cached[idx];
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
  cached[idx] = effect;
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
inline const std::vector<Star>& starData() {
  static const std::vector<Star> k = {
      {96, 64, 253}, {69, 9, 244},  {59, 101, 238},  {44, 43, 235},
      {9, 104, 233}, {3, 66, 222},  {89, 79, 219},   {40, 105, 209},
      {14, 48, 205}, {16, 12, 194}, {50, 65, 188},   {52, 30, 161},
      {15, 85, 153}, {28, 52, 150}, {102, 102, 145}, {73, 87, 143},
      {38, 24, 140}, {94, 22, 138}, {107, 64, 133},  {43, 13, 130},
      {11, 32, 129}, {85, 45, 128}, {32, 84, 127},   {61, 36, 125},
      {13, 5, 120},  {107, 1, 112}, {98, 46, 112},   {22, 70, 110},
      {86, 21, 109}, {68, 68, 97},  {48, 80, 97},    {40, 0, 72},
      {107, 110, 68}};
  return k;
}
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
  Element tile = box().width(Dim(T)).height(Dim(T));

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
  for (const Star& s : starData()) {
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
 *  cap height. `measure()` is doing the work `<img width=>` did for the
 *  browser: the run has to fit its box before the table sees it. */
inline Element navLabel(sigil::weave::FontContext& fonts, const char* s,
                        float x, float y, float w, float capPx,
                        SkColor4f ink = kLabel) {
  const float track = 0.4f * kScale;
  auto styleAt = [&](float sz) { return ty(display(), sz, ink, track); };
  float size = capPx / 0.72f;  // Impact cap height ~0.72 em
  SkSize m = measure(text(U(s), styleAt(size)), fonts);
  float sx = 1.0f;
  if (m.width() > w && m.width() > 1) {
    sx = w / m.width();
    if (sx < 0.70f) {  // past the condensing floor, give up cap height
      size *= sx / 0.70f;
      m = measure(text(U(s), styleAt(size)), fonts);
      sx = (m.width() > w && m.width() > 1) ? w / m.width() : 1.0f;
    }
  }
  Element t = text(U(s), styleAt(size));
  outlineText(t, kScale);
  // scaleX is PAINT-only, so a condensed run still MEASURES at its natural
  // width and wraps against the image box. Pinning the node to that natural
  // width is what keeps it one line; the artBox's clip() takes the
  // overhang, and the paint-time condense brings it back inside.
  t.left(Dim(x)).top(Dim(y)).width(Dim(m.width() + 4.0f));
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
  return stack().width(Dim(w)).height(Dim(h)).clip(true);
}

// --- p-souvenirs.gif, 83x83 — the CENTRED glow, and half of the controlled
// --- comparison against the eight off-centre spheres.
inline Element artSouvenirs(sigil::weave::FontContext& f) {
  const float W = S(83), H = S(83);
  return artBox(W, H)
      .child(sphere({S(41.5f), S(47.5f)}, S(35),
                    mskia::Paint::glowUnit({0.5f, 0.5f}, 1.0f,
                                       {{0.0f, C5(0xEFEFEF)},
                                        {0.16f, C5(0xDEEFEF)},
                                        {0.52f, C5(0x29EFEF)},
                                        {0.80f, C5(0x08C6C6)},
                                        {1.0f, C5(0x006363)}}))
                 .stroke(stroke(S(1.5f), Fill::color(C5(0x005252)),
                                PathFormat::Align::Inner)))
      .child(navLabel(f, "STELLAR SOUVENIRS", 0, S(-1), W, S(10), kLabelWhite));
}

// --- p-jump.gif, 58x52 — the other centred glow.
inline Element artJump(sigil::weave::FontContext& f) {
  const float W = S(58), H = S(52);
  return artBox(W, H)
      .child(sphere({S(28.5f), S(30.0f)}, S(21),
                    mskia::Paint::glowUnit({0.46f, 0.60f}, 1.0f,
                                       {{0.0f, C5(0xFFFFFF)},
                                        {0.22f, C5(0xADF7A5)},
                                        {0.52f, C5(0x39D631)},
                                        {0.86f, C5(0x009400)},
                                        {1.0f, C5(0x006B00)}}))
                 .stroke(stroke(S(1.5f), Fill::color(C5(0x005A00)),
                                PathFormat::Align::Inner)))
      .child(navLabel(f, "JUMP STATION", 0, S(0), W, S(10)));
}

// --- p-bball.gif, 62x62 — the static build of the seam shader.
inline Element artBball(sigil::weave::FontContext& f) {
  const float W = S(62), H = S(62);
  return artBox(W, H)
      .child(kit::disc(SkPoint{S(31), S(37.5f)}, S(25.5f))
                 .shape(shapes::circle())
                 .fill(ballMaterial(false, C5(0xFF9C10), C5(0xC66300),
                                    C5(0x843900), 0.055f))
                 .stroke(stroke(S(1.2f), Fill::color(C5(0x632900)),
                                PathFormat::Align::Inner)))
      .child(navLabel(f, "PLANET B-BALL", 0, S(0), W, S(10)));
}

// --- p-jamcentral.gif, 55x67 — purple globe, green continents.
inline Element artJamCentral(sigil::weave::FontContext& f) {
  const float W = S(55), H = S(67);
  const SkPoint c{S(27.5f), S(40)};
  const float r = S(26);
  Element globe = sphere(c, r,
                         mskia::Paint::glowUnit({0.34f, 0.28f}, 1.32f,
                                            {{0.0f, C5(0xA542DE)},
                                             {0.30f, C5(0x8418CE)},
                                             {0.62f, C5(0x7B10C6)},
                                             {1.0f, C5(0x630894)}}))
                      .clip(true)
                      .stroke(stroke(S(1.5f), Fill::color(C5(0x9400DE)),
                                     PathFormat::Align::Inner));
  // Six landmasses, seeded blobs clipped to the disc. Their coordinates are
  // fractions of the disc box (2r), not of the image box.
  const float d = r * 2;
  struct Mass {
    float x, y, w, h;
    uint32_t seed;
    uint32_t ink;
  };
  const Mass mass[6] = {{0.04f, 0.10f, 0.44f, 0.26f, 11, 0x08E700},
                        {0.14f, 0.30f, 0.26f, 0.52f, 27, 0x00EF00},
                        {0.46f, 0.14f, 0.44f, 0.26f, 43, 0x00EF00},
                        {0.50f, 0.36f, 0.30f, 0.50f, 61, 0x08E700},
                        {0.74f, 0.28f, 0.24f, 0.30f, 83, 0x08E700},
                        {0.28f, 0.80f, 0.34f, 0.16f, 97, 0x00EF00}};
  for (const Mass& m : mass)
    globe.child(rect(m.x * d, m.y * d, m.w * d, m.h * d)
                    .shape(shapes::blob(m.seed, 0.62f, 13))
                    .fill(Fill::color(C5(m.ink))));
  return artBox(W, H)
      .child(std::move(globe))
      .child(navLabel(f, "JAM CENTRAL", 0, S(-1), W, S(10)));
}

/** The shared gas-giant recipe: solid body, torn bands as an overlay(),
 *  and one shading child for the highlight and the hard limb. */
inline Element gasGiant(SkPoint c, float r, SkColor4f body, SkColor4f limb,
                        SkColor4f hi, Bands bands) {
  Element d =
      sphere(c, r, mskia::Paint::solid(body))
          .clip(true)
          .overlay(std::move(bands))
          .stroke(stroke(S(1.5f), Fill::color(limb), PathFormat::Align::Inner));
  d.child(box().inset(0).fill(mskia::Paint::glowUnit({0.34f, 0.28f}, 1.35f,
                                                 {{0.0f, alpha(hi, 0.42f)},
                                                  {0.34f, alpha(hi, 0.10f)},
                                                  {0.62f, {0, 0, 0, 0}},
                                                  {0.90f, {0, 0, 0, 0.30f}},
                                                  {1.0f, {0, 0, 0, 0.62f}}})));
  return d;
}

// --- p-junior.gif, 49x57 — green body, yellow bands.
inline Element artJunior(sigil::weave::FontContext& f) {
  const float W = S(49), H = S(57);
  Bands b{{C5(0xFFFF00), C5(0xBDF700), C5(0xA5F700), C5(0x7BEF00), C5(0xFFFF00),
           C5(0xBDF700)},
          6,
          5,
          0.135f,
          0.040f,
          0.42f,
          0.24f,
          -0.05f};
  return artBox(W, H)
      .child(gasGiant({S(24), S(34)}, S(22.5f), C5(0x00DE00), C5(0x005A00),
                      C5(0xCEFFB5), std::move(b)))
      .child(navLabel(f, "JUNIOR JAM", 0, S(-1), W, S(10)));
}

// --- p-studiostore.gif, 94x72 — orange body, purple streaks, torn hard.
inline Element artStudioStore(sigil::weave::FontContext& f) {
  const float W = S(94), H = S(72);
  Bands b{
      {C5(0x7310C6), C5(0x6B08CE), C5(0x7310C6), C5(0x6B08CE), C5(0x7310C6)},
      5,
      17,
      0.150f,
      0.055f,
      0.60f,
      0.28f,
      -0.07f};
  return artBox(W, H)
      .child(gasGiant({S(48), S(42)}, S(29.5f), C5(0xFF9400), C5(0xE7A518),
                      C5(0xFFDE9C), std::move(b)))
      .child(navLabel(f, "WARNER STUDIO STORE", 0, S(-1), W, S(10)));
}

// --- p-behind.gif, 67x63 — navy body, four thin cyan cloud streaks.
inline Element artBehind(sigil::weave::FontContext& f) {
  const float W = S(67), H = S(63);
  Bands b{{C5(0x21FFFF), C5(0x21FFFF), C5(0x18E7EF), C5(0x21FFFF)},
          4,
          31,
          0.085f,
          0.045f,
          0.52f,
          0.30f,
          -0.17f};
  return artBox(W, H)
      .child(gasGiant({S(33), S(37)}, S(25.5f), C5(0x000873), C5(0x0010BD),
                      C5(0x2131C6), std::move(b)))
      .child(navLabel(f, "BEHIND THE JAM", 0, S(-1), W, S(10)));
}

// --- p-lunartunes.gif, 95x77 — blue Saturn, red ring: two arcs and a
// --- z-order, since the ring passes behind at the top and in front below.
inline Element artLunarTunes(sigil::weave::FontContext& f) {
  const float W = S(95), H = S(77);
  const SkPoint c{S(48), S(46)};
  auto ringMat = [] {
    return mskia::Paint::linearUnit({0, 0}, {0, 1},
                                {{0.0f, C5(0xF71018)},
                                 {0.38f, C5(0xF773A5)},
                                 {0.62f, C5(0xF71818)},
                                 {1.0f, C5(0xAD0810)}});
  };
  return artBox(W, H)
      .child(ring(c, S(47), S(16), -20, 0.62f, ringMat()).zIndex(0))
      .child(sphere(c, S(30),
                    mskia::Paint::glowUnit({0.34f, 0.28f}, 1.32f,
                                       {{0.0f, C5(0x0073E7)},
                                        {0.30f, C5(0x006BD6)},
                                        {0.66f, C5(0x0052AD)},
                                        {1.0f, C5(0x00317B)}}))
                 .stroke(stroke(S(1.5f), Fill::color(C5(0x00397B)),
                                PathFormat::Align::Inner))
                 .zIndex(1))
      // the front half: the same ellipse, clipped to below the sphere's centre
      .child(rect(0, c.fY, W, H - c.fY)
                 .clip(true)
                 .zIndex(2)
                 .child(ring({c.fX, -S(0)}, S(47), S(16), -20, 0.62f, ringMat())
                            .top(Dim(-c.fY))))
      .child(navLabel(f, "LUNAR TUNES", S(19), S(0), S(57), S(9), kLabelWhite));
}

// --- p-lineup.gif, 63x52 — the same construction, red and cyan.
inline Element artLineup(sigil::weave::FontContext& f) {
  const float W = S(63), H = S(52);
  const SkPoint c{S(33), S(31)};
  auto ringMat = [] {
    return mskia::Paint::linearUnit({0, 0}, {0, 1},
                                {{0.0f, C5(0x21FFFF)},
                                 {0.45f, C5(0x9CFFFF)},
                                 {0.75f, C5(0x21FFFF)},
                                 {1.0f, C5(0x089494)}});
  };
  return artBox(W, H)
      .child(ring(c, S(29), S(15), -22, 0.60f, ringMat()).zIndex(0))
      .child(sphere({S(38), S(32)}, S(17),
                    mskia::Paint::glowUnit({0.34f, 0.30f}, 1.30f,
                                       {{0.0f, C5(0xFF4A6B)},
                                        {0.28f, C5(0xFF425A)},
                                        {0.62f, C5(0xF71818)},
                                        {1.0f, C5(0xBD0810)}}))
                 .stroke(stroke(S(1.4f), Fill::color(C5(0xA50008)),
                                PathFormat::Align::Inner))
                 .zIndex(1))
      .child(rect(0, S(34), W, H - S(34))
                 .clip(true)
                 .zIndex(2)
                 .child(ring({c.fX, 0}, S(29), S(15), -22, 0.60f, ringMat())
                            .top(Dim(c.fY - S(34) - S(15)))))
      .child(navLabel(f, "THE LINEUP", S(8), S(-1), S(50), S(9)));
}

// --- p-sitemap.gif, 104x67 — a rainbow vortex and four yellow darts.
/** An arrowhead: tip forward, two barbs, a notch in the back. */
inline shapes::OutlineFn dart() {
  return [](SkSize s) {
    SkPathBuilder b;
    b.moveTo(s.width(), s.height() * 0.5f);
    b.lineTo(0, 0);
    b.lineTo(s.width() * 0.34f, s.height() * 0.5f);
    b.lineTo(0, s.height());
    b.close();
    return b.detach();
  };
}

inline Element artSitemap(sigil::weave::FontContext& f) {
  const float W = S(104), H = S(67);
  const SkPoint c{S(36), S(32)};
  // glowUnit, not radialUnit: on a 2:1 box radialUnit's radius is a
  // fraction of the HALF-DIAGONAL, so the whole band stack lands inside
  // t < 0.71 and the outer bands never appear. glowUnit's radius is the
  // shorter side, so t = 1 IS the ellipse edge and the bands sit where
  // they were authored.
  Element vortex = rect(c.fX - S(35), c.fY - S(17), S(70), S(34))
                       .shape(shapes::annulus(0.30f))
                       .fill(mskia::Paint::glowUnit({0.5f, 0.5f}, 1.0f,
                                                {{0.0f, C5(0xFFFF00)},
                                                 {0.34f, C5(0xFFEF00)},
                                                 {0.52f, C5(0xFFAD42)},
                                                 {0.68f, C5(0xFF5A00)},
                                                 {0.86f, C5(0xF70000)},
                                                 {1.0f, C5(0x8C0000)}}))
                       .rotate(-33);
  Element out = artBox(W, H).child(std::move(vortex));
  // four darts, outside the vortex on its two axes
  const float dw = S(17), dh = S(16);
  const float ang[4] = {122, -58, 210, 30};
  const float px[4] = {0.28f, -0.28f, 0.86f, -0.86f};
  const float py[4] = {-0.95f, 0.95f, 0.42f, -0.42f};
  for (int i = 0; i < 4; ++i)
    out.child(rect(c.fX + px[i] * S(34) - dw * 0.5f,
                   c.fY + py[i] * S(31) - dh * 0.5f, dw, dh)
                  .shape(dart())
                  .fill(Fill::color(C5(0xFFFF00)))
                  .rotate(ang[i]));
  out.child(navLabel(f, "SITE MAP", S(64), S(26), S(39), S(10)));
  return out;
}

// --- p-pressbox.gif, 131x56 — the one genuinely photographic asset, and
// --- the one deliberate approximation: a lozenge fuselage and swept fins.
/** A triangle in unit-box coordinates — the fin primitive.
 *  `shapes::polygon(3, deg)` is inscribed and equilateral, which is the
 *  wrong shape for a swept fin: it has one aspect ratio and one rotation,
 *  where a fin needs three independent vertices. */
inline shapes::OutlineFn tri(float ax, float ay, float bx, float by, float cx,
                             float cy) {
  return [=](SkSize s) {
    SkPathBuilder b;
    b.moveTo(ax * s.width(), ay * s.height());
    b.lineTo(bx * s.width(), by * s.height());
    b.lineTo(cx * s.width(), cy * s.height());
    b.close();
    return b.detach();
  };
}

inline Element artPressBox(sigil::weave::FontContext& f) {
  const float W = S(131), H = S(56);
  const SkColor4f hull = C5(0xFF0042), hullLo = C5(0xCE0031),
                  hullHi = C5(0xFF8CA5), grn = C5(0x319431),
                  grnLo = C5(0x101800), gold = C5(0xFFFF00);

  // The measured body axis runs from the tail at (4.5, 41) to the nose at
  // (125, 14): atan2(-27, 120) = -12.6 degrees.
  Element ship = rect(0, 0, W, H).rotate(-12.6f).transformOrigin(0.5f, 0.5f);
  // dorsal fin, swept back from mid-body
  ship.child(rect(S(38), S(6), S(52), S(20))
                 .shape(tri(1.0f, 1.0f, 0.86f, 0.0f, 0.0f, 1.0f))
                 .fill(mskia::Paint::linearUnit(
                     {0, 0}, {0, 1}, {{0.0f, C5(0xF71039)}, {1.0f, hullLo}})));
  // ventral fin
  ship.child(rect(S(58), S(36), S(40), S(15))
                 .shape(tri(0.0f, 0.0f, 1.0f, 0.0f, 0.62f, 1.0f))
                 .fill(Fill::color(C5(0xA50029))));
  // rear nacelle
  ship.child(rect(S(4), S(25), S(36), S(14))
                 .shape(shapes::squircle(2.6f))
                 .fill(mskia::Paint::linearUnit(
                     {0, 0}, {0, 1},
                     {{0.0f, C5(0x8CDE73)}, {0.42f, grn}, {1.0f, grnLo}})));
  // fuselage
  ship.child(rect(S(16), S(23), S(100), S(17))
                 .shape(shapes::squircle(2.2f))
                 .fill(mskia::Paint::linearUnit({0, 0}, {0, 1},
                                            {{0.0f, hullHi},
                                             {0.26f, hull},
                                             {0.68f, hullLo},
                                             {1.0f, C5(0x8C0021)}})));
  // dorsal ridge highlight
  ship.child(rect(S(28), S(25), S(72), S(3))
                 .shape(shapes::squircle(2.0f))
                 .fill(Fill::color(alpha(C5(0xFFC6D6), 0.85f))));
  // nose spike
  ship.child(rect(S(108), S(27), S(24), S(8))
                 .shape(shapes::arrow(0.28f, 0.90f))
                 .fill(Fill::color(hull)));
  // window strip
  for (int i = 0; i < 5; ++i)
    ship.child(rect(S(44 + i * 7.0f), S(29), S(4), S(4))
                   .corners({S(1)})
                   .fill(Fill::color(gold)));

  return artBox(W, H)
      .child(std::move(ship))
      .child(navLabel(f, "PRESS BOX SHUTTLE", S(50), S(38), S(80), S(10),
                      kLabelWhite));
}

// --- p-jamlogo.gif, 272x165 — the largest object on the page by a factor
// --- of four, and the one a viewer judges the sketch on.
inline Element artLogo(sigil::weave::FontContext& fonts) {
  const float W = S(272), H = S(165);
  // Swirl geometry measured off p-jamlogo_x2: centre ~(172, 62) in the
  // 272x165 box, spanning x 65..250 and y 5..145.
  const SkPoint c{S(176), S(54)};
  const float rx = S(92), ry = S(58);

  auto swirlFill = [] {
    // glowUnit again, for the reason artSitemap() gives: on this 1.4:1 box
    // radialUnit would put the whole rainbow inside t < 0.71 and the outer
    // band would never draw.
    return mskia::Paint::glowUnit({0.5f, 0.5f}, 1.0f,
                              {{0.0f, C5(0x101831)},
                               {0.44f, C5(0x21103A)},
                               {0.56f, C5(0xFFEF00)},
                               {0.70f, C5(0xFFAD42)},
                               {0.86f, C5(0xF70000)},
                               {1.0f, C5(0x7310C6)}});
  };
  auto swirl = [&] {
    return rect(c.fX - rx, c.fY - ry, rx * 2, ry * 2)
        .shape(shapes::annulus(0.44f))
        .fill(swirlFill())
        .rotate(-18);
  };

  // The letters. Chunky angular caps in the original, set MUCH narrower
  // than a system grotesque: measured "JAM" is 128 px wide at a 90 px cap
  // (0.47 per glyph), where Impact is 0.62. So size by cap height and
  // condense with scaleX — the same move the nav labels need, and the same
  // one a 1996 art director made by hand.
  //
  // textFill() maps the ramp onto the TEXT METRICS (cap top to baseline),
  // so the teal -> yellow-green horizon crosses the capitals at any size
  // with no hand-positioned gradient. That is the whole reason it exists —
  // but NOT with the Unit ramps. textFill already installs a local matrix
  // mapping [0,1]^2 onto the metric band, and linearUnit's own SkSL then
  // divides by uResolution (the NODE size) on top of it, so t collapses to
  // ~0 and every glyph comes out the first stop, flat. The pixel-space
  // linear() with unit-square endpoints is the spelling that works.
  auto letters = [&](const char* s, float capPx, float targetW, float x,
                     float capTopY, float lean) {
    const float size = capPx / 0.72f;
    Element t = text(U(s), ty(display(), size, C5(0x2FA9A0), 0));
    t.textFill(mskia::Paint::linear({0, 0}, {0, 1},
                                {{0.0f, C5(0x006BA5)},
                                 {0.22f, C5(0x007BAD)},
                                 {0.52f, C5(0x00A584)},
                                 {0.78f, C5(0x9CCE84)},
                                 {1.0f, C5(0xCEDE73)}}));
    const float r = S(2.2f);
    const float d[8][2] = {{-1, 0},  {1, 0},  {0, -1}, {0, 1},
                           {-1, -1}, {1, -1}, {-1, 1}, {1, 1}};
    for (auto& v : d) t.echo({v[0] * r, v[1] * r}, C5(0x101831));
    const SkSize m = measure(text(U(s), ty(display(), size, kLabel, 0)), fonts);
    const float sx = m.width() > 1 ? targetW / m.width() : 1.0f;
    return t.left(Dim(x))
        .top(Dim(capTopY - 0.20f * size))
        .width(Dim(m.width() + 4.0f))
        .scaleX(sx)
        .skewX(lean)
        .transformOrigin(0.0f, 0.5f);
  };

  return artBox(W, H)
      .child(swirl().zIndex(0))
      // "SPACE": measured x 17..132, cap band y 47..87
      .child(letters("SPACE", S(40), S(112), S(16), S(47), -7).zIndex(1))
      // "JAM": measured x 122..250, cap band y 25..115
      .child(letters("JAM", S(80), S(132), S(124), S(28), -8).zIndex(2))
      // The swirl passing IN FRONT of the bottom of the J — the single cue
      // that makes this read as a 1996 logotype instead of a gradient
      // wordmark. Four concentric arcs, not one stroke: a PathFormat's fill
      // is evaluated in node-local space, so it can vary ALONG a stroked
      // band but not ACROSS its width, which is the direction the rainbow
      // runs.
      .child([&] {
        Element band = rect(c.fX - rx, c.fY - ry, rx * 2, ry * 2).rotate(-18);
        // Same four bands, at the same four radii the annulus ramp puts
        // them at, so the ring READS as one ring that goes behind at the
        // top and comes round in front at the bottom left.
        const uint32_t ink[4] = {0x7310C6, 0xF70000, 0xFFAD42, 0xFFEF00};
        for (int i = 0; i < 4; ++i) {
          const float k = 0.960f - (float)i * 0.092f;
          band.child(rect(rx * (1 - k), ry * (1 - k), rx * 2 * k, ry * 2 * k)
                         .shape(shapes::arc(96, 90))
                         .stroke(stroke(S(9.4f), Fill::color(C5(ink[i])),
                                        PathFormat::Align::Center)));
        }
        return band.zIndex(3);
      }());
}

// --- the Fast Break row's two wordmarks, 50x11 each, bright red caps.
inline Element wordmark(sigil::weave::FontContext& fonts, const char* s,
                        float w, float h, bool rightAlign) {
  // fast.gif is right-aligned inside its 50x11 box, with the left ~40% of
  // the box empty; break.gif fills its own.
  const float track = 0.35f * kScale;
  const float target = (rightAlign ? 0.60f : 0.97f) * w;
  auto styleAt = [&](float sz) {
    return ty(display(), sz, C5(0xFF0000), track);
  };
  float size = h * 1.16f;
  SkSize m = measure(text(U(s), styleAt(size)), fonts);
  float sx = 1.0f;
  if (m.width() > target && m.width() > 1) sx = target / m.width();
  Element t = text(U(s), styleAt(size));
  t.echo({kScale, kScale}, C5(0x8C0000));
  t.left(Dim(rightAlign ? w - target : 0))
      .top(Dim(-h * 0.22f))
      .width(Dim(m.width() + 4.0f));
  if (sx < 0.999f) t.scaleX(sx).transformOrigin(0.0f, 0.5f);
  return stack().width(Dim(w)).height(Dim(h)).clip(true).child(std::move(t));
}

// ---------------------------------------------------------------------------
// The load — 28.8 kbps over four connections. Byte counts are the files'
// own, as served.

struct Asset {
  const char* name;
  int bytes;
};
inline const std::vector<Asset>& manifest() {
  static const std::vector<Asset> k = {
      {"bg_stars", 8452},    {"fast", 189},        {"fastbreak", 6756},
      {"break", 229},        {"pressbox", 4422},   {"jamcentral", 1908},
      {"bball", 1368},       {"lunartunes", 3538}, {"lineup", 1929},
      {"jamlogo", 15410},    {"jump", 2593},       {"junior", 1253},
      {"studiostore", 2745}, {"souvenirs", 3594},  {"sitemap", 3401},
      {"behind", 1902}};
  return k;
}
enum Ix {
  kStars = 0,
  kFast,
  kFastbreak,
  kBreak,
  kPressbox,
  kJamcentral,
  kBball,
  kLunartunes,
  kLineup,
  kJamlogo,
  kJump,
  kJunior,
  kStudiostore,
  kSouvenirs,
  kSitemap,
  kBehind,
  kAssetCount
};

constexpr double kBandwidth = 3000.0;  // bytes/sec, effective on a 28.8k line
constexpr int kSlots = 4;              // Netscape's HTTP/1.0 default
constexpr double kSpeedup = 2.5;       // sketch-time compression
constexpr double kReloadAt = 11.5;     // length of one cycle in sketch
                                       // seconds: the finished page holds for
                                       // the balance of it, then every byte
                                       // counter is reset and the load replays
                                       // from empty

// ---------------------------------------------------------------------------
// The table — the page's <TABLE>, cell by cell.

/** One <TD> in document order: which asset fills it, how many <br> stand
 *  above the image inside it, the cells it claims and the alignment the
 *  HTML gives it.
 *
 *  The children are BUILT from this list and each one carries its own claim
 *  through `Element::cells`, so there is nothing running parallel to them
 *  that an inserted row could knock out of step. */
struct Slot {
  int asset = -1;  ///< -1 for the two cells the page leaves empty
  int brs = 0;
  int col = 0, row = 0, colspan = 1, rowspan = 1;
  Align across = Align::Center;
  Align down = Align::Start;
};

/** The occupancy straight out of the HTML: <TABLE WIDTH=500 CELLSPACING=2
 *  CELLPADDING=1>, five columns by five rows. */
constexpr Slot kSlotTable[] = {
    // <TD colspan=5 align=right valign=top>, empty
    {-1, 0, 0, 0, 5, 1, Align::End, Align::Start},
    {kPressbox, 3, 0, 1, 2, 1, Align::End, Align::Center},
    {kJamcentral, 0, 2, 1, 1, 1, Align::Center, Align::Center},
    {kBball, 0, 3, 1, 1, 1, Align::Center, Align::Start},
    {kLunartunes, 2, 4, 1, 1, 1, Align::Center, Align::End},
    // align=middle on the image reads as centre
    {kLineup, 2, 0, 2, 1, 1, Align::Center, Align::Start},
    {kJamlogo, 0, 1, 2, 3, 2, Align::End, Align::Center},
    {kJump, 0, 4, 2, 1, 1, Align::End, Align::End},
    {kJunior, 0, 0, 3, 1, 1, Align::Center, Align::End},
    {kStudiostore, 2, 4, 3, 1, 2, Align::Center, Align::Start},
    {-1, 0, 0, 4, 1, 1, Align::Center, Align::Start},
    {kSouvenirs, 0, 1, 4, 1, 1, Align::Center, Align::Start},
    {kSitemap, 4, 2, 4, 1, 1, Align::Center, Align::End},
    {kBehind, 0, 3, 4, 1, 1, Align::Center, Align::Center},
};

// ---------------------------------------------------------------------------
// The display quantisation — the 216-colour web cube, in setView().
//
// THE SNAP CARRIES NO SCREEN, and that is a finding rather than a saving.
// The page's palette is the 216-colour cube and the view transform rounds
// to it; an ORDERED dither would lay a regular 4x4 lattice across every
// disc, and the shipped GIFs carry no lattice at all — p-souvenirs.gif is
// a smooth cyan radial with a white core, p-lunartunes.gif flat blue with
// a hard pink ring. A 1996 encoder that dithered at all dithered by error
// diffusion, which is scattered; a lattice is the one thing the reference
// definitely does not have. So the shader ROUNDS: each component to the
// nearest sixth, with no position, no cell and no time in it.
//
// One expression and no helper function, because a shader authored in a
// sketch and compiled by the host's Skia cannot carry user-defined ones.
// Unpremultiply, round, re-premultiply — rounding premultiplied colour
// would quantise each channel against a different scale.

inline sk_sp<SkRuntimeEffect> viewEffect() {
  static sk_sp<SkRuntimeEffect> fx = [] {
    static constexpr char kSrc[] = R"(
uniform shader content;

half4 main(float2 pos) {
  half4 src = content.eval(pos);
  float  al = max(float(src.a), 1e-4);
  float3 v  = float3(src.rgb) / al;

  float3 q = clamp(floor(v * 5.0 + 0.5), 0.0, 5.0) / 5.0;
  return half4(half3(q * al), src.a);
}
)";
    auto [effect, error] = SkRuntimeEffect::MakeForShader(SkString(kSrc));
    if (!effect) SkDebugf("spacejam view: %s\n", error.c_str());
    return effect;
  }();
  return fx;
}

}  // namespace sj

// ---------------------------------------------------------------------------

struct SpaceJam1996 : sketch::Sketch {
  using Ix = sj::Ix;

  // The load simulator's state: one Output per asset, in [0,1].
  ch::Output<float> got[sj::kAssetCount];
  double gotBytes[sj::kAssetCount] = {};
  double sinceDone = 0.0;
  uint32_t arrivedMask = 0;
  bool needRender = true;

  // Everything the browser would have cached as a decoded GIF: each nav
  // image baked ONCE from its element tree via snapshot(), then replayed
  // under a hard scanline clip. Baking is what makes the reveal affordable —
  // sixteen assets, each redrawn on every frame it is still arriving — and
  // the clip is hand-written because a picture has no directional-wipe
  // control of its own.
  sk_sp<SkPicture> pic[sj::kAssetCount];
  float artW[sj::kAssetCount] = {};
  float artH[sj::kAssetCount] = {};

  Pattern stars;
  mskia::Paint starsMat;
  // <TABLE WIDTH=500 CELLSPACING=2 CELLPADDING=1>, at this sketch's scale.
  // The columns and rows are the ones the children claim.
  layouts::Table table{.columns = 5,
                       .rows = 5,
                       .width = sj::S(500),
                       .spacing = sj::S(2),
                       .padding = sj::S(1)};

  // ---- the reveal --------------------------------------------------------
  Element revealed(int i, bool inFlight) const {
    const sk_sp<SkPicture> p = pic[i];
    const float h = artH[i];
    const ch::Output<float>* g = &got[i];
    Element e = custom([p, h, g](SkCanvas& canvas, const PaintContext& ctx) {
                  const float frac = g->value();
                  if (frac <= 0.0f || !p) return;
                  // No interlacing on any of the sixteen (every image
                  // descriptor's flag is zero), so this is the older, simpler
                  // behaviour: a complete top band and nothing below, the
                  // edge hard and landing on a 1996 scanline.
                  const float rows =
                      std::floor(h / sj::kScale * frac) * sj::kScale;
                  if (rows <= 0.0f) return;
                  canvas.save();
                  canvas.clipRect(SkRect::MakeWH(ctx.size.width(), rows));
                  canvas.drawPicture(p.get());
                  canvas.restore();
                })
                    .width(Dim(artW[i]))
                    .height(Dim(artH[i]));
    if (inFlight) e.cache(Cache::None);
    return e;
  }

  /** A table cell: the `<br>` blocks as an 18 px line box each, then the
   *  image. Its MEASURED size is what the table algorithm reads, so the
   *  br-count reaches the layout the same way it does in a browser. */
  /** One <TD>: the <br> block above the image inside the cell, the image
   *  itself, and the claim on the grid — said on the child, so a cell and
   *  its occupancy cannot drift apart. */
  Element cell(const sj::Slot& s) const {
    const bool inFlight =
        s.asset >= 0 && (arrivedMask & (1u << (unsigned)s.asset)) == 0;
    Element c = box().column().alignSelf(Align::Start).shrink(0);
    if (s.asset < 0) c.width(Dim(0)).height(Dim(0));
    if (s.brs > 0)
      c.child(box().width(Dim(0)).height(Dim(sj::S(18) * (float)s.brs)));
    if (s.asset >= 0) c.child(revealed(s.asset, inFlight));
    c.cells(s.col, s.row, s.colspan, s.rowspan).cellAlign(s.across, s.down);
    return c;
  }

  // ---- the page ----------------------------------------------------------
  Element describe(sketch::SketchContext& ctx) {
    using namespace sj;

    // 1. the starfield, genuinely full bleed: the body background paints the
    //    whole viewport including under the 8 px margin, so stars run to all
    //    four edges. Black until the tile's LAST byte lands, then the whole
    //    lattice at once — a tiled background cannot wipe (each repeat would
    //    reveal its own band and comb the page every 111 px), so a hard cut
    //    is the defensible model.
    //
    //    Spelled with bind()'s affine chain rather than a second Output:
    //    v*1000 - 999, clamped, is zero for every byte but the last. The
    //    step is the shaping, and it lives at the property instead of in
    //    the tick loop.
    Element field = box()
                        .inset(0)
                        .fill(starsMat)
                        .opacity(motion::bind(&got[kStars])
                                     .scale(1000.0f)
                                     .offset(-999.0f)
                                     .clamp(0.0f, 1.0f))
                        .key("starfield");

    // 2. the Fast Break row — the only left-aligned thing on the page, and
    //    the only thing that moves. Restored from the HTML comment the live
    //    page keeps it in.
    const bool ballIn = (arrivedMask & (1u << kFastbreak)) != 0;
    Element fastRow =
        stack()
            .left(Dim(S(70)))
            .top(Dim(S(86)))
            .width(Dim(S(500)))
            .height(Dim(S(46)))
            .child(revealed(kFast, (arrivedMask & (1u << kFast)) == 0)
                       .left(Dim(S(3)))
                       .top(Dim(S(17.5f))))
            .child(revealed(kBreak, (arrivedMask & (1u << kBreak)) == 0)
                       .left(Dim(S(93)))
                       .top(Dim(S(17.5f))));
    if (ballIn) {
      // Fully arrived: the live element, whose material steps its uTime at
      // 10 Hz — the GIF's own frame rate, six frames, forever.
      fastRow.child(rect(S(53), S(3), S(40), S(40))
                        .shape(shapes::circle())
                        .fill(ballMaterial(true, C5(0xFF6B29), C5(0xC64210),
                                           C5(0x521800), 0.050f))
                        .key("fastbreak"));
    } else {
      // Still arriving: a partially-downloaded animated GIF shows its first
      // frame and does not animate. Same picture path as everything else.
      fastRow.child(revealed(kFastbreak, true).left(Dim(S(53))).top(Dim(S(3))));
    }

    // 3. the planet table. Nothing below is hand-placed: `layouts::Table`
    //    runs the auto-layout rule over the children's measured sizes and
    //    the cells they claim.
    Element grid = layout(table)
                       .left(Dim(S(70)))
                       .top(Dim(S(168)))
                       .width(Dim(S(500)))
                       .height(Dim(S(435)))
                       .key("table");
    for (const Slot& slot : kSlotTable) grid.child(cell(slot));

    // 4. the © line — the ONLY live text on the page. <font size="-1"> is
    //    HTML size 2 of 7 -> 13.33 px computed, hard-wrapped by the author's
    //    own <br> into two centred lines.
    auto small = ty(serif(), S(13.33f), kBodyText);
    Element colophon =
        box()
            .left(Dim(0))
            .top(Dim(S(757)))
            .width(Dim(S(640)))
            .column()
            .alignItems(Align::Center)
            .child(
                text(U("SPACE JAM, characters, names, and all related"), small))
            .child(
                text(U("indicia are trademarks of Warner Bros. \xc2\xa9 1996"),
                     small));

    (void)ctx;
    return stack()
        .child(std::move(field))
        // the ad-slot table: 488x60 of server-side includes that no longer
        // resolve. Left empty on purpose — 60 px of stars, and the reason
        // the page has a bald strip at the top.
        .child(std::move(fastRow))
        .child(std::move(grid))
        .child(std::move(colophon));
  }

  // ---- setup -------------------------------------------------------------
  void bakeArt(sketch::SketchContext& ctx) {
    using namespace sj;
    sigil::weave::FontContext& f = *ctx.fonts;
    struct Job {
      int ix;
      Element tree;
      float w, h;
    };
    std::vector<Job> jobs;
    jobs.push_back(
        {kFast, wordmark(f, "FAST", S(50), S(11), true), S(50), S(11)});
    jobs.push_back(
        {kBreak, wordmark(f, "BREAK", S(50), S(11), false), S(50), S(11)});
    jobs.push_back({kFastbreak,
                    rect(0, 0, S(40), S(40))
                        .left(Dim(0))
                        .top(Dim(0))
                        .shape(shapes::circle())
                        .fill(ballMaterial(false, C5(0xFF6B29), C5(0xC64210),
                                           C5(0x521800), 0.050f)),
                    S(40), S(40)});
    jobs.push_back({kPressbox, artPressBox(f), S(131), S(56)});
    jobs.push_back({kJamcentral, artJamCentral(f), S(55), S(67)});
    jobs.push_back({kBball, artBball(f), S(62), S(62)});
    jobs.push_back({kLunartunes, artLunarTunes(f), S(95), S(77)});
    jobs.push_back({kLineup, artLineup(f), S(63), S(52)});
    jobs.push_back({kJamlogo, artLogo(f), S(272), S(165)});
    jobs.push_back({kJump, artJump(f), S(58), S(52)});
    jobs.push_back({kJunior, artJunior(f), S(49), S(57)});
    jobs.push_back({kStudiostore, artStudioStore(f), S(94), S(72)});
    jobs.push_back({kSouvenirs, artSouvenirs(f), S(83), S(83)});
    jobs.push_back({kSitemap, artSitemap(f), S(104), S(67)});
    jobs.push_back({kBehind, artBehind(f), S(67), S(63)});

    for (Job& j : jobs) {
      artW[j.ix] = j.w;
      artH[j.ix] = j.h;
      pic[j.ix] =
          snapshot(box().width(Dim(j.w)).height(Dim(j.h)).clip(true).child(
                       std::move(j.tree)),
                   f, {j.w, j.h});
    }
    artW[kStars] = artH[kStars] = 0;
  }

  /** Print the grid the table resolved next to the browser's, once at
   *  startup, so the layout is checkable against the reference render
   *  instead of taken on trust. The literals are what headless Chrome
   *  reports for the same page.
   *
   *  The input is built from the same `kSlotTable` the children are, so
   *  what is printed is the layout that was drawn. */
  void reportGrid() const {
    using namespace sj;
    LayoutInput in;
    in.container = {S(500), S(435)};
    for (const Slot& s : kSlotTable) {
      in.childSizes.push_back(
          s.asset < 0 ? SkSize{0, 0}
                      : SkSize{artW[s.asset],
                               artH[s.asset] + S(18) * (float)s.brs});
      in.childCells.push_back({.column = s.col,
                               .row = s.row,
                               .columns = s.colspan,
                               .rows = s.rowspan,
                               .across = s.across,
                               .down = s.down,
                               .declared = true});
    }
    const layouts::Table::Grid grid = table.solve(in);
    SkDebugf("[spacejam] resolved columns (content px, 1x):");
    for (float w : grid.columnWidths) SkDebugf(" %.2f", w / kScale);
    SkDebugf(
        "\n[spacejam]  chrome measured:  71.42 97.70 122.33 78.95 107.59\n");
    SkDebugf("[spacejam] resolved rows (px, 1x):");
    for (float h : grid.rowHeights) SkDebugf(" %.2f", h / kScale);
    SkDebugf("\n[spacejam]  chrome measured:  0 113 88 73 139\n");

    // ...and the twelve images, which is what actually has to land. The
    // table origin is (70, 168) on the page; each row prints the
    // scheme-placed rect against the headless-Chrome
    // getBoundingClientRect() probe for the same image.
    const float refX[14] = {0,      115.13f, 283.78f, 384.92f, 465.70f,
                            77.20f, 183.41f, 509.00f, 84.20f,  466.20f,
                            0,      155.77f, 259.28f, 382.42f};
    const float refY[14] = {0,       230.50f, 198.00f, 175.00f, 211.00f,
                            328.00f, 292.00f, 328.00f, 400.00f, 420.00f,
                            0,       461.00f, 533.00f, 499.00f};
    const std::vector<SkRect> rects = table.place(in);
    float worst = 0;
    SkDebugf("[spacejam] image rects vs Chrome (page px):\n");
    for (size_t i = 0; i < std::size(kSlotTable); ++i) {
      const Slot& s = kSlotTable[i];
      if (s.asset < 0) continue;
      const float px = 70.0f + rects[i].left() / kScale;
      // the <br> block sits above the image inside the cell
      const float py = 168.0f + rects[i].top() / kScale + 18.0f * (float)s.brs;
      const float dx = px - refX[i], dy = py - refY[i];
      worst = std::max({worst, std::abs(dx), std::abs(dy)});
      SkDebugf("  %-14s %8.2f %8.2f   d %+.2f %+.2f\n",
               manifest()[(size_t)s.asset].name, px, py, dx, dy);
    }
    SkDebugf("[spacejam] worst deviation from the browser: %.2f px\n", worst);
  }

  void setup(sketch::SketchContext& ctx) override {
    using namespace sj;
    ctx.canvas(S(640), S(800));
    ctx.background(kPageBlack);  // <body bgcolor="#000000">, literally
    // The still is taken mid-hold: the load finishes around 7.96 s of sketch
    // time and the reload wipes the page around 11.46 s, so 9.5 s is the one
    // window where every asset is present. Anything earlier catches the page
    // mid-load and misses the logotype, which is dead last in the byte
    // schedule — and the reference this study is diffed against is the
    // FINISHED page.
    ctx.captureAt(9.5);

    bakeArt(ctx);
    reportGrid();

    stars = Pattern::tile({S(111), S(111)}, starTile());
    starsMat = stars.material(*ctx.fonts);

    // The 216-colour round, over the finished frame. It is a property of
    // the SCREEN, not of the artwork — which is exactly why it lives here
    // and the RGB555 snap lives in the materials.
    ctx.composer.setView(mskia::Effect::shader(viewEffect()));

    for (int i = 0; i < kAssetCount; ++i) {
      gotBytes[i] = 0;
      got[i] = 0.0f;
    }
    arrivedMask = 0;
    sinceDone = 0.0;

    // The transport. Fixed 120 Hz so the schedule is identical whatever the
    // host draws at (and whatever --fps a capture pre-rolls with).
    ctx.ticker.addFixed(
        120.0,
        [this] {
          stepLoad(1.0 / 120.0);
          return true;
        },
        16);

    ctx.composer.render(describe(ctx));
    needRender = false;
  }

  void stepLoad(double dt) {
    using namespace sj;
    const auto& m = manifest();
    dt *= kSpeedup;

    uint32_t done = 0;
    int active[kSlots];
    int nActive = 0;
    for (int i = 0; i < kAssetCount; ++i) {
      if (gotBytes[i] >= (double)m[(size_t)i].bytes) {
        done |= 1u << (unsigned)i;
        continue;
      }
      if (nActive < kSlots) active[nActive++] = i;
    }
    if (nActive == 0) {
      sinceDone += dt / kSpeedup;  // back to sketch seconds
      // Idle time since the last byte. The subtrahend is the load's own
      // duration in sketch seconds, so a cycle — load plus hold — comes to
      // kReloadAt. Every counter goes back to zero, so the whole page
      // re-downloads: a cold reload, deliberately — a 1996 Reload
      // re-requested every asset, so everything blanks and re-arrives,
      // the spinning ball included. Do not add a warm cache here.
      if (sinceDone > kReloadAt - 8.0) {
        for (int i = 0; i < kAssetCount; ++i) {
          gotBytes[i] = 0;
          got[i] = 0.0f;
        }
        sinceDone = 0.0;
      }
    } else {
      const double share = kBandwidth * dt / (double)nActive;
      for (int k = 0; k < nActive; ++k) {
        const int i = active[k];
        gotBytes[i] = std::min((double)m[(size_t)i].bytes, gotBytes[i] + share);
        got[i] = (float)(gotBytes[i] / (double)m[(size_t)i].bytes);
      }
    }
    if (done != arrivedMask) {
      arrivedMask = done;
      needRender = true;
    }
  }

  int reported = 0;
  void update(double elapsed, sketch::SketchContext& ctx) override {
    if (reported < 4 && elapsed > 8.5) {
      ++reported;
      const Composer::Stats& st = ctx.composer.stats();
      SkDebugf(
          "[spacejam] settled: instances %zu  pictures %zu  recorded %zu  "
          "painted-live %zu  layout %.2fms  paint %.2fms\n",
          st.instances, st.picturesLive, st.picturesRecorded, st.nodesPainted,
          st.layoutMs, st.paintMs);
    }
    if (!needRender) return;
    needRender = false;
    ctx.composer.render(describe(ctx));
  }
};

SIGIL_SKETCH(
    SpaceJam1996, "Study \xc2\xb7 Screens",
    "spacejam.com, still live \xe2\x80\x94 the page set by layouts::Table, "
    "each <TD> naming its own cells")
