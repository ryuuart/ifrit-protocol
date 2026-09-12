#include "ChaucerAstrolabe.h"

auto ChaucerAstrolabe::projectionPanel() -> Element {
  const float px = 1210, py = 148, pw = 450, ph = 372;
  auto g = panel(px, py, pw, ph, "THE PROIECTIOVN",
                 "stereographic, from the "
                 "SOVTH celestial pole");
  const SkPoint c{px + 172, py + 170};
  const float rr = 100;

  // the celestial sphere in section
  g.child(kit::disc(c, rr)
              .shape(shapes::circle())
              .fill(Fill::none())
              .stroke(stroke(1.6f, Fill::color(hexColor(0x241c15, 0.75f)))));
  // the equatorial plane — the plane of projection
  g.child(box()
              .rect(SkRect::MakeXYWH(c.fX - rr - 78, c.fY - 1, 2 * rr + 156, 2))
              .fill(Fill::color(hexColor(0x241c15, 0.6f))));
  // the two tropics as chords
  for (int s = -1; s <= 1; s += 2) {
    const float y = c.fY - s * rr * std::sin(kEps * kD);
    const float half = rr * std::cos(kEps * kD);
    g.child(box()
                .rect(SkRect::MakeXYWH(c.fX - half, y - 0.7f, 2 * half, 1.4f))
                .fill(Fill::color(hexColor(0x241c15, 0.42f))));
  }
  // the poles
  g.child(kit::disc(SkPoint{c.fX, c.fY - rr}, 3.4f)
              .shape(shapes::circle())
              .fill(Fill::color(kInk)));
  g.child(kit::disc(SkPoint{c.fX, c.fY + rr}, 5.0f)
              .shape(shapes::circle())
              .fill(Fill::color(kRubric)));
  g.child(text(toUtf8("N"), type(faceSerif, 15, kInk))
              .centerAt({c.fX + 14, c.fY - rr - 2}));
  g.child(text(toUtf8("S \xe2\x80\x94 the eye of the projection"),
               type(faceItalic, 13, kRubric))
              .at({c.fX + 10, c.fY + rr + 2}));
  g.child(slot("projray"));
  g.child(slot("projread"));
  return g;
}

auto ChaucerAstrolabe::projRay() -> Element {
  const float px = 1210, py = 148;
  const SkPoint c{px + 172, py + 170};
  const float rr = 100;
  const float dec = projDec.value();
  const SkPoint P{c.fX + rr * std::cos(dec * kD),
                  c.fY - rr * std::sin(dec * kD)};
  const SkPoint S{c.fX, c.fY + rr};
  // The equator projects onto ITSELF, so the section's sphere radius is the
  // equator's radius on the plate: the landing distance is rr·r(δ)/R_eq —
  // and the same number falls out of the straight line from S through P,
  // since cos δ/(1 + sin δ) IS tan((90−δ)/2). Two derivations, one point.
  const SkPoint Lp{c.fX + rr * rOfDec(dec) / kReq, c.fY};
  auto g = box().rect(SkRect::MakeXYWH(0, 0, kW, kH));
  SkPathBuilder pb;
  pb.moveTo(S);
  pb.lineTo(Lp.fX + (Lp.fX - S.fX) * 0.06f, Lp.fY + (Lp.fY - S.fY) * 0.06f);
  g.child(pathFigure(pb.detach(), 4)
              .fill(Fill::none())
              .stroke(PathFormat{
                  .width = 1.5f,
                  .strokeFill = Fill::color(hexColor(0x2f6f9c, 0.9f)),
                  .dashIntervals = {6.0f, 4.0f}}));
  g.child(kit::disc(P, 5.0f).shape(shapes::circle()).fill(Fill::color(kTrace)));
  g.child(
      kit::disc(Lp, 5.0f).shape(shapes::circle()).fill(Fill::color(kRubric)));
  g.child(box()
              .rect(SkRect::MakeXYWH(Lp.fX - 1, c.fY - 20, 2, 40))
              .fill(Fill::color(hexColor(0x8c2f22, 0.55f))));
  return g;
}

auto ChaucerAstrolabe::projRead() -> Element {
  const float px = 1210, py = 148, pw = 450;
  const float dec = projDec.value();
  auto row = [&](const std::string& s, SkColor4f c, float sz) {
    return text(toUtf8(s), type(faceMono, sz, c));
  };
  return box()
      .left(px + 16)
      .top(py + 264)
      .width(pw - 32)
      .column()
      .gap(4)
      .child(row(kit::formatted("\xce\xb4 = %+7.3f\xc2\xb0", dec), kInk, 14))
      .child(row(
          kit::formatted("r = R_eq\xc2\xb7tan((90\xe2\x88\x92\xce\xb4)/2) = "
                         "%.6f R",
                         rOfDec(dec)),
          kInk, 14))
      .child(row("R_can 0.424423  R_eq 0.651477  R_cap 1.000000",
                 hexColor(0x7b6a54), 12))
      .child(row("a circle through the EYE projects to a LINE \xe2\x80\x94 "
                 "which is",
                 hexColor(0x7b6a54), 12))
      .child(row("why the meridian, alone of the 12 azimuths, is straight.",
                 hexColor(0x7b6a54), 12));
}

auto ChaucerAstrolabe::panel(float x, float y, float w, float h,
                             const char* title, const char* sub) -> Element {
  auto g = box().rect(SkRect::MakeXYWH(0, 0, kW, kH));
  g.child(kit::sheet({.title = toUtf8(title),
                      .subtitle = sub && *sub ? toUtf8(sub) : std::u8string(),
                      .titleStyle = type(faceLimb, 15, kRubric, 1.9f),
                      .subtitleStyle = type(faceItalic, 14, hexColor(0x6b5a44)),
                      .marginX = 16,
                      .marginTop = 11,
                      .marginBottom = 12,
                      .subtitleGap = 5,
                      .contentGap = 13,
                      .ground = Fill::color(hexColor(0xe8dcc2, 0.62f)),
                      .rule = Fill::color(hexColor(0x241c15, 0.28f))},
                     box())
              .rect(SkRect::MakeXYWH(x, y, w, h))
              .stroke(stroke(1.0f, Fill::color(hexColor(0x241c15, 0.24f)),
                             PathFormat::Align::Inner)));
  return g;
}

auto ChaucerAstrolabe::familiesPanel() -> Element {
  const float px = 1210, py = 536, pw = 450, ph = 380;
  auto g = panel(px, py, pw, ph, "THE PLATE, IN FOVR FAMILIES",
                 "every curve is a circle; no two families share a centre");
  struct Fam {
    const char* name;
    const char* formula;
  };
  const Fam fams[4] = {
      {"CERCLES",
       "R \xc2\xb7 k\xe2\x81\xbf, k = tan(45\xc2\xb0\xe2\x88\x92\xce\xb5/2)"},
      {"ALMICANTERAS", "cy = R_eq cos \xcf\x86/(sin \xcf\x86 + sin h)"},
      {"AZIMVTES", "coaxal, zenith \xe2\x88\xa7 nadir"},
      {"HOVRES INEQVALES", "3-point circles, err 0.00374 R"}};
  for (int i = 0; i < 4; ++i) {
    const arrange::Cell at = arrange::cellAt((size_t)i, 2);
    const float cx = px + 116 + arrange::cellRect(at, {218, 0}).fLeft;
    const int row = at.row;
    const float cy = py + 126 + (float)row * 158;
    const float r = 54;
    auto d = kit::disc(SkPoint{cx, cy}, r)
                 .shape(shapes::circle())
                 .clip(true)
                 .fill(Fill::color(hexColor(0xf6efdd)))
                 .stroke(stroke(1.2f, Fill::color(hexColor(0x241c15, 0.5f)),
                                PathFormat::Align::Inner));
    auto put = [&](float mcx, float mcy, float mr, float w, float a) {
      d.child(kit::disc(SkPoint{r + mcx * r, r - mcy * r}, mr * r)
                  .shape(shapes::circle())
                  .fill(Fill::none())
                  .stroke(stroke(w, Fill::color(hexColor(0x241c15, a)))));
    };
    if (i == 0) {
      put(0, 0, 1.0f, 1.3f, 0.85f);
      put(0, 0, kReq, 1.3f, 0.85f);
      put(0, 0, kRcan, 1.3f, 0.85f);
    } else if (i == 1) {
      for (int h = 0; h <= 88; h += 4)
        put(0, almCy((float)h), almR((float)h), 0.8f, 0.55f);
    } else if (i == 2) {
      for (int a = 1; a <= 5; ++a)
        for (int s = -1; s <= 1; s += 2) {
          const path::PlaneCircle az = azimuth((float)s * (float)(a * 15));
          put(az.centre.x, az.centre.y, az.radius, 0.8f, 0.6f);
        }
      const path::PlaneCircle pv = azimuth(0.0f);
      put(pv.centre.x, pv.centre.y, pv.radius, 1.0f, 0.75f);
      d.child(box()
                  .rect(SkRect::MakeXYWH(r - 0.5f, 0, 1, 2 * r))
                  .fill(Fill::color(hexColor(0x241c15, 0.75f))));
    } else {
      for (int k = 1; k <= 11; ++k) {
        const std::optional<path::PlaneCircle> c = seasonalLine(k);
        if (c) put(c->centre.x, c->centre.y, c->radius, 0.8f, 0.6f);
      }
      d.child(box()
                  .rect(SkRect::MakeXYWH(r - 0.5f, r, 1, r))
                  .fill(Fill::color(hexColor(0x241c15, 0.7f))));
    }
    g.child(std::move(d));
    g.child(text(toUtf8(fams[i].name), type(faceLimb, 11.5f, kInk, 1.2f))
                .width(212)
                .textAlign(sigil::weave::TextAlignment::kCenter)
                .centerAt({cx, cy + r + 15}));
    g.child(
        text(toUtf8(fams[i].formula), type(faceMono, 9.5f, hexColor(0x7b6a54)))
            .width(212)
            .textAlign(sigil::weave::TextAlignment::kCenter)
            .centerAt({cx, cy + r + 30}));
  }
  return g;
}

auto ChaucerAstrolabe::backPanel() -> Element {
  const float px = 1210, py = 932, pw = 450, ph = 468;
  auto g = panel(px, py, pw, ph, "THE BAK",
                 "where Chaucer's example starts: a date and an altitude");
  const SkPoint c{px + 225, py + 232};
  const float r = 156;

  g.child(kit::disc(c, r)
              .shape(shapes::circle())
              .fill(brass(0.44f))
              .foreground(
                  styles::BevelEmboss{.depth = 2,
                                      .size = 4,
                                      .angleDeg = 125,
                                      .highlight = hexColor(0xffe9b0, 0.45f),
                                      .shadow = hexColor(0x2a1d08, 0.5f)}));
  // Four quadrants of 90° altitude scale (I.7–8): 181 rules every 2°,
  // every fifth of them heavier and reaching further in.
  //
  // TWO NODES, not 181. This was a node per rule, each carrying its own
  // bounding box and its own captured SkPath — 181 layouts, 181
  // reconciliations and 181 pictures for a ladder that never moves. It
  // is one path with N contours per stroke weight, and it needs two
  // nodes only because a node has one stroke width.
  //
  // The light pass SKIPS every fifth mark (a degenerate span), because
  // the two weights share a colour at 0.6 alpha: drawn over each other
  // the fifths would composite to 0.84 and print darker than the plate.
  {
    const path::Frame limb{.centre = c, .radius = r, .zero = path::Zero::East};
    auto ladder = [&](const shapes::Ticks& spec, float width) {
      g.child(
          pathFigure(shapes::ticks(limb, spec), 2)
              .fill(Fill::none())
              .stroke(stroke(width, Fill::color(hexColor(0x3a2a10, 0.6f)))));
    };
    ladder({.divisions = 180,
            .sweep = 360.0f,
            .closed = true,
            .mark = {0.925f, 0.96f},
            .classify =
                [](int i, shapes::Span s) {
                  return i % 5 == 0 ? shapes::Span{s.inner, s.inner} : s;
                }},
           0.7f);
    ladder({.divisions = 36,
            .sweep = 360.0f,
            .closed = true,
            .mark = {0.90f, 0.96f}},
           1.1f);
  }
  for (float rr : {0.90f, 0.96f, 0.855f, 0.78f, 0.70f})
    g.child(kit::disc(c, r * rr)
                .shape(shapes::circle())
                .fill(Fill::none())
                .stroke(groove(r * rr, 1.2f, 0.6f, 0.3f)));
  // the calendar and zodiac rings — Chaucer I.10 gives the month lengths
  {
    float acc = 0;
    for (int m = 0; m < 12; ++m) {
      const float days = (float)kMonthDays[m];
      const float a0 = -90.0f + acc / 365.0f * 360.0f;
      const float a1 = -90.0f + (acc + days) / 365.0f * 360.0f;
      acc += days;
      SkPathBuilder pb;
      pb.moveTo(arrange::onEllipse(c, {r * 0.78f, r * 0.78f}, a0 * kD));
      pb.lineTo(arrange::onEllipse(c, {r * 0.855f, r * 0.855f}, a0 * kD));
      g.child(pathFigure(pb.detach(), 2)
                  .fill(Fill::none())
                  .stroke(stroke(1.1f, Fill::color(hexColor(0x3a2a10, 0.7f)))));
      const float am = (a0 + a1) * 0.5f;
      g.child(text(toUtf8(kMonths[m]),
                   type(faceLimb, 9.5f, hexColor(0x33240c, 0.85f)))
                  .centerAt(arrange::onEllipse(c, {r * 0.817f, r * 0.817f},
                                               am * kD)));
      const float az =
          arrange::along(-90.0f, 360.0f, (size_t)m, 12, arrange::Turn::Closed);
      const float azm = az + 15.0f;
      g.child(text(toUtf8(std::string(kSigns[(m + 9) % 12]).substr(0, 3)),
                   type(faceLimb, 9.0f, hexColor(0x33240c, 0.7f)))
                  .centerAt(
                      arrange::onEllipse(c, {r * 0.74f, r * 0.74f}, azm * kD)));
    }
  }
  // the shadow square: umbra recta and umbra versa, 12 divisions each (I.12)
  {
    const float s = r * 0.50f;
    g.child(box()
                .rect(SkRect::MakeXYWH(c.fX - s, c.fY, 2 * s, s))
                .fill(Fill::none())
                .stroke(stroke(1.4f, Fill::color(hexColor(0x3a2a10, 0.75f)))));
    for (int i = 1; i < 12; ++i) {
      const float t = (float)i / 12.0f;
      g.child(box()
                  .rect(SkRect::MakeXYWH(c.fX - s + 2 * s * t, c.fY, 0.8f,
                                         s * (i % 3 == 0 ? 0.34f : 0.20f)))
                  .fill(Fill::color(hexColor(0x3a2a10, 0.6f))));
      g.child(
          box()
              .rect(SkRect::MakeXYWH(c.fX - s, c.fY + s * t,
                                     s * (i % 3 == 0 ? 0.34f : 0.20f), 0.8f))
              .fill(Fill::color(hexColor(0x3a2a10, 0.6f))));
      g.child(box()
                  .rect(SkRect::MakeXYWH(
                      c.fX + s - s * (i % 3 == 0 ? 0.34f : 0.20f), c.fY + s * t,
                      s * (i % 3 == 0 ? 0.34f : 0.20f), 0.8f))
                  .fill(Fill::color(hexColor(0x3a2a10, 0.6f))));
    }
    g.child(
        text(toUtf8("VMBRA RECTA"), type(faceLimb, 9, hexColor(0x33240c, 0.8f)))
            .centerAt({c.fX - s * 0.52f, c.fY + s * 0.86f}));
    g.child(
        text(toUtf8("VMBRA VERSA"), type(faceLimb, 9, hexColor(0x33240c, 0.8f)))
            .centerAt({c.fX + s * 0.52f, c.fY + s * 0.86f}));
  }
  // the alidade, swung to 25° 30′ — the measurement II.3 starts from
  g.child(
      box()
          .rect(SkRect::MakeXYWH(c.fX - r * 0.97f, c.fY - 5, 2 * r * 0.97f, 10))
          .transformOrigin(0.5f, 0.5f)
          .rotate(animate(from(0.0f).to(-25.5f),
                          ramp(tChaucer * 1000 + 200, 900, ease::outBack())))
          .fill(brass(0.76f))
          .foreground(stroke(1.0f, Fill::color(hexColor(0x2a1d08, 0.6f))))
          .background(shadow(hexColor(0x2a1d08, 0.45f), {2, 3}, 5)));
  for (int s = -1; s <= 1; s += 2)
    g.child(
        box()
            .rect(SkRect::MakeXYWH(c.fX + s * r * 0.90f - 5, c.fY - 16, 10, 32))
            .transformOriginPx({5.0f - s * r * 0.90f, 16})
            .rotate(animate(from(0.0f).to(-25.5f),
                            ramp(tChaucer * 1000 + 200, 900, ease::outBack())))
            .fill(brass(0.80f))
            .foreground(stroke(1.0f, Fill::color(hexColor(0x2a1d08, 0.6f)))));
  g.child(kit::disc(c, 8).shape(shapes::circle()).fill(brass(0.82f)));
  g.child(text(toUtf8("altitude 25\xc2\xb0 30\xe2\x80\xb2 \xe2\x80\x94 "
                      "12 March 1391"),
               type(faceItalic, 13, kRubric))
              .centerAt({px + 225, py + ph - 22}));
  return g;
}

auto ChaucerAstrolabe::specCard() -> Element {
  const float px = 1690, py = 148, pw = 646, ph = 286;
  auto g = panel(px, py, pw, ph, "PROVENANCE & SPECIFICATION",
                 "British Museum 1909,0617.1");
  struct KV {
    const char* k;
    const char* v;
  };
  const KV rows[] = {
      {"object", "planispheric astrolabe, English"},
      {"date", "1326 \xe2\x80\x94 earliest DATED astrolabe made in Europe"},
      {"material", "brass (medieval latten, Cu\xe2\x80\x93Zn)"},
      {"diameter", "132 mm  \xc2\xb7  mater ~10 mm thick"},
      {"plates",
       "Oxford \xc2\xb7 Jerusalem \xc2\xb7 Babilonie \xc2\xb7 "
       "Rome \xc2\xb7 Montpellier \xc2\xb7 Paris"},
      {"rete", "Y-shaped, 33 stars; birds, and a DOG'S HEAD for Sirius"},
      {"manual",
       "Chaucer, A Treatise on the Astrolabe, 1391 \xe2\x80\x94 "
       "the first technical manual in English"},
  };
  float y = py + 58;
  for (const KV& r : rows) {
    g.child(text(toUtf8(r.k), type(faceLimb, 11, hexColor(0x6b5a44), 1.2f))
                .left(px + 18)
                .top(y)
                .width(88));
    g.child(text(toUtf8(r.v), type(faceSerif, 13.5f, kInk))
                .left(px + 112)
                .top(y - 2)
                .width(pw - 130));
    y += 20;
  }
  // the two obliquities, side by side
  g.child(box()
              .rect(SkRect::MakeXYWH(px + 18, y + 6, pw - 36, 1))
              .fill(Fill::color(hexColor(0x241c15, 0.22f))));
  g.child(text(toUtf8("\xcf\x86 = 51\xc2\xb0 50\xe2\x80\xb2  Chaucer I.14, "
                      "Oxenford        \xce\xb5 = 23\xc2\xb0 50.0\xe2\x80\xb2  "
                      "Chaucer I.17"),
               type(faceMono, 12, kInk))
              .left(px + 18)
              .top(y + 14)
              .width(pw - 36));
  g.child(text(toUtf8("                                        \xce\xb5 = "
                      "23\xc2\xb0 31.6\xe2\x80\xb2  TRVE at 1326      "
                      "\xce\x94 18.4\xe2\x80\xb2"),
               type(faceMono, 12, kRubric))
              .left(px + 18)
              .top(y + 30)
              .width(pw - 36));
  g.child(
      text(toUtf8("his \xce\xb5 is an inherited PTOLEMAIC value, 1200 years "
                  "old \xe2\x80\x94 the equator comes out 0.586% small "
                  "(\xe2\x88\x92"
                  "0.229 mm), Cancer 1.175% "
                  "(\xe2\x88\x92"
                  "0.299 mm)"),
           type(faceItalic, 12.5f, kInk))
          .left(px + 18)
          .top(y + 52)
          .width(pw - 36));
  return g;
}

auto ChaucerAstrolabe::starPanel() -> Element {
  const float px = 1690, py = 450, pw = 646, ph = 450;
  auto g = panel(px, py, pw, ph, "THE RETE \xc2\xb7 XII STERRES",
                 "precessed J2000 \xe2\x86\x92 1326.0, IAU 1976 "
                 "\xce\xb6/z/\xce\xb8 \xe2\x80\x94 the sky has slid "
                 "8.6\xc2\xb0 in RA");
  g.child(text(toUtf8("name on the rete        modern         RA 1326   "
                      "dec 1326    r / R"),
               type(faceMono, 11, hexColor(0x6b5a44)))
              .at({px + 18, py + 60}));
  for (size_t i = 0; i < kStars.size(); ++i) {
    const float y = py + 80 + (float)i * 25.5f;
    const float r = rOfDec(kStars[i].dec1326);
    g.child(text(toUtf8(kStars[i].name), type(faceLimb, 12.5f, kInk, 0.8f))
                .at({px + 18, y}));
    g.child(text(toUtf8(kStars[i].modern),
                 type(faceItalic, 12.5f, hexColor(0x6b5a44)))
                .at({px + 168, y}));
    g.child(text(toUtf8(kit::formatted("%8.3f  %+8.3f   %.5f", kStars[i].ra1326,
                                       kStars[i].dec1326, r)),
                 type(faceMono, 11.5f, kInk))
                .at({px + 276, y + 1}));
    // where the star lands between Cancer and Capricorn
    const float bx = px + 480, bw = 148, lo = 0.18f;
    g.child(box()
                .rect(SkRect::MakeXYWH(bx, y + 8, bw, 1))
                .fill(Fill::color(hexColor(0x241c15, 0.3f))));
    for (float t : {kRcan, kReq, 1.0f})
      g.child(box()
                  .rect(SkRect::MakeXYWH(
                      bx + bw * (t - lo) / (1.0f - lo) - 0.5f, y + 4, 1, 9))
                  .fill(Fill::color(hexColor(0x241c15, 0.35f))));
    g.child(kit::disc(SkPoint{bx + bw * (r - lo) / (1.0f - lo), y + 8.5f}, 3.6f)
                .shape(shapes::circle())
                .fill(Fill::color(i == 3 ? kRubric : kInk)));
  }
  g.child(text(toUtf8("ALHABOR / Sirius at 0.868 R is the outermost by a long "
                      "way \xe2\x80\x94 the only southern star here, which is "
                      "why it gets the biggest pointer on every rete ever "
                      "made."),
               type(faceItalic, 12, kRubric))
              .left(px + 18)
              .top(py + ph - 44)
              .width(pw - 36));
  return g;
}

auto ChaucerAstrolabe::chaucerPanel() -> Element {
  const float px = 1690, py = 920, pw = 646, ph = 260;
  auto g = panel(px, py, pw, ph, "CHAVCER'S OWNE ENSAMPLE",
                 "A Treatise on the Astrolabe, II.3");
  g.child(
      text(toUtf8("\xe2\x80\x9cthe yeer of oure lord 1391, the 12 day of "
                  "March \xe2\x80\xa6 I took the altitude of my sonne, and "
                  "fond that it was 25 degrees and 30 of minutes \xe2\x80\xa6 "
                  "fond the poynte of my label in the bordure, up-on a "
                  "capital lettre that is cleped an X \xe2\x80\xa6 and fond "
                  "that it was 9 of the clokke of the day.\xe2\x80\x9d"),
           type(faceItalic, 13.5f, kInk))
          .left(px + 18)
          .top(py + 62)
          .width(pw - 36));
  g.child(slot("chaucer"));
  return g;
}

auto ChaucerAstrolabe::chaucerBody() -> Element {
  const float px = 1690, py = 920, pw = 646;
  auto g = box().left(px + 18).top(py + 140).width(pw - 36).column().gap(3);
  g.child(text(toUtf8(chaucerH), type(faceMono, 12.5f, kInk)));
  g.child(text(toUtf8(chaucerA), type(faceMono, 12.5f, kInk)));
  g.child(text(toUtf8(chaucerDelta), type(faceMono, 12.5f, kRubric)));
  g.child(box().height(6));
  g.child(text(toUtf8("Chaucer 09:00   \xc2\xb7   computed 08:53.8   "
                      "\xc2\xb7   \xce\x94 6.2 min \xe2\x80\x94 one hour-"
                      "letter's worth of reading precision on 132 mm"),
               type(faceSerif, 13, kInk)));
  return g;
}

auto ChaucerAstrolabe::zodiacPanel() -> Element {
  const float px = 1690, py = 1200, pw = 646, ph = 200;
  auto g = panel(px, py, pw, ph, "THE ZODIAC IS NOT VNIFORM",
                 "span of each sign ON THE RING \xe2\x80\x94 19.8\xc2\xb0 to "
                 "44.7\xc2\xb0, and they sum to 360.000000");
  const float bx = px + 22, by = py + 158, bw = pw - 44;
  const float maxSpan = 44.714f;
  for (int i = 0; i < 12; ++i) {
    const float a0 = ringAngle((float)(i * 30));
    float a1 = ringAngle((float)((i + 1) * 30));
    if (a1 < a0) a1 += 360.0f;
    const float span = a1 - a0;
    const float w = bw / 12.0f - 5.0f;
    const float h = 84.0f * span / maxSpan;
    g.child(
        box()
            .rect(SkRect::MakeXYWH(bx + (bw / 12.0f) * (float)i, by - h, w, h))
            .fill(Fill::color(span > 30 ? hexColor(0x8c2f22, 0.72f)
                                        : hexColor(0x241c15, 0.62f)))
            .scaleY(animate(
                from(0.0f).to(1.0f),
                ramp(tYear * 1000 + (float)i * 45, 520, ease::outBack())))
            .transformOrigin(0.5f, 1.0f));
    g.child(text(toUtf8(std::string(kSigns[i]).substr(0, 3)),
                 type(faceLimb, 10, hexColor(0x6b5a44), 0.5f))
                .width(w)
                .textAlign(sigil::weave::TextAlignment::kCenter)
                .centerAt({bx + (bw / 12.0f) * (float)i + w * 0.5f, by + 12}));
    // The 30° reference rule cuts across this band. A bar within ~9 px of
    // it (the 24.9° signs) put its value label ON the rule and the dashes
    // struck the digits through; those labels centre in the gap instead.
    const float y30r = by - 84.0f * 30.0f / maxSpan;
    float ly = by - h - 9;
    if (h < 84.0f * 30.0f / maxSpan && ly - 5.0f < y30r)
      ly = 0.5f * ((by - h) + y30r);
    g.child(
        text(toUtf8(kit::formatted("%.1f", span)), type(faceMono, 9.5f, kInk))
            .width(w)
            .textAlign(sigil::weave::TextAlignment::kCenter)
            .centerAt({bx + (bw / 12.0f) * (float)i + w * 0.5f, ly}));
  }
  // the 30° reference — a 5-on/4-off STRIPE tile filling a 1 px band, not
  // a dashed stroke around the perimeter of a 1 px box. The perimeter walk
  // laid the dash down twice, one scanline apart and out of phase, and the
  // pair read as a sawtooth rather than a rule.
  const float y30 = by - 84.0f * 30.0f / maxSpan;
  g.child(box()
              .rect(SkRect::MakeXYWH(bx, y30, bw, 1))
              .fill(Pattern(patterns::stripes(
                                5.0f, 4.0f,
                                skia::toColor(hexColor(0x241c15, 0.55f))))
                        .material()));
  g.child(text(toUtf8("30\xc2\xb0 \xe2\x80\x94 an unprojected ring"),
               type(faceItalic, 11, hexColor(0x7b6a54)))
              .at({bx + 4, y30 - 16}));
  // the live sign marker
  g.child(box()
              .rect(SkRect::MakeXYWH(bx - 3, by + 2, bw / 12.0f - 5.0f + 6, 3))
              .fill(Fill::color(kTrace))
              .translateX(bind(&signMark).scale(bw / 12.0f)));
  return g;
}

auto ChaucerAstrolabe::logStyle() -> feed::TextOptions {
  feed::TextOptions s;
  s.styles = kit::tinted(faceMono, 9.4f, kInk,
                         {{"dim", hexColor(0x7b6a54)},
                          {"heading", kRubric},
                          {"pass", hexColor(0x1d6b3f)},
                          {"fail", hexColor(0x8c2f22)}});
  s.window.gap = 0.6f;
  // Every row of the four tables, so a heading is never scrolled off the
  // top of the column it titles.
  s.window.visible = 16;
  return s;
}

auto ChaucerAstrolabe::consolePanel() -> Element {
  return kit::console(
             {.feeds = {&logA, &logB, &logC, &logD},
              .style = logStyle(),
              .plate = {.paddingX = 14,
                        .paddingY = 9,
                        .gap = 18,
                        .fill = Fill::color(hexColor(0xe4d9c0, 0.78f)),
                        .border = Fill::color(hexColor(0x241c15, 0.25f)),
                        .divider = Fill::color(hexColor(0x241c15, 0.18f))}})
      .rect(SkRect::MakeXYWH(64, 1396, kW - 128, 190));
}

auto ChaucerAstrolabe::titleStrip() -> Element {
  auto g = box().rect(SkRect::MakeXYWH(0, 0, kW, kH));
  g.child(sketch::kit::titleCard(
              {.title = {toUtf8("ASTROLABIVM \xc2\xb7 ANNO DOMINI M CCC "
                                "XXVI")},
               .subtitle = {toUtf8("compowned after the latitude of "
                                   "Oxenford \xc2\xb7 51\xc2\xb0 "
                                   "50\xe2\x80\xb2")},
               .notes = {{.words = toUtf8("British Museum 1909,0617.1 "
                                          "\xc2\xb7 brass \xc2\xb7 132 "
                                          "mm \xc2\xb7 the earliest "
                                          "dated astrolabe made in "
                                          "Europe"),
                          .ink = Fill::color(hexColor(0x6b5a44))}},
               .ruled = true})
              .left(64)
              .top(44)
              .width(Dimension(kW - 128)));
  return g;
}

auto ChaucerAstrolabe::readout() -> Element {
  const int hh = (int)std::floor(latHours);
  const float mm = (latHours - (float)hh) * 60.0f;
  const int n = ((int)std::lround(hourAngle.value() / 15.0f) + 24 * 4) % 24;
  const int letter = (n == 0 ? 24 : n);
  auto g = box().left(92).top(1334).width(1064).row().gap(26).alignItems(
      Align::Baseline);
  auto cell = [&](const std::string& k, const std::string& v, SkColor4f c) {
    return box()
        .column()
        .gap(1)
        .child(text(toUtf8(k), type(faceLimb, 10, hexColor(0x8a99b0), 1.4f)))
        .child(text(toUtf8(v), type(faceMono, 19, c)));
  };
  g.child(cell("LOCAL APPARENT TIME", kit::formatted("%02d:%04.1f", hh, mm),
               hexColor(0xffdc8b)));
  g.child(cell("HOVR ANGLE",
               kit::formatted("%+8.3f\xc2\xb0", hourAngle.value()),
               hexColor(0xd8c79c)));
  g.child(cell("SONNE ALTITVDE",
               kit::formatted("%+7.3f\xc2\xb0", sunAlt.value()),
               hexColor(0xd8c79c)));
  g.child(cell("SONNE IN",
               kit::formatted("\xce\xbb %6.2f\xc2\xb0", sunLam.value()),
               hexColor(0xd8c79c)));
  g.child(cell(
      "LETTRE IN THE BORDVRE",
      std::string(kLetters[letter - 1]) + "  (" + std::to_string(letter) + ")",
      hexColor(0xffdc8b)));
  g.child(cell("HOVRE INEQVAL",
               sunAlt.value() > 0
                   ? std::string("\xe2\x80\x94 day")
                   : std::string("night ") + std::to_string(nightHour),
               hexColor(0xd8c79c)));
  return g;
}
