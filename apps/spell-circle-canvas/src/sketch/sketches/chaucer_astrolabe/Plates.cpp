#include "ChaucerAstrolabe.h"

auto ChaucerAstrolabe::sheet() -> matkit::LattenParameters {
  return {.shadow = mat::skia::toColor(kBrassP10),
          .body = mat::skia::toColor(kBrassP50),
          .light = mat::skia::toColor(kBrassP90),
          .from = {kCx - kMaterR * 0.95f, kCy + kMaterR * 0.95f},
          .to = {kCx + kMaterR * 0.85f, kCy - kMaterR * 1.05f},
          .level = 0.0f,
          // EVEN LATTEN. The object is a flat sheet of yellow latten under
          // a museum's own even light: the swing corner to corner is a
          // SHEEN, not a key. A larger one makes the mater's centre the
          // darkest region of the picture — which is exactly where all
          // seventy-three drawn circles live, so the material would win
          // over the geometry the plate exists to show.
          .sheen = 0.10f,
          // The tooling. Brass is worked, and the mark of the tool is
          // fine concentric turning; at this scale it reads as a tooth.
          .tooth = 0.07f,
          .toothScale = 0.55f,
          .patina = 0.0f};
}

auto ChaucerAstrolabe::brass(float level) -> Paint {
  const uint32_t bucket = (uint32_t)std::lround(std::clamp(level, 0.0f, 1.0f) *
                                                (float)(kLevels - 1));
  return Paint::recipe(latten.get(matkit::lattenRecipe(), sheet(), bucket,
                                  [](uint32_t b) {
                                    matkit::LattenParameters p = sheet();
                                    p.level = (float)b / (float)(kLevels - 1);
                                    return mat::Material(matkit::lattenRecipe(),
                                                         p);
                                  }))
      .worldSpace();
}

auto ChaucerAstrolabe::brassStroke(SkRect r, float level) const -> Fill {
  auto p = [&](SkPoint q) { return SkPoint{q.fX - r.left(), q.fY - r.top()}; };
  matkit::LattenParameters face = sheet();
  face.level = level;
  return linearGradient(p({kCx - kMaterR * 0.95f, kCy + kMaterR * 0.95f}),
                        p({kCx + kMaterR * 0.85f, kCy - kMaterR * 1.05f}),
                        {mat::skia::toSkColor(matkit::lattenTone(face, 0.0f)),
                         mat::skia::toSkColor(matkit::lattenTone(face, 0.5f)),
                         mat::skia::toSkColor(matkit::lattenTone(face, 1.0f))},
                        {0.0f, 0.5f, 1.0f});
}

auto ChaucerAstrolabe::plate() -> Element {
  auto g = box()
               .rect(SkRect::MakeXYWH(kCx - kR, kCy - kR, 2 * kR, 2 * kR))
               .key("plate")
               .shape(shapes::circle())
               .clip(true)
               // The plate is lifted off the bottom of the ramp: a
               // recessed disc drawn at a quarter of the range put the
               // engraved circle family on the darkest brass on the
               // object, where an engraved hairline cannot survive.
               .fill(brass(0.44f));

  // Wear: brass handled for 700 years is bright on the high edges and dark
  // in the cuts, and an unpolished latten greens in its recesses first. A
  // very low-amplitude speckle, only over the engraved field.
  // The speckle is SEEDED, so it is the same field every run and every
  // frame: baked over the plate disc, and the growth is the blit's alpha.
  g.child(
      box()
          .inset(0)
          .key("verdigris")
          .shape(shapes::circle())
          .cache(Cache::Texture)
          .fill(verdigris.material())
          .opacity(animate(from(0.0f).to(1.0f), ramp(tTropics * 1000, 900))));

  // the recess: the plate sits one millimetre below the limb. A filter
  // over the whole disc with nothing behind it that changes — baked, on
  // the same rule as the mater's four passes.
  g.child(box()
              .inset(0)
              .key("recess")
              .shape(shapes::circle())
              .cache(Cache::Texture)
              .fill(Fill::none())
              .foreground(
                  styles::InnerShadow{hexColor(0x2a1d08, 0.55f), {0, 3}, 9}));

  // --- the twilight arc, crepusculum, h = -18 ---------------------------
  {
    PathFormat dotted{.width = 1.5f,
                      .strokeFill = Fill::color(hexColor(0x3a2a10, 0.42f)),
                      .dashIntervals = {3.0f, 5.0f}};
    g.child(kit::disc(PL(0, almCy(-18.0f)), almR(-18.0f) * kR)
                .key("twilight")
                .shape(shapes::circle())
                .fill(Fill::none())
                .stroke(dotted)
                .opacity(animate(from(0.0f).to(1.0f),
                                 ramp(tHorizon * 1000 + 700, 700))));
  }

  // --- 45 almucantars, "compowned by two and two" (I.18) ----------------
  // They grow out of the zenith one every 38 ms. This is the sketch's most
  // beautiful three seconds and it is not rushed.
  for (int i = 1; i <= 44; ++i) {
    const float h = (float)(i * 2);
    const bool five = (i % 5) == 0;
    // The almucantar family is the plate's whole subject: on the object
    // they are ~30 crisp lines filling the upper half. Cut at less than
    // half opacity they are a smooth disc with two or three faint rings
    // on it, whatever the brass is doing.
    g.child(cut({0, almCy(h)}, almR(h), five ? 1.9f : 1.5f,
                five ? 0.86f : 0.70f, five ? 0.46f : 0.34f,
                "alm" + std::to_string(i))
                .opacity(animate(from(0.0f).to(1.0f),
                                 ramp(tAlmu * 1000 + (float)(44 - i) * 38.0f,
                                      520, ch::easeOutQuint))));
  }

  // --- 12 azimuth curves, clipped to the visible sky --------------------
  // Every azimuth passes through BOTH the zenith and the nadir — a coaxal
  // family through two fixed points. They are clipped to inside the horizon
  // AND inside the Capricorn disc, which is an intersection, so a nested
  // clip() is exactly right. (The seasonal hours below are the case where
  // it is not.)
  {
    const SkPoint hc = PL(0, almCy(0.0f));
    const float hr = almR(0.0f) * kR;
    auto sky =
        box()
            .rect(SkRect::MakeXYWH(hc.fX - hr, hc.fY - hr, 2 * hr, 2 * hr))
            .key("sky")
            .shape(shapes::circle())
            .clip(true);
    auto local = [&](float mx, float my) {
      const SkPoint p = PL(mx, my);
      return SkPoint{p.fX - (hc.fX - hr), p.fY - (hc.fY - hr)};
    };
    for (int i = 1; i <= 5; ++i) {
      const float ap = (float)(i * 15);  // A' from the PRIME VERTICAL
      const float delay = tAzim * 1000 + (float)i * 150.0f;
      for (int s = -1; s <= 1; s += 2) {
        const path::PlaneCircle az = azimuth((float)s * ap);
        const float rad = az.radius * kR;
        sky.child(kit::disc(local(az.centre.x, az.centre.y), rad)
                      .key("az" + std::to_string(i * s))
                      .shape(shapes::circle())
                      .fill(Fill::none())
                      .stroke(groove(rad, 1.3f, 0.42f, 0.20f))
                      .opacity(animate(from(0.0f).to(1.0f), ramp(delay, 460))));
      }
    }
    // the prime vertical (A = 90/270): a circle centred on the axis, and it
    // passes through the east and west points by the identity
    // tan(45−φ/2)·tan(45+φ/2) = 1
    {
      const path::PlaneCircle pv = azimuth(0.0f);
      const float rad = pv.radius * kR;
      sky.child(
          kit::disc(local(pv.centre.x, pv.centre.y), rad)
              .key("azPV")
              .shape(shapes::circle())
              .fill(Fill::none())
              .stroke(groove(rad, 1.7f, 0.55f, 0.26f))
              .opacity(animate(from(0.0f).to(1.0f), ramp(tAzim * 1000, 460))));
    }
    g.child(std::move(sky));
  }

  // --- the meridian: STRAIGHT, and that is the projection showing -------
  // A = 0/180 is the one vertical circle through BOTH celestial poles,
  // including the south pole, which is the eye of the projection — and any
  // circle through the eye projects to a line.
  g.child(box()
              .rect(SkRect::MakeXYWH(kR - 1.0f, 0, 2.0f, 2 * kR))
              .key("meridian")
              .fill(Fill::color(hexColor(0x3a2a10, 0.55f)))
              // BY EDGE, not by spans, and this is the one port in the
              // corpus that MOVES PIXELS on purpose. The bar is a filled
              // 2 × 2R rect with no stroke at all; an arc-length window
              // walks its PERIMETER, so the first half of the ramp crawled
              // up a 2px-wide left edge enclosing no area — a 260 ms dead
              // beat and then a snap. A meridian draws DOWNWARD.
              .mask(by::edge(90.0f, animate(from(0.0f).to(1.0f),
                                            ramp(tAzim * 1000 + 820, 520)))));

  // --- 12 unequal-hour lines, "twelve devisiouns embelif" (I.20) --------
  // THE REGION IS A SET DIFFERENCE: these live only inside Capricorn AND
  // OUTSIDE the horizon. clip() intersects and nesting intersects more;
  // there is no clipOut() and no shapes::subtract, so the region is built
  // with a raw SkPathOp below the Compose seam.
  {
    const SkPoint hc = PL(0, almCy(0.0f));
    const float hr = almR(0.0f) * kR;
    SkPathBuilder cb, hb;
    cb.addOval(SkRect::MakeWH(2 * kR, 2 * kR));
    hb.addOval(
        SkRect::MakeLTRB(hc.fX - hr, hc.fY - hr, hc.fX + hr, hc.fY + hr));
    const SkPath capDisc = cb.detach(), horDisc = hb.detach();
    SkPath region;
    Op(capDisc, horDisc, kDifference_SkPathOp, &region);
    auto night = box().inset(0).key("night").shape(heldPath(region)).clip(true);
    for (int k = 1; k <= 11; ++k) {
      const std::optional<path::PlaneCircle> c = seasonalLine(k);
      const float delay = tHours * 1000 + (float)std::abs(k - 6) * 105.0f;
      if (!c)  // k = 6 is straight: midnight is midnight at every dec
        continue;
      const float rad = c->radius * kR;
      night.child(kit::disc(PL(c->centre.x, c->centre.y), rad)
                      .key("hr" + std::to_string(k))
                      .shape(shapes::circle())
                      .fill(Fill::none())
                      .stroke(groove(rad, 1.5f, 0.50f, 0.24f))
                      .opacity(animate(from(0.0f).to(1.0f), ramp(delay, 480))));
    }
    // k = 6, the straight one
    night.child(
        box()
            .rect(SkRect::MakeXYWH(kR - 0.8f, kR, 1.6f, kR))
            .key("hr6")
            .fill(Fill::color(hexColor(0x3a2a10, 0.5f)))
            .opacity(animate(from(0.0f).to(1.0f), ramp(tHours * 1000, 480))));
    g.child(std::move(night));
  }

  // --- the horizon, h = 0: heavier than its 44 siblings ------------------
  g.child(cut({0, almCy(0.0f)}, almR(0.0f), 3.4f, 0.90f, 0.50f, "horizon")
              .mask(by::spans(spans::upTo(
                  animate(from(0.0f).to(1.0f),
                          ramp(tHorizon * 1000, 900, ch::easeOutQuint))))));

  // --- the three tropics: Capricorn : equator : Cancer = 1 : k : k² -----
  const float trop[3] = {1.0f, kReq, kRcan};
  for (int i = 0; i < 3; ++i)
    g.child(cut({0, 0}, trop[i], i == 0 ? 3.0f : 2.4f, 0.80f, 0.46f,
                "trop" + std::to_string(i))
                .mask(by::spans(spans::upTo(animate(
                    from(0.0f).to(1.0f), ramp(tTropics * 1000 + (float)i * 200,
                                              760, ch::easeOutQuint))))));

  // --- the east and west points, where the horizon meets the equator ----
  // Exact invariant: √(R_eq² + R_eq²/tan²φ) = R_eq/sin φ, to the last bit
  // of a double. The cheapest real check on the whole instrument.
  for (int s = -1; s <= 1; s += 2)
    g.child(kit::disc(PL(s * kReq, 0), 5.0f)
                .key(std::string("ew") + (s < 0 ? "E" : "W"))
                .shape(shapes::circle())
                .fill(Fill::color(kTrace))
                .opacity(bind(&trace).scale(0.9f).clamp(0, 1)));

  // --- the zenith ------------------------------------------------------
  g.child(kit::disc(PL(0, kYzen), 3.6f)
              .key("zenith")
              .shape(shapes::circle())
              .fill(Fill::color(hexColor(0x3a2a10, 0.85f)))
              .opacity(animate(from(0.0f).to(1.0f), ramp(tAlmu * 1000, 400))));

  return g;
}

auto ChaucerAstrolabe::construction() -> Element {
  auto g = box()
               .rect(SkRect::MakeXYWH(kCx - kR, kCy - kR, 2 * kR, 2 * kR))
               .key("trace")
               .shape(shapes::circle())
               .clip(true)
               .opacity(&trace);

  const float lam = 1.0f, h = 25.5f;
  const float dec = sunDec(lam);
  const float rs = rOfDec(dec);
  const float cy = almCy(h), ra = almR(h);

  // the sun's declination circle
  g.child(kit::disc(PL(0, 0), rs * kR)
              .key("tdec")
              .shape(shapes::circle())
              .fill(Fill::none())
              .stroke(stroke(2.0f, Fill::color(hexColor(0x2f6f9c, 0.85f)))));
  // the almucantar for the measured altitude
  g.child(kit::disc(PL(0, cy), ra * kR)
              .key("talm")
              .shape(shapes::circle())
              .fill(Fill::none())
              .stroke(stroke(2.0f, Fill::color(hexColor(0x2f6f9c, 0.85f)))));

  // their intersection — the hour angle, read off the DRAWN geometry
  const float y = (rs * rs - ra * ra + cy * cy) / (2 * cy);
  const float x = std::sqrt(std::max(0.0f, rs * rs - y * y));
  for (int s = -1; s <= 1; s += 2) {
    const SkPoint p = PL(s * x, y);
    g.child(kit::disc(p, 23.0f)
                .key(std::string("xr") + (s < 0 ? "a" : "b"))
                .shape(shapes::circle())
                .fill(Fill::none())
                .stroke(stroke(1.8f, Fill::color(hexColor(0x2f6f9c, 0.9f)))));
    g.child(box()
                .rect(SkRect::MakeXYWH(p.fX - 34, p.fY - 1.4f, 68, 2.8f))
                .key(std::string("xh") + (s < 0 ? "a" : "b"))
                .fill(Fill::color(kTrace)));
    g.child(box()
                .rect(SkRect::MakeXYWH(p.fX - 1.4f, p.fY - 34, 2.8f, 68))
                .key(std::string("xv") + (s < 0 ? "a" : "b"))
                .fill(Fill::color(kTrace)));
  }
  return g;
}

auto ChaucerAstrolabe::limb() -> Element {
  auto g = box().rect(SkRect::MakeXYWH(0, 0, kW, kH)).key("limb");

  // THE MATER IS FOUR FULL-DISC PASSES OVER 1086 px, and every one of them
  // is the same kind of thing: a field or a filter whose CONTENT is fixed
  // and whose entrance only fades the finished image in. Each declares its
  // bake, so the pass is rasterised once and the ramp costs one image draw
  // — the opacity rides the blit, outside the bake. Nothing here can be
  // left to automatic promotion: promotion is a per-frame cost estimate,
  // and the sweep that photographs this sketch runs with it switched off,
  // because a hash must not depend on how loaded the machine was.

  // the mater's brass field
  g.child(
      kit::disc(SkPoint{kCx, kCy}, kMaterR)
          .key("mater")
          .shape(shapes::circle())
          .cache(Cache::Texture)
          .fill(brass(0.50f))
          .background(shadow(hexColor(0x05070c, 0.62f), {8, 12}, 26))
          .foreground(styles::BevelEmboss{.depth = 3,
                                          .size = 6,
                                          .angleDeg = 125,
                                          .highlight = hexColor(0xfff0c4, 0.5f),
                                          .shadow = hexColor(0x2a1d08, 0.6f)})
          .opacity(animate(from(0.0f).to(1.0f), ramp(tMater * 1000, 700))));

  // the polished dome: a sheen centred slightly above the pin. glowUnit,
  // because it must FILL its box — radialUnit's radius is a fraction of
  // the HALF-DIAGONAL, so it reaches the corners and stops short of the
  // edges, which on a disc is the wrong stop entirely.
  g.child(kit::disc(SkPoint{kCx, kCy}, kMaterR)
              .key("sheen")
              .shape(shapes::circle())
              .cache(Cache::Texture)
              .fill(Paint::glowUnit({0.40f, 0.30f}, 0.95f,
                                    {{0.0f, hexColor(0xfff3cf, 0.30f)},
                                     {0.55f, hexColor(0xffdc8b, 0.10f)},
                                     {1.0f, hexColor(0x4f360e, 0.14f)}}))
              .blend(SkBlendMode::kSoftLight)
              .opacity(animate(from(0.0f).to(1.0f),
                               ramp(tMater * 1000 + 200, 700))));

  // brass is TOOLED, and the tool marks are fine concentric turning —
  // 120 stroked circles, which a picture would REPLAY by re-stroking
  // all 120. An image blits.
  g.child(kit::disc(SkPoint{kCx, kCy}, kMaterR)
              .key("turning")
              .shape(shapes::circle())
              .cache(Cache::Texture)
              .background(lines::presets::concentric(
                  Fill::color(hexColor(0x6b4d18, 0.055f)), 120, 0.9f))
              .opacity(animate(from(0.0f).to(1.0f),
                               ramp(tMater * 1000 + 300, 600))));
  // …and the tooling's own tooth, over the whole mater
  g.child(kit::disc(SkPoint{kCx, kCy}, kMaterR)
              .key("brassgrain")
              .shape(shapes::circle())
              .cache(Cache::Texture)
              .fill(brassGrain)
              .blend(SkBlendMode::kOverlay)
              .opacity(animate(from(0.0f).to(0.30f),
                               ramp(tMater * 1000 + 300, 600))));

  // the three rules of the limb: 1.155 / 1.082 / 1.005 R
  const float rules[3] = {1.155f, 1.082f, 1.005f};
  for (int i = 0; i < 3; ++i)
    g.child(
        kit::disc(SkPoint{kCx, kCy}, rules[i] * kR)
            .key("rule" + std::to_string(i))
            .shape(shapes::circle())
            .fill(Fill::none())
            .stroke(
                spans::upTo(animate(from(0.0f).to(1.0f),
                                    ramp(tMater * 1000 + 200 + (float)i * 120,
                                         900, ch::easeOutQuint))),
                groove(rules[i] * kR, i == 0 ? 3.0f : 2.0f, 0.75f, 0.45f)));

  // the 360 degree ticks — ONE atlas cell, three LENGTHS through
  // Pool::sizes(). This is the instancing case, and it works here for
  // exactly the reason it cannot work for the almucantars: a tick is a
  // FILLED rect, so a non-uniform scale changes its length without
  // changing the width of the mark. On a stroked circle the stroke IS the
  // shape's outline, and RSXform would scale it with the geometry.
  g.child(box()
              .rect(SkRect::MakeXYWH(0, 0, kW, kH))
              .key("ticks")
              .opacity(animate(from(0.0f).to(1.0f), ramp(tTicks * 1000, 700)))
              .child(instancing::instances(tickAtlas, tickPool,
                                           instancing::Mode::Data)));

  // the 12 degree numerals, RADIAL — the type radiates like a spoke,
  // because you turn the instrument to read a limb
  for (int i = 0; i < 12; ++i) {
    const int deg = i * 30;
    const float psi = 90.0f - (float)deg;  // plate angle of this division
    const float f = kPlateAngles.fraction(psi);
    const float rr = 1.104f * kR;
    g.child(
        text(toUtf8(std::to_string(deg == 0 ? 360 : deg)),
             type(faceLimb, 0.026f * kR, hexColor(0x33240c, 0.92f), 0.6f))
            .width(Dimension(2 * rr))
            .height(Dimension(2 * rr))
            .centerAt({kCx, kCy})
            .key("degnum" + std::to_string(i))
            .onPath(TextPath{.path = shapes::circle(),
                             .at = f,
                             .align = TextPath::Align::Center,
                             .offset = 0.0f,
                             .autoFlip = false,
                             .orient = TextPath::Orient::Radial})
            .opacity(animate(from(0.0f).to(1.0f),
                             ramp(tTicks * 1000 + 300 + (float)i * 25, 400))));
  }

  // the 24 hour letters, RADIAL. A at the first hour after noon, running
  // clockwise at 15°/hour; X is the 21st, at ψ = 135°, and 21 hours after
  // noon is 9 a.m. — which is exactly what Chaucer reads in II.3.
  for (int n = 1; n <= 24; ++n) {
    const float psi =
        arrange::along(90.0f, -360.0f, (size_t)n, 24, arrange::Turn::Closed);
    const float f = kPlateAngles.fraction(psi);
    const float rr = 1.044f * kR;
    const bool isX = (n == 21);
    g.child(text(toUtf8(kLetters[n - 1]), type(faceLimb, 0.040f * kR,
                                               isX ? hexColor(0x33240c, 1.0f)
                                                   : hexColor(0x33240c, 0.88f),
                                               0))
                .width(Dimension(2 * rr))
                .height(Dimension(2 * rr))
                .centerAt({kCx, kCy})
                .key("hl" + std::to_string(n))
                .onPath(TextPath{.path = shapes::circle(),
                                 .at = f,
                                 .align = TextPath::Align::Center,
                                 .offset = 0.0f,
                                 .autoFlip = false,
                                 .orient = TextPath::Orient::Radial})
                .opacity(animate(from(0.0f).to(1.0f),
                                 ramp(tLetters * 1000 + (float)n * 12, 380))));
    // the letter under the label lights as it passes
    const SkPoint glow = arrange::onEllipse({0, 0}, {1.044f, 1.044f}, psi * kD);
    g.child(kit::disc(MC(glow.fX, glow.fY), 0.052f * kR)
                .key("hlg" + std::to_string(n))
                .shape(shapes::circle())
                .fill(Paint::glowUnit({0.5f, 0.5f}, 1.0f,
                                      {{0.0f, hexColor(0xfff3cf, 0.85f)},
                                       {1.0f, hexColor(0xfff3cf, 0.0f)}}))
                .blend(SkBlendMode::kPlus)
                .opacity(&letterGlow[n - 1]));
  }

  // the throne (kursi) and its shackle — the instrument hangs plumb
  {
    const float th = 0.20f * kR;
    const SkPoint top{kCx, kCy - kMaterR};
    g.child(box()
                .rect(SkRect::MakeXYWH(top.fX - 0.16f * kR, top.fY - th,
                                       0.32f * kR, th + 0.05f * kR))
                .key("throne")
                .shape(shapes::blob(3u, 0.10f, 9))
                .fill(brass(0.74f))
                .foreground(
                    styles::BevelEmboss{.depth = 2,
                                        .size = 4,
                                        .angleDeg = 125,
                                        .highlight = hexColor(0xfff0c4, 0.6f),
                                        .shadow = hexColor(0x2a1d08, 0.6f)})
                .translateY(
                    animate(from(-26.0f).to(0.0f),
                            ramp(tMater * 1000 + 200, 900, ease::outBack())))
                .opacity(animate(from(0.0f).to(1.0f),
                                 ramp(tMater * 1000 + 200, 600))));
    g.child(kit::disc(SkPoint{top.fX, top.fY - th - 0.055f * kR}, 0.075f * kR)
                .key("shackle")
                .shape(shapes::annulus(0.62f))
                .fill(brass(0.78f))
                .translateY(
                    animate(from(-26.0f).to(0.0f),
                            ramp(tMater * 1000 + 260, 900, ease::outBack())))
                .opacity(animate(from(0.0f).to(1.0f),
                                 ramp(tMater * 1000 + 260, 600))));
  }
  return g;
}

auto ChaucerAstrolabe::rule() -> Element {
  auto g = box().rect(SkRect::MakeXYWH(0, 0, kW, kH)).key("rule");

  // the label swings with the sun: ψ = 90 − H, and the canvas angle is −ψ
  g.child(
      box()
          .rect(SkRect::MakeXYWH(kCx, kCy - 4.5f, kMaterR * 1.02f, 9.0f))
          .key("label")
          .transformOriginPx({0, 4.5f})
          .rotate(bind(&hourAngle).scale(1.0f).offset(-90.0f))
          .fill(brass(0.80f))
          .foreground(stroke(1.2f, Fill::color(hexColor(0x2a1d08, 0.7f))))
          .overlay(
              PathFormat{.width = 1.2f,
                         .strokeFill = Fill::color(hexColor(0x3a2a10, 0.85f)),
                         .dashIntervals = {2.0f, 6.0f}})
          .background(shadow(hexColor(0x2a1d08, 0.5f), {4, 5}, 7))
          .opacity(animate(from(0.0f).to(1.0f), ramp(tPin * 1000 + 200, 600))));

  // the pin and its horse
  g.child(
      kit::disc(SkPoint{kCx, kCy}, 0.040f * kR)
          .key("pin")
          .shape(shapes::circle())
          .fill(brass(0.82f))
          .foreground(styles::BevelEmboss{.depth = 2,
                                          .size = 2,
                                          .angleDeg = 125,
                                          .highlight = hexColor(0xfff0c4, 0.7f),
                                          .shadow = hexColor(0x2a1d08, 0.6f)})
          .background(shadow(hexColor(0x2a1d08, 0.5f), {2, 3}, 5))
          .opacity(animate(from(0.0f).to(1.0f), ramp(tPin * 1000, 420))));
  return g;
}

auto ChaucerAstrolabe::sunMark() -> Element {
  const float lam = sunLam.value();
  const float dec = sunDec(lam);
  const float H = hourAngle.value();
  const float psi = 90.0f - H;
  const SkPoint p =
      MC(rOfDec(dec) * std::cos(psi * kD), rOfDec(dec) * std::sin(psi * kD));
  const bool up = sunAlt.value() > 0;
  // the point OPPOSITE the sun on the ecliptic — the sun's nadir. The 12
  // seasonal-hour arcs are engraved BELOW the horizon, so read directly
  // they give the night hours of a body that is down; the traditional
  // daytime reading puts this mark on the same twelve curves instead.
  const float psiN = psi + 180.0f;
  const SkPoint q = MC(rOfDec(-dec) * std::cos(psiN * kD),
                       rOfDec(-dec) * std::sin(psiN * kD));
  return box()
      .rect(SkRect::MakeXYWH(0, 0, kW, kH))
      .child(kit::disc(p, 34.0f)
                 .shape(shapes::circle())
                 .fill(Paint::glowUnit({0.5f, 0.5f}, 1.0f,
                                       {{0.0f, up ? hexColor(0xfff3cf, 0.60f)
                                                  : hexColor(0x8fb0d0, 0.30f)},
                                        {1.0f, hexColor(0xfff3cf, 0.0f)}}))
                 .blend(SkBlendMode::kPlus))
      .child(
          kit::disc(p, 15.0f)
              .shape(shapes::star(12, 0.40f, 0.16f))
              .fill(Fill::color(up ? hexColor(0xfff6dc, 1.0f)
                                   : hexColor(0xa8bed2, 0.9f)))
              .foreground(stroke(1.4f, Fill::color(hexColor(0x2a1d08, 0.75f)))))
      .child(kit::disc(p, 5.0f)
                 .shape(shapes::circle())
                 .fill(Fill::color(hexColor(0x6b4d18, 0.8f))))
      .child(kit::disc(q, 7.0f)
                 .shape(shapes::circle())
                 .fill(Fill::none())
                 .stroke(stroke(1.6f, Fill::color(hexColor(0x2f6f9c, 0.75f)))))
      .child(kit::disc(q, 2.2f)
                 .shape(shapes::circle())
                 .fill(Fill::color(hexColor(0x2f6f9c, 0.8f))));
}
