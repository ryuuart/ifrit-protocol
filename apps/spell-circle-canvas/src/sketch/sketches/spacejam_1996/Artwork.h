#pragma once

#include "Drawing.h"

namespace sj {

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
  d.child(box().inset(0).fill(
      mskia::Paint::glowUnit({0.34f, 0.28f}, 1.35f,
                             {{0.0f, mskia::withAlpha(hi, 0.42f)},
                              {0.34f, mskia::withAlpha(hi, 0.10f)},
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
                 .fill(Fill::color(mskia::withAlpha(C5(0xFFC6D6), 0.85f))));
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
    const SkSize m =
        intrinsicSize(text(U(s), ty(display(), size, kLabel, 0)), fonts);
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
  SkSize m = intrinsicSize(text(U(s), styleAt(size)), fonts);
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
/** THE SIXTEEN REQUESTS AND THEIR BYTE COUNTS, as a constant for the same
 *  reason the star field above is one. */
inline constexpr auto kManifest = std::to_array<Asset>({{"bg_stars", 8452},
                                                        {"fast", 189},
                                                        {"fastbreak", 6756},
                                                        {"break", 229},
                                                        {"pressbox", 4422},
                                                        {"jamcentral", 1908},
                                                        {"bball", 1368},
                                                        {"lunartunes", 3538},
                                                        {"lineup", 1929},
                                                        {"jamlogo", 15410},
                                                        {"jump", 2593},
                                                        {"junior", 1253},
                                                        {"studiostore", 2745},
                                                        {"souvenirs", 3594},
                                                        {"sitemap", 3401},
                                                        {"behind", 1902}});
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
}

}  // namespace sj

// ---------------------------------------------------------------------------
