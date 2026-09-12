#include "Minard1869.h"

auto Minard1869::paperGround() -> Element {
  return box()
      .inset(0)
      .fill(paperMat)
      // the ageing: warmer and dirtier toward the edges
      .foreground(decorations::wash(vignette, SkBlendMode::kMultiply, 0.30f))
      .cache(Cache::Texture)
      .key("paper");
}

auto Minard1869::frames() -> Element {
  auto g = box().inset(0);
  // the two printed frames are ONE rule around each panel, drawn as a
  // double rule the way the plate cuts them
  auto rule = [&](float l, float t, float r, float bm, const char* k, float t0,
                  float t1) {
    g.child(box()
                .inset(0)
                .shape(pathFn(rectPath(l, t, r, bm)))
                .stroke(spans::upTo(beat(t0, t1)),
                        lines::Line{.width = 1.1f,
                                    .fill = Fill::color(kInk),
                                    .parallels = 2,
                                    .gap = 3.0f})
                .key(k));
  };
  rule(kFrameL, kFrameT, kFrameR, kDivHN, "frameH", 0.25f, 1.1f);
  rule(kFrameL, kDivHN + 4, kFrameR, kFrameB, "frameN", 0.35f, 1.2f);
  // the map | temperature divider
  g.child(box()
              .inset(0)
              .shape(segFn({kFrameL, kDivNT}, {kFrameR, kDivNT}))
              .stroke(spans::upTo(beat(0.4f, 1.2f)),
                      stroke(1.0f, Fill::color(kInk)))
              .key("divNT"));
  return g;
}

auto Minard1869::provenance() -> Element {
  auto g = box().inset(0);
  g.child(text(toUtf8("pour la Bibliothèque impériale"),
               type(faceScript, 30, kManuscript, 1.0f))
              .at({40, 4})
              .key("dedic")
              .mask(by::edge(0.0f, beat(0.45f, 1.15f))));
  g.child(text(toUtf8("Ge Don 4182"), type(faceScript, 16, kManuscript, 0.4f))
              .at({1218, 12})
              .key("gedon")
              .opacity(beat(0.9f, 1.2f)));
  auto stamp = [&](float cx, float cy, float r, const char* label,
                   const char* k, float t0) {
    g.child(
        box()
            .rect(SkRect::MakeXYWH(cx - r, cy - r * 0.66f, 2 * r, 1.32f * r))
            .shape(shapes::circle())
            .stroke(stroke(1.5f, Fill::color(kStampRed)))
            .child(text(toUtf8(label), type(faceRoman, 7.5f, kStampRed, 0.3f))
                       .at({r * 0.35f, r * 0.42f}))
            .key(k)
            .scale(animate(from(0.0f).to(1.0f),
                           ramp(t0 * 1000, 420, ch::EaseOutBack())))
            .opacity(beat(t0, t0 + 0.2f)));
  };
  stamp(230, 118, 30, "DON\nN° 4182", "stamp1", 0.85f);
  stamp(920, 118, 26, "BIBL.", "stamp2", 0.95f);
  return g;
}

auto Minard1869::spot(int i) const -> SpotRead {
  switch ((unsigned)i & 3u) {
    case 0:
      return {mapX(24.0f) + 8, mapY(54.9f), bandPx(422000) * 0.5f,
              47.15f,          422000,      "Napoléon, at the Niemen"};
    case 1:
      return {mapX(24.9f), mapY(55.0f), bandPx(400000) * 0.5f,
              44.54f,      400000,      "Napoléon, after the northern column"};
    case 2:
      return {mapX(37.0f), mapY(55.62f), bandPx(100000) * 0.5f,
              11.43f,      100000,       "Napoléon, at Moscou"};
    default:
      return {222,    264,   bandPx(96000) * 0.5f,
              10.84f, 96000, "Annibal, at the Ebro — a DIFFERENT panel"};
  }
}

auto Minard1869::caliper() -> Element {
  const SpotRead r = spot(calStep);
  auto g = box().inset(0).key("caliperGrp").opacity(&calAlpha);
  auto jaw = [&](float x, float y, float halfPx, const char* k) {
    SkPathBuilder p;
    p.moveTo(x - 16, y - halfPx);
    p.lineTo(x + 16, y - halfPx);
    p.moveTo(x - 16, y + halfPx);
    p.lineTo(x + 16, y + halfPx);
    p.moveTo(x + 12, y - halfPx);
    p.lineTo(x + 12, y + halfPx);
    g.child(
        box()
            .inset(0)
            .shape(pathFn(p.detach()))
            .background(shadow(hexColor(0x000000, 0.30f), {1.5f, 2.0f}, 3.0f))
            .stroke(stroke(2.0f, Fill::color(kBlue)))
            .key(k));
  };
  jaw(r.x, r.y, r.halfPx, "jaw1");
  const float rx = kFrameL + 10, ry = 686.0f;
  g.child(
      text(toUtf8(kit::formatted("%.2f mm", r.mm)), type(faceUiBold, 17, kBlue))
          .at({rx, ry})
          .key("calread"));
  g.child(text(toUtf8(kit::formatted("÷ %.0f = %.4f mm / 10.000", r.men,
                                     r.mm / (r.men / 10000.0f))),
               type(faceUi, 9.5f, kBlue))
              .at({rx, ry + 20})
              .key("calread2"));
  g.child(text(toUtf8(std::string(r.where) +
                      "\n(measured on the BnF sheet, no cross-scan "
                      "calibration)"),
               type(faceUi, 9, hexColor(0x2f6f9c, 0.9f)))
              .at({rx, ry + 33})
              .key("calread3"));
  g.child(
      text(toUtf8("the legend says 1.0000"), type(faceUiBold, 10, kClaimRed))
          .at({rx, ry + 58})
          .key("calread4"));
  return g;
}

auto Minard1869::sheet(sketch::SketchContext& ctx) -> Element {
  return box()
      .rect(SkRect::MakeXYWH(kSheetX, kSheetY, kSheetW, kSheetH))
      .background(shadow(hexColor(0x000000, 0.55f), {6, 10}, 26))
      .child(paperGround())
      .child(frames())
      .child(hannibalPanel())
      .child(napoleonPanel(ctx))
      .child(temperaturePanel())
      .child(imprints())
      .child(provenance())
      .child(box()
                 .inset(0)
                 .fill(Fill::color(hexColor(0x120f0b)))
                 .opacity(&dimAmt)
                 .key("dim"))
      // (the dim veil is painted BELOW this: an instrument laid on the
      // paper does not dim with it.)
      // NOTE the key on caliper()'s root is "caliperGrp", NOT
      // "caliper". slot(name) stores `name` as the slot node's key, and
      // the content rendered into the slot carries a key of its own, so
      // two nodes in the same tree would answer to "caliper" if both were
      // spelled that way. Slot lookup does not go through the general key
      // index, so this is a readability rule rather than a correctness
      // one: keeping the names apart means a key in a log or a hit test
      // names exactly one node, and the content can be re-rendered into
      // the slot without anyone having to work out which was found.
      .child(slot("caliper"))
      .key("sheet")
      .opacity(beat(0.0f, 0.6f));
}

auto Minard1869::card(float y, float h, const char* title, const char* key,
                      float t0, Element body) -> Element {
  auto c = box()
               .rect(SkRect::MakeXYWH(kAuditX, y, kAuditW, h))
               .key(key)
               .opacity(beat(t0, t0 + 0.4f))
               .translateY(bind(&T).window(t0, t0 + 0.4f).invert().scale(14));
  c.child(kit::sheet({.title = toUtf8(title),
                      .titleStyle = type(faceUiBold, 15, kCardInk, 1.6f),
                      .marginX = 18,
                      .marginTop = 12,
                      .marginBottom = 12,
                      .contentGap = 13,
                      .ground = Fill::color(kCard),
                      .rule = Fill::color(kCardInk)},
                     box())
              .inset(0)
              .stroke(stroke(1.0f, Fill::color(hexColor(0xcfc6b4)))));
  c.child(std::move(body));
  return c;
}

auto Minard1869::cardScale() -> Element {
  // the measured staircase (Commons scan), from the file
  const std::vector<Measured>& treads = plate.treads;
  const float slope = 3.828f, intercept = -0.19f, r2 = 0.99266f;
  const float px0 = 60, py0 = 60, pw = 560, ph = 236;
  // WHAT THE TWO AXES MEAN, as the library's own mapping value: men
  // across, band px up. Every dot, the fitted line and any label go
  // through this one `at()`, so a mark and its caption cannot land on
  // two different arithmetics.
  const kit::Plot plot{.fromT = 0, .toT = 440000, .fromY = 0, .toY = 180};
  const SkSize field{pw, ph};
  auto P = [&](float men, float px) {
    const SkPoint q = plot.at(men, px, field);
    return SkPoint{px0 + q.x(), py0 + q.y()};
  };
  auto g = box().inset(0);
  // axes
  g.child(box()
              .inset(0)
              .shape(segFn({px0, py0 + ph}, {px0 + pw, py0 + ph}))
              .stroke(stroke(1.0f, Fill::color(kCardInk))));
  g.child(box()
              .inset(0)
              .shape(segFn({px0, py0}, {px0, py0 + ph}))
              .stroke(stroke(1.0f, Fill::color(kCardInk))));
  // the fitted line
  g.child(
      box()
          .inset(0)
          .shape(segFn(P(0, intercept), P(440000, intercept + slope * 44.0f)))
          .stroke(spans::upTo(beat(tScale + 1.8f, tScale + 2.4f)),
                  stroke(1.6f, Fill::color(kBlue)))
          .key("fitline"));
  for (size_t i = 0; i < treads.size(); ++i) {
    g.child(kit::disc(P(treads[i].men, treads[i].mm), 3.6f)
                .shape(shapes::circle())
                .fill(Paint::solid(kBlue))
                .key("tread" + std::to_string(i))
                .opacity(beat(tScale + 0.6f + 0.09f * (float)i,
                              tScale + 0.8f + 0.09f * (float)i)));
  }
  g.child(text(toUtf8("width_px = 3.828 px per 10,000 men,  intercept "
                      "−0.19 px,  R² = 0.99266"),
               type(faceUi, 11, kBlue))
              .at({px0 + 6, py0 + 4})
              .key("fitlab")
              .opacity(beat(tScale + 2.3f, tScale + 2.6f)));
  g.child(text(toUtf8("the fitted line goes through the ORIGIN to a fifth of "
                      "a pixel — the zones are not merely\nlinear in men, "
                      "they are proportional"),
               type(faceUi, 10, kGrey))
              .at({px0 + 6, py0 + 22})
              .key("proplab")
              .opacity(beat(tScale + 2.4f, tScale + 2.7f)));
  g.child(text(toUtf8("men →"), type(faceUi, 9, kGrey))
              .at({px0 + pw - 40, py0 + ph + 6}));
  g.child(text(toUtf8("px"), type(faceUi, 9, kGrey)).at({px0 - 24, py0 - 2}));

  // the two horizontal rules that matter
  // the two rules are drawn PROPORTIONAL: their lengths are the two
  // millimetre values, so the 12.6% is a length rather than a caption
  const float rx = 660, rwUnit = 268;
  auto ruleRow = [&](float y, const char* v, const char* what, SkColor4f col,
                     const char* k, float t0, float mm) {
    const float rw = rwUnit * mm;
    g.child(box()
                .inset(0)
                .shape(segFn({rx, y}, {rx + rw, y}))
                .stroke(spans::upTo(beat(t0, t0 + 0.3f)),
                        stroke(2.0f, Fill::color(col)))
                .key(k));
    g.child(text(toUtf8(v), type(faceUiBold, 17, col))
                .at({rx, y - 26})
                .key(std::string(k) + "v")
                .opacity(beat(t0, t0 + 0.3f)));
    g.child(text(toUtf8(what), type(faceUi, 10, col))
                .at({rx, y + 6})
                .key(std::string(k) + "w")
                .opacity(beat(t0, t0 + 0.3f)));
  };
  ruleRow(140, "1.000 mm", "what the legend says", kClaimRed, "ruleStated",
          tScale + 0.4f, 1.0f);
  ruleRow(196, "1.126 mm", "what the ink measures  (12.6% wider)", kBlue,
          "ruleMeasured", tScale + 2.0f, kMmPer10k);
  g.child(box()
              .inset(0)
              .shape(segFn({rx + rwUnit, 132}, {rx + rwUnit, 204}))
              .stroke(PathFormat{.width = 1.0f,
                                 .strokeFill = Fill::color(kGrey),
                                 .dashIntervals = {3, 3}})
              .key("ruleTick")
              .opacity(beat(tScale + 2.0f, tScale + 2.3f)));
  g.child(text(toUtf8("half a French ligne = 1.1279 mm      (SPECULATION)"),
               type(faceUi, 10, kGrey))
              .at({rx, 238})
              .key("ligneNote")
              .opacity(beat(tLigne, tLigne + 0.4f)));
  g.child(text(toUtf8("competing: litho reduction · catalogued paper size "
                      "wrong (this one would kill it)"),
               type(faceUi, 9, kGrey))
              .at({rx, 254})
              .key("ligneAlt")
              .opacity(beat(tLigne + 0.4f, tLigne + 0.8f)));
  g.child(text(toUtf8("and the SAME factor appears on the Hannibal panel, "
                      "drawn from different data."),
               type(faceUi, 10, kCardInk))
              .at({rx, 276})
              .key("hannSame")
              .opacity(beat(tScale + 2.5f, tScale + 2.8f)));
  return g;
}

auto Minard1869::cardFloor() -> Element {
  const std::vector<Measured>& pts = plate.floorPts;
  const float px0 = 60, py0 = 58, pw = 470, ph = 108;
  auto g = box().inset(0);
  // THE DOMAIN IS THE LOGARITHM. The floor is a fact about the smallest
  // strengths, which crowd into the last twentieth of a linear axis, so
  // the abscissa is log10(men) and the mapping value is handed that
  // rather than the men.
  const kit::Plot plot{
      .fromT = 3.5f, .toT = 5.05f, .fromY = 3.0f, .toY = 11.5f};
  const SkSize field{pw, ph};
  auto P = [&](float men, float px) {
    const SkPoint q = plot.at(std::log10(std::max(men, 1000.0f)), px, field);
    return SkPoint{px0 + q.x(), py0 + q.y()};
  };
  g.child(box()
              .inset(0)
              .shape(segFn({px0, py0 + ph}, {px0 + pw, py0 + ph}))
              .stroke(stroke(1.0f, Fill::color(kCardInk))));
  SkPathBuilder line;
  for (size_t i = pts.size(); i-- > 0;) {
    const SkPoint q = P(pts[i].men, pts[i].mm);
    i == pts.size() - 1 ? line.moveTo(q) : line.lineTo(q);
  }
  g.child(box()
              .inset(0)
              .shape(pathFn(line.detach()))
              .stroke(spans::upTo(beat(tScale + 1.0f, tScale + 1.8f)),
                      stroke(1.6f, Fill::color(kBlue)))
              .key("floorline"));
  // the crayon floor
  g.child(box()
              .inset(0)
              .shape(segFn({px0, P(1000, 3.83f).y()},
                           {px0 + pw, P(1000, 3.83f).y()}))
              .stroke(PathFormat{.width = 1.0f,
                                 .strokeFill = Fill::color(kGrey),
                                 .dashIntervals = {5, 4}})
              .key("floorRule")
              .opacity(beat(tScale + 1.4f, tScale + 1.7f)));
  g.child(text(toUtf8("3.8 px per 10,000 — the advance band's slope; the "
                      "retreat holds it above ~35,000 men"),
               type(faceUi, 9, kGrey))
              .at({px0 + 120, py0 - 14})
              .key("floorLab")
              .opacity(beat(tScale + 1.5f, tScale + 1.8f)));
  for (size_t i = 0; i < pts.size(); ++i)
    g.child(kit::disc(P(pts[i].men, pts[i].mm), 3.0f)
                .shape(shapes::circle())
                .fill(Paint::solid(i >= 8 ? kAmber : kBlue))
                .key("fp" + std::to_string(i))
                .opacity(beat(tScale + 1.0f + 0.05f * (float)i,
                              tScale + 1.2f + 0.05f * (float)i)));
  for (float men : {4000.0f, 10000.0f, 30000.0f, 100000.0f})
    g.child(text(toUtf8(french(men)), type(faceUi, 8.5f, kGrey))
                .at({P(men, 3.0f).x() - 12, py0 + ph + 4})
                .key("fx" + std::to_string((int)men)));
  g.child(text(toUtf8("4,000 men drawn 2.6× too wide — 0.4 mm is "
                      "below what a lithographic crayon will hold"),
               type(faceUi, 10, kAmber))
              .at({px0, py0 + ph + 18})
              .key("floorAmber")
              .opacity(beat(tScale + 1.8f, tScale + 2.1f)));
  g.child(text(toUtf8("minimum drawn width 5.4 px = 1.57 mm"),
               type(faceUi, 10, kCardInk))
              .at({px0 + 480, py0 + 44})
              .key("floorMin")
              .opacity(beat(tScale + 1.9f, tScale + 2.2f)));
  g.child(text(toUtf8("NEGATIVE RESULT — and it is the more useful half: "
                      "the famous 12,000→14,000 anomaly is NOT\nmeasurable "
                      "in the ink. At the floor both readings are 5.4 px. The "
                      "prettier finding does not exist."),
               type(faceUi, 10, kClaimRed))
              .left(Dimension(px0 + 480))
              .top(Dimension(py0 + 10))
              .width(Dimension(kAuditW - px0 - 500))
              .key("floorNeg")
              .opacity(beat(tScale + 2.2f, tScale + 2.6f)));
  return g;
}

auto Minard1869::cardGeo() -> Element {
  auto g = box().inset(0);
  const float ox = 40, oy = 52, sc = 31.0f;  // px per degree, inset map
  const float exagg = 8.0f;
  auto MX = [&](float lon) { return ox + (lon - 23.5f) * sc; };
  auto MY = [&](float lat) { return oy + (56.2f - lat) * sc * 1.4f; };
  // Minard's cities as dots, the real positions as crosses, residual
  // vectors at 20x
  // the route itself, so the dots read as a campaign and not a scatter
  {
    SkPathBuilder rt;
    const std::vector<Station>* legs[] = {&plate.advTrunk, &plate.retEast,
                                          &plate.retWest};
    for (const std::vector<Station>* v : legs)
      for (size_t i = 0; i < v->size(); ++i) {
        const SkPoint q{MX((*v)[i].lon), MY((*v)[i].lat)};
        i == 0 ? rt.moveTo(q) : rt.lineTo(q);
      }
    g.child(box()
                .inset(0)
                .shape(pathFn(rt.detach()))
                .stroke(spans::upTo(beat(tGeo, tGeo + 0.5f)),
                        stroke(1.4f, Fill::color(hexColor(0x1c1a17, 0.35f))))
                .key("georoute"));
  }
  SkPathBuilder crosses, vectors;
  for (const City& c : plate.cities) {
    const float mx = MX(c.lon), my = MY(c.lat);
    const float rx = mx + (c.rlon - c.lon) * sc * exagg;
    const float ry = my - (c.rlat - c.lat) * sc * 1.4f * exagg;
    crosses.moveTo(rx - 3, ry);
    crosses.lineTo(rx + 3, ry);
    crosses.moveTo(rx, ry - 3);
    crosses.lineTo(rx, ry + 3);
    vectors.moveTo(mx, my);
    vectors.lineTo(rx, ry);
  }
  g.child(box()
              .inset(0)
              .shape(pathFn(vectors.detach()))
              .stroke(spans::upTo(beat(tGeo + 0.5f, tGeo + 1.1f)),
                      stroke(0.8f, Fill::color(hexColor(0x2f6f9c, 0.6f))))
              .key("geovec"));
  g.child(box()
              .inset(0)
              .shape(pathFn(crosses.detach()))
              .stroke(stroke(1.0f, Fill::color(kCardInk)))
              .key("geocross")
              .opacity(beat(tGeo + 0.2f, tGeo + 0.6f)));
  for (size_t i = 0; i < plate.cities.size(); ++i) {
    const City& c = plate.cities[i];
    const bool out = cityKm(c) > 20.0f;
    g.child(kit::disc(SkPoint{MX(c.lon), MY(c.lat)}, out ? 4.0f : 2.6f)
                .shape(shapes::circle())
                .fill(Paint::solid(out ? kAmber : kBlue))
                .key("gc" + std::to_string(i))
                .opacity(beat(tGeo + 0.1f + 0.02f * (float)i,
                              tGeo + 0.35f + 0.02f * (float)i)));
    if (out)
      g.child(text(toUtf8(c.plate), type(faceUiBold, 10, kAmber))
                  .at({MX(c.lon) + 7, MY(c.lat) - 6})
                  .key("gcl" + std::to_string(i))
                  .opacity(beat(tGeo + 1.6f, tGeo + 1.9f)));
  }

  // the histogram of the 20 residuals
  const float hx = 640, hy = 58, hw = 250, hh = 108;
  std::array<int, 8> bins{};
  for (const City& c : plate.cities) {
    int b = (int)(cityKm(c) / 5.0f);
    bins[(size_t)std::min(7, b)]++;
  }
  for (size_t i = 0; i < bins.size(); ++i) {
    const float bw = hw / 8.0f;
    const float bh = (float)bins[i] / 9.0f * hh;
    g.child(
        box()
            .rect(SkRect::MakeXYWH(hx + bw * (float)i + 1, hy + hh - bh, bw - 2,
                                   std::max(bh, 1.0f)))
            .fill(Paint::solid(i >= 4 ? kAmber : kBlue))
            .key("hist" + std::to_string(i))
            .scale(animate(from(0.0f).to(1.0f),
                           ramp((tGeo + 1.0f) * 1000 + 60.0f * (float)i, 320)))
            .transformOrigin(0.5f, 1.0f)
            .opacity(beat(tGeo + 1.0f + 0.06f * (float)i,
                          tGeo + 1.2f + 0.06f * (float)i)));
  }
  // the digitisation quantum, as a grey band behind
  g.child(box()
              .rect(SkRect::MakeXYWH(hx + hw * 6.41f / 40.0f - 6, hy, 12, hh))
              .fill(Paint::solid(hexColor(0x6d675c, 0.22f)))
              .key("quantum")
              .opacity(beat(tGeo + 1.3f, tGeo + 1.6f)));
  g.child(text(toUtf8("residual vectors ×8"), type(faceUi, 9, kGrey))
              .at({ox, oy + 150})
              .key("exaggLab")
              .opacity(beat(tGeo + 0.6f, tGeo + 0.9f)));
  g.child(text(toUtf8("0.1° grid = 6.41 km"), type(faceUi, 9, kGrey))
              .at({hx + hw * 6.41f / 40.0f + 10, hy + 4})
              .key("quantumLab")
              .opacity(beat(tGeo + 1.35f, tGeo + 1.65f)));
  g.child(text(toUtf8("median 5.35 km on an 871 km span — 0.6%. The "
                      "received account is wrong."),
               type(faceUiBold, 12, kPass))
              .left(Dimension(hx))
              .top(Dimension(hy + hh + 10))
              .width(Dimension(330))
              .key("geoCap")
              .opacity(beat(tGeo + 1.7f, tGeo + 2.0f)));
  g.child(text(toUtf8("residual is within 1.8× of what the 0.1° "
                      "digitisation grid alone produces"),
               type(faceUi, 9, kGrey))
              .left(Dimension(hx))
              .top(Dimension(hy + hh + 42))
              .width(Dimension(330))
              .key("geoCap2")
              .opacity(beat(tGeo + 1.8f, tGeo + 2.1f)));
  return g;
}

auto Minard1869::cardLegs() -> Element {
  const std::vector<Leg>& legs = plate.legs;
  auto g = box().inset(0);
  const float bx = 250, by = 50, bw = 480, rowH = 12.2f;
  const float mid = bx + bw * 0.5f;
  g.child(box()
              .inset(0)
              .shape(segFn({mid, by - 4}, {mid, by + rowH * 10 + 4}))
              .stroke(stroke(1.0f, Fill::color(kCardInk))));
  for (size_t i = 0; i < legs.size(); ++i) {
    const float y = by + rowH * (float)i;
    const bool bad = legs[i].ratio < 0.7f || legs[i].ratio > 1.3f;
    const float dx = (legs[i].ratio - 1.0f) * bw * 0.62f;
    g.child(text(toUtf8(legs[i].name), type(faceUi, 9.5f, bad ? kAmber : kGrey))
                .at({60, y - 2})
                .key("legn" + std::to_string(i))
                .opacity(beat(tDistort + 0.1f + 0.04f * (float)i,
                              tDistort + 0.3f + 0.04f * (float)i)));
    g.child(box()
                .rect(SkRect::MakeXYWH(dx < 0 ? mid + dx : mid, y,
                                       std::max(std::fabs(dx), 1.0f), 7))
                .fill(Paint::solid(bad ? kAmber : kBlue))
                .key("legb" + std::to_string(i))
                .scale(animate(from(0.0f).to(1.0f),
                               ramp((tDistort + 0.2f) * 1000 + 70.0f * (float)i,
                                    420, ch::EaseOutBack())))
                .transformOrigin(dx < 0 ? 1.0f : 0.0f, 0.5f)
                .opacity(beat(tDistort + 0.2f + 0.05f * (float)i,
                              tDistort + 0.4f + 0.05f * (float)i)));
    g.child(text(toUtf8(kit::formatted("%.3f", legs[i].ratio)),
                 type(faceUi, 9.5f, bad ? kAmber : kGrey))
                .at({bx + bw + 20, y - 2})
                .key("legv" + std::to_string(i))
                .opacity(beat(tDistort + 0.2f + 0.04f * (float)i,
                              tDistort + 0.4f + 0.04f * (float)i)));
  }
  g.child(text(toUtf8("TOTAL 934.2 km real → 944.6 km on Minard, ratio "
                      "1.011 — one leg squeezed to 59%, the next stretched "
                      "to 153%, the total kept right,\nexactly where Wizma, "
                      "Chjat and Mojaisk crowd into 130 px of lettering.  "
                      "That the room was for the labels is an INFERENCE."),
               type(faceUi, 10, kCardInk))
              .left(Dimension(60))
              // Two lines of 10 pt under ten rows of 12.2 is what the card's
              // 206 holds: set any lower and the second line's baseline
              // falls past the card edge and the sentence is cut in half.
              .top(Dimension(by + rowH * 10 + 4))
              .width(Dimension(900))
              .key("legTotal")
              .opacity(beat(tDistort + 0.9f, tDistort + 1.3f)));
  return g;
}

auto Minard1869::cardReaumur() -> Element {
  auto g = box().inset(0);
  const float x0 = 40, y0 = 50, rowH = 15.0f;
  const char* heads[] = {"date on the plate", "°R", "°C", "°F", "days"};
  const float cols[] = {0, 250, 330, 410, 500};
  for (int c = 0; c < 5; ++c)
    g.child(text(toUtf8(heads[c]), type(faceUiBold, 9.5f, kGrey))
                .at({x0 + cols[c], y0 - 16})
                .key("rh" + std::to_string(c)));
  for (size_t i = 0; i < plate.temps.size(); ++i) {
    const Temp& t = plate.temps[i];
    const bool cold = t.reaumur <= -30.0f;
    const SkColor4f col = cold ? kBlue : kCardInk;
    const float y = y0 + rowH * (float)i;
    auto cell = [&](int c, const std::string& s, SkColor4f cc, float sz) {
      g.child(text(toUtf8(s), type(faceUi, sz, cc))
                  .at({x0 + cols[c], y})
                  .key("rc" + std::to_string(i) + "_" + std::to_string(c))
                  .opacity(beat(tReaumur + 0.05f * (float)i,
                                tReaumur + 0.25f + 0.05f * (float)i)));
    };
    cell(0,
         i == 4 ? std::string(t.label) + "   (NO DATE ENGRAVED)"
                : std::string(t.label),
         i == 4 ? kAmber : col, cold ? 11.0f : 10.0f);
    cell(1, kit::formatted("%.0f", t.reaumur), col, cold ? 11.5f : 10.0f);
    cell(2, kit::formatted("%.2f", t.reaumur * 1.25f), col,
         cold ? 11.5f : 10.0f);
    cell(3, kit::formatted("%.2f", t.reaumur * 2.25f + 32.0f), col,
         cold ? 11.5f : 10.0f);
    cell(4, i == 0 ? std::string("—") : std::to_string(t.daysSincePrev), kGrey,
         10.0f);
  }
  g.child(text(toUtf8("°C = °R × 5/4      °F = °R "
                      "× 9/4 + 32      (exact — Réaumur puts 80 "
                      "degrees between ice and steam)"),
               type(faceUi, 10, kGrey))
              .at({x0, y0 + rowH * 9 + 2})
              .key("reqs")
              .opacity(beat(tReaumur + 0.5f, tReaumur + 0.8f)));
  g.child(text(toUtf8("−30 °R = −37.50 °C = −35.50 "
                      "°F.  The plate's title says degrés du\nthermomètre "
                      "de Réaumur in display capitals, and reproductions "
                      "still\nrelabel the axis Celsius while keeping his "
                      "numbers."),
               type(faceUi, 10, kClaimRed))
              .left(Dimension(x0 + 560))
              .top(Dimension(y0 + 4))
              .width(Dimension(400))
              .key("reaWrong")
              .opacity(beat(tReaumur + 0.9f, tReaumur + 1.3f)));
  // the two campaigns, the reason the panels share a sheet
  g.child(text(toUtf8("Hannibal 218 BC    96,000 → 26,000    survived "
                      "27.08%\nNapoleon 1812     422,000 → 10,000    "
                      "survived  2.37%\nThis is why he printed them together."),
               type(faceUiBold, 12, kCardInk))
              .left(Dimension(x0 + 560))
              .top(Dimension(y0 + 74))
              .width(Dimension(420))
              .key("twoCamp")
              .opacity(beat(tTwo, tTwo + 0.5f)));
  return g;
}

auto Minard1869::auditColumn() -> Element {
  auto g = box().inset(0);
  g.child(card(128, 326, "DOES THE PLATE OBEY ITS OWN LEGEND?", "card1", tScale,
               cardScale()));
  g.child(card(466, 196, "THE FLOOR", "card2", tScale + 0.8f, cardFloor()));
  g.child(card(668, 250, "THE MAP IS A REAL MAP", "card3", tGeo, cardGeo()));
  g.child(card(926, 206, "WHAT HE DID DISTORT", "card4", tDistort, cardLegs()));
  g.child(card(1144, 204, "RÉAUMUR", "card5", tReaumur, cardReaumur()));
  return g;
}

auto Minard1869::titleStrip() -> Element {
  auto g = box().rect(SkRect::MakeXYWH(48, 28, 2464, 80));
  g.child(
      sketch::kit::titleCard(
          {.title = {toUtf8("Carte figurative des pertes successives en "
                            "hommes de l'armée française dans la campagne "
                            "de Russie 1812–1813, comparée à celle "
                            "d'Annibal durant la 2ᵉᵐᵉ guerre punique")},
           .subtitle = {toUtf8("BnF, Ge Don 4182 · lithograph · 62 × 54 "
                               "cm · Paris, 20 novembre 1869 · Minard was "
                               "88, and died ten months later during the "
                               "siege of Paris")},
           .notes = {{.words = toUtf8("the sheet is drawn at its own aspect "
                                      "— 2.258 px per millimetre of "
                                      "Minard's paper, so every band width "
                                      "on screen is a real millimetre "
                                      "count"),
                      .ink = Fill::color(hexColor(0x2f6f9c))},
                     {.words = toUtf8("THE PLATE STATES ITS OWN "
                                      "CONSTRUCTION RULE.  THIS SKETCH "
                                      "CHECKS IT — AND THEN CHECKS ITSELF "
                                      "WITH THE SAME MEASUREMENT."),
                      .ink = Fill::color(hexColor(0xb5761e))}}})
          .left(Dimension(0))
          .top(Dimension(0))
          .width(Dimension(2464)));
  return g;
}

auto Minard1869::consoleStrip() -> Element {
  feed::TextOptions s;
  s.styles = kit::tinted(faceMono, 8.2f, hexColor(0xb9b2a4),
                         {{"dim", hexColor(0x6d675c)},
                          {"pass", hexColor(0x62ab74)},
                          {"fail", hexColor(0xd08a2a)},
                          {"measured", hexColor(0x64a8d8)},
                          {"heading", hexColor(0xf0e8d8)}});
  // The heading runs a shade larger; set() replaces it where it sits.
  s.styles.set("heading",
               weave::Type{.size = 8.8f, .color = hexColor(0xf0e8d8)});
  s.window.gap = 0.0f;
  s.window.visible = 20;
  return kit::console({.feeds = {&colA, &colB, &colC, &colD, &colE},
                       .style = s,
                       .plate = {.paddingX = 8,
                                 .paddingY = 8,
                                 .gap = 12,
                                 .fill = Fill::color(hexColor(0x141311)),
                                 .border = Fill::color(hexColor(0x2c2a26)),
                                 .borderAlign = PathFormat::Align::Center,
                                 .columnExtent = 480}})
      .rect(SkRect::MakeXYWH(48, kConsoleY, 2464, kConsoleH))
      .key("console");
}
