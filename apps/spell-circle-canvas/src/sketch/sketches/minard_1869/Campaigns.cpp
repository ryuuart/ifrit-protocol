#include "Minard1869.h"

auto Minard1869::hannibalSea() -> Element {
  const std::vector<SkPoint> coast = {
      {60, 300},   {150, 318},  {200, 330},  {268, 348},  {330, 344},
      {392, 358},  {470, 344},  {540, 348},  {586, 344},  {626, 330},
      {700, 348},  {760, 372},  {800, 400},  {822, 448},  {846, 492},
      {900, 512},  {962, 528},  {1010, 542}, {1052, 528}, {1100, 512},
      {1150, 520}, {1210, 546}, {1258, 560},
  };
  // A lithographic edge is slightly ragged. One displace pass at low
  // amplitude and a long wavelength, before anything is stroked.
  const SkPath line =
      geometry::path::displace(smooth(coast), 0.9f, 90.0f, false);
  // The sea as a CLOSED region: the coast, then round the panel's own
  // south-east corner. Built by hand because there are no boolean path
  // ops here — `panelRect − land` is the natural spelling, and `.clip()`
  // only intersects, which would keep the land instead of dropping it.
  // Having the polygon, the hachures are one clipPath.
  SkPathBuilder seab;
  seab.addPath(line);
  seab.lineTo(kFrameR - 2, kDivHN - 2);
  seab.lineTo(60, kDivHN - 2);
  seab.close();
  const SkPath sea = seab.detach();

  std::vector<SkPath> rings;
  float d = 0;
  for (int i = 1; i <= 7; ++i) {
    d += 2.4f + 1.05f * (float)i;
    rings.push_back(geometry::path::parallel(line, -d, 4.0f));
  }

  auto g = box().inset(0);
  // Keyed: the two cooked paths are the whole of what the program closes
  // over, and both are a function of the coast this file states once.
  g.child(custom("seahatch",
                 [sea, rings](SkCanvas& c, const PaintContext&) {
                   SkPaint p;
                   p.setAntiAlias(true);
                   p.setStyle(SkPaint::kStroke_Style);
                   p.setStrokeWidth(0.5f);
                   c.save();
                   c.clipPath(sea, true);
                   for (size_t i = 0; i < rings.size(); ++i) {
                     p.setColor4f(hexColor(0x4e4436, 0.55f - 0.062f * (float)i),
                                  nullptr);
                     c.drawPath(rings[i], p);
                   }
                   c.restore();
                 })
              .inset(0)
              .cache(Cache::Texture)
              .key("seahatch")
              .opacity(beat(tHann + 0.25f, tHann + 1.0f)));
  // the shore itself, engraved on top
  g.child(box()
              .inset(0)
              .shape(pathFn(line))
              .stroke(spans::upTo(beat(tHann, tHann + 0.5f)),
                      stroke(1.1f, Fill::color(kInk)))
              .key("coast"));
  g.child(text(toUtf8("Iles Baléares"), type(faceItalic, 9, kInk, 0.2f))
              .at({150, 500})
              .key("baleares")
              .opacity(beat(tHann + 0.9f, tHann + 1.2f)));
  return g;
}

auto Minard1869::lehmann(const std::vector<std::array<float, 4>>& ridges,
                         float x0, float y0, float x1, float y1,
                         const char* key, float t0) -> Element {
  // copying the captures can fail only on allocation
  // NOLINTNEXTLINE(bugprone-exception-escape)
  // Keyed on the caller's own name for the field, which is what names the
  // ridge table and the four bounds the program closes over.
  return custom(key,
                [ridges, x0, y0, x1, y1](SkCanvas& c, const PaintContext&) {
                  auto height = [&](float x, float y) {
                    float h = 0;
                    for (const auto& r : ridges) {
                      const float dx = (x - r[0]) / r[2];
                      const float dy = (y - r[1]) / r[3];
                      h += std::exp(-(dx * dx + dy * dy));
                    }
                    return h;
                  };
                  SkPaint p;
                  p.setAntiAlias(true);
                  p.setStyle(SkPaint::kStroke_Style);
                  const float pitch = 4.6f;
                  // the loop walks a distance; the accumulated float is the
                  // position
                  // NOLINTNEXTLINE(clang-analyzer-security.FloatLoopCounter,bugprone-float-loop-counter)
                  for (float y = y0; y < y1; y += pitch) {
                    // the loop walks a distance; the accumulated float is the
                    // position
                    // NOLINTNEXTLINE(clang-analyzer-security.FloatLoopCounter,bugprone-float-loop-counter)
                    for (float x = x0; x < x1; x += pitch) {
                      const float h = height(x, y);
                      if (h < 0.10f) continue;
                      const float e = 1.2f;
                      const float gx =
                          (height(x + e, y) - height(x - e, y)) / (2 * e);
                      const float gy =
                          (height(x, y + e) - height(x, y - e)) / (2 * e);
                      const float slope = std::sqrt(gx * gx + gy * gy);
                      if (slope < 0.004f) continue;
                      // Lehmann: black fraction = slope/45deg, capped
                      const float k = std::min(1.0f, slope / 0.055f);
                      const float len = pitch * (0.55f + 1.35f * k);
                      const float ux = -gx / (slope + 1e-6f),
                                  uy = -gy / (slope + 1e-6f);
                      p.setStrokeWidth(0.45f + 0.75f * k);
                      p.setColor4f({kInkThin.fR, kInkThin.fG, kInkThin.fB,
                                    0.30f + 0.62f * k},
                                   nullptr);
                      c.drawLine(x - ux * len * 0.5f, y - uy * len * 0.5f,
                                 x + ux * len * 0.5f, y + uy * len * 0.5f, p);
                    }
                  }
                })
      .inset(0)
      .cache(Cache::Texture)
      .key(key)
      .opacity(beat(t0, t0 + 0.55f));
}

auto Minard1869::hannibalPanel() -> Element {
  auto g = box().inset(0);

  g.child(text(toUtf8("Carte Figurative des pertes successives en hommes de "
                      "l'armée qu'Annibal conduisit d'Espagne"),
               type(faceScript, 19, kInk, 0.2f))
              .at({210, 58})
              .key("htitle1")
              .opacity(beat(tHann, tHann + 0.4f)));
  g.child(text(toUtf8("en Italie en traversant les Gaules (selon Polybe)."),
               type(faceScript, 17, kInk, 0.2f))
              .at({320, 84})
              .key("htitle2")
              .opacity(beat(tHann + 0.1f, tHann + 0.5f)));
  g.child(text(toUtf8("Dressée par M. Minard, Inspecteur Général "
                      "des Ponts et Chaussées en retraite."),
               type(faceScript, 14, kInk, 0.15f))
              .at({300, 110})
              .key("htitle3")
              .opacity(beat(tHann + 0.2f, tHann + 0.6f)));
  g.child(text(toUtf8("Paris, le 20 Novembre 1869."),
               type(faceScript, 14, kInk, 0.15f))
              .at({700, 132})
              .key("htitle4")
              .opacity(beat(tHann + 0.3f, tHann + 0.7f)));

  g.child(hannibalSea());

  // The Pyrenees and the Alps, hachured by Lehmann's rule.
  g.child(lehmann({{{562, 302, 58, 15}}, {{604, 288, 50, 13}}}, 440, 250, 720,
                  350, "pyrenees", tHann + 0.55f));
  g.child(lehmann({{{1046, 336, 52, 30}},
                   {{1086, 382, 62, 34}},
                   {{1002, 296, 40, 22}},
                   {{1128, 432, 54, 30}},
                   {{1064, 476, 72, 26}},
                   {{1176, 392, 44, 24}}},
                  900, 220, 1290, 570, "alps", tHann + 0.7f));

  // rivers, in a lighter sloped hand
  auto river = [&](const std::vector<SkPoint>& pts, const char* k, float t0) {
    g.child(box()
                .inset(0)
                .shape(pathFn(smooth(pts)))
                .stroke(spans::upTo(beat(t0, t0 + 0.4f)),
                        stroke(0.8f, Fill::color(hexColor(0x4e4436, 0.8f))))
                .key(k));
  };
  river({{206, 190}, {198, 240}, {212, 280}, {200, 322}}, "ebre", tHann + 0.9f);
  river({{796, 150}, {806, 210}, {790, 268}, {800, 320}, {780, 358}}, "rhone",
        tHann + 0.95f);
  river({{910, 236}, {930, 268}, {952, 286}}, "isere", tHann + 1.0f);
  river({{1040, 300}, {1010, 330}, {980, 350}}, "arc", tHann + 1.0f);
  river({{1160, 430}, {1200, 452}, {1250, 462}}, "po", tHann + 1.05f);

  // THE BAND — brush::Ribbon on the width Profile seam, over the plate's
  // own strengths. This is the primitive the whole sheet is made of.
  const SkPath spine = polylineH(plate.hannibal);
  const WidthProfile prof = profileOfH(plate.hannibal);
  g.child(bandElement(spine, prof, kZone, "hband",
                      beat(tHann + 0.55f, tHann + 1.6f)));

  // the ten numbers, written ACROSS the zones ("écrits en travers"),
  // Orient::Tangent on a cross-segment
  for (size_t i = 0; i + 1 < plate.hannibal.size(); ++i) {
    if (i > 0 && plate.hannibal[i].men == plate.hannibal[i - 1].men) continue;
    g.child(bandNumber({plate.hannibal[i].x, plate.hannibal[i].y},
                       {plate.hannibal[i + 1].x - plate.hannibal[i].x,
                        plate.hannibal[i + 1].y - plate.hannibal[i].y},
                       plate.hannibal[i].men, 8.2f, "hn" + std::to_string(i),
                       tHann + 0.7f + 0.06f * (float)i));
  }

  // the place names
  for (size_t i = 0; i < plate.places.size(); ++i) {
    const Place& p = plate.places[i];
    sigil::weave::TextStyle st =
        p.kind == 0   ? type(faceRoman, 11, kInk, 2.6f)
        : p.kind == 3 ? type(faceItalic, 10, hexColor(0x4e4436), 1.2f)
                      : type(faceItalic, 9, kInk, 0.2f);
    g.child(text(toUtf8(p.name), st)
                .at({p.x, p.y})
                .key("hp" + std::to_string(i))
                .opacity(beat(tHann + 1.0f + 0.012f * (float)i,
                              tHann + 1.4f + 0.012f * (float)i)));
  }

  // The LEGEND BOX — top right, and it is the second, independent
  // statement of the same scale rule.
  g.child(legendBox());
  // the scale bar, in the panel's OWN lieue (4,560 m — not the Russian
  // panel's 4,444.8 m; the two panels use different lieues)
  g.child(scaleBar(640, 500, 14.09f / 3.0f, 30, 5, "Lieues de 4.560 m", "hbar",
                   tHann + 1.45f));
  // the compass arrow in the Mediterranean
  g.child(box()
              .inset(0)
              .shape(segFn({846, 486}, {900, 424}))
              .stroke(spans::upTo(beat(tHann + 1.5f, tHann + 1.75f)),
                      lines::presets::arrow(1.2f, Fill::color(kInk), 9.0f))
              .key("compass"));
  return g;
}

auto Minard1869::legendBox() -> Element {
  const float l = 1010, t = 52, w = 344, h = 108;
  auto g = box()
               .rect(SkRect::MakeXYWH(l, t, w, h))
               .shape(pathFn(rectPath(0, 0, w, h)))
               .stroke(stroke(1.0f, Fill::color(kInk)))
               .key("legendbox")
               .opacity(beat(tHann + 1.5f, tHann + 1.8f));
  g.child(text(toUtf8("Légende."), type(faceScript, 14, kInk)).at({140, 4}));
  const char* lines_[] = {
      "Les nombres d'hommes restés à Annibal sont "
      "représentés",
      "par la largeur des zônes colorées à raison d'un "
      "millimètre",
      "pour dix mille hommes, ils sont de plus écrits en travers des "
      "zônes.",
      "Il n'y a pas d'opinion arrêtée sur le point où Annibal "
      "a",
      "traversé les Alpes, j'ai adopté celle de Larosa sans "
      "prétendre la justifier.",
  };
  for (int i = 0; i < 5; ++i)
    g.child(text(toUtf8(lines_[i]), type(faceScript, 9.6f, kInk, 0.05f))
                .at({8, 24 + 16.0f * (float)i}));
  return g;
}

auto Minard1869::flowRibbon(const WidthProfile& prof, SkColor4f colour)
    -> brush::Ribbon {
  brush::Ribbon r =
      brush::ribbon(FlowWidth{prof, &mmScale}, Fill::color(colour));
  r.step = 2.0f;
  r.join = SkPaint::kBevel_Join;
  return r;
}

auto Minard1869::bandElement(const SkPath& spine, const WidthProfile& prof,
                             SkColor4f colour, const std::string& key,
                             Animatable<float> reveal) -> Element {
  // `pathFigure` is the route in its own bounding box: the node's rect
  // is the route's bounds and the shape is the route re-based into it.
  // No bleed — the band overflows that box by up to w/2 on each side by
  // design, which is what makes the profile's max() load bearing.
  //
  // The profile reads a LIVE Output (the 12.6% morph), which the
  // reconciler cannot see change — hence Cache::None. See FlowWidth.
  return pathFigure(spine)
      .stroke(spans::upTo(std::move(reveal)), flowRibbon(prof, colour))
      .cache(Cache::None)
      .key(key);
}

auto Minard1869::bandNumber(SkPoint at, SkVector tangent, float men, float size,
                            const std::string& key, float t0) -> Element {
  const float L = std::hypot(tangent.x(), tangent.y());
  SkVector t =
      L > 0 ? SkVector{tangent.x() / L, tangent.y() / L} : SkVector{1, 0};
  SkVector n{-t.y(), t.x()};
  if (n.y() > 0) {  // make the type read bottom-up, as on the plate
    n = {-n.x(), -n.y()};
  }
  const auto style = type(faceNum, size, kInk, 0.2f);
  float runLen = 0;
  float slack = size * 0.3f;  // metrics-free fallback, same shape
  if (fonts) {
    runLen = runPens(toUtf8(french(men)), style, *fonts).back();
    slack = metrics(style, *fonts).capSlack();
  }
  const float half = std::max(bandPx(men) * 0.5f, runLen * 0.5f) + slack;
  const SkPoint a{at.x() - n.x() * half, at.y() - n.y() * half};
  const SkPoint b{at.x() + n.x() * half, at.y() + n.y() * half};
  return text(toUtf8(french(men)), style)
      .rect(SkRect::MakeXYWH(0, 0, kSheetW, kSheetH))
      .onPath(TextPath{.path = segFn(a, b),
                       .at = 0.5f,
                       .align = TextPath::Align::Center,
                       .offset = 0.0f,
                       .autoFlip = false,
                       .orient = TextPath::Orient::Tangent})
      .key(key)
      .opacity(beat(t0, t0 + 0.3f));
}

auto Minard1869::advanceZones() -> Element {
  auto zone = [this](const std::vector<Station>& st, const std::string& key,
                     Animatable<float> reveal) {
    return bandElement(polyline(st), profileOf(st), kZone, key,
                       std::move(reveal));
  };
  return box()
      .inset(0)
      .child(zone(plate.advTrunk, "advTrunk", beat(tAdv, tAdv + 1.6f)))
      .child(zone(plate.advNorth, "advNorth", beat(tAdv + 0.35f, tAdv + 0.8f)))
      .child(zone(plate.advPolotzk, "advPol", beat(tAdv + 0.55f, tAdv + 1.2f)));
}

auto Minard1869::napoleonPanel(sketch::SketchContext& ctx) -> Element {
  auto g = box().inset(0);

  g.child(text(toUtf8("Carte Figurative des pertes successives en hommes de "
                      "l'Armée Française dans la campagne de Russie "
                      "1812—1813."),
               type(faceScript, 18, kInk, 0.15f))
              .at({200, kDivHN + 14})
              .key("ntitle")
              .opacity(beat(tLegend, tLegend + 0.3f)));
  g.child(text(toUtf8("Dressée par M. Minard, Inspecteur Général "
                      "des Ponts et Chaussées en retraite.        Paris, "
                      "le 20 Novembre 1869."),
               type(faceScript, 13, kInk, 0.1f))
              .at({300, kDivHN + 40})
              .key("ntitle2")
              .opacity(beat(tLegend + 0.1f, tLegend + 0.4f)));

  // the legend as a PARAGRAPH, which is what it is — not a key.
  for (int i = 0; i < 5; ++i) {
    g.child(
        text(toUtf8(plate.legend[i]), type(faceScript, 9.8f, kInk, 0.02f))
            .at({i == 3 ? 148.0f : 128.0f, kDivHN + 58 + 14.6f * (float)i})
            .key("nleg" + std::to_string(i))
            .mask(by::edge(0.0f, beat(tLegend + 0.25f + 0.16f * (float)i,
                                      tLegend + 0.55f + 0.16f * (float)i))));
  }

  // the rivers of the Russian panel
  auto river = [&](const std::vector<SkPoint>& pts, const char* label,
                   SkPoint lp, const char* k, float t0) {
    g.child(box()
                .inset(0)
                .shape(pathFn(smooth(pts)))
                .stroke(spans::upTo(beat(t0, t0 + 0.35f)),
                        stroke(0.7f, Fill::color(hexColor(0x4e4436, 0.85f))))
                .key(k));
    g.child(text(toUtf8(label), type(faceItalic, 8, hexColor(0x4e4436), 0.6f))
                .at({lp.x(), lp.y()})
                .key(std::string(k) + "L")
                .opacity(beat(t0 + 0.2f, t0 + 0.5f)));
  };
  river({{mapX(23.7f), mapY(56.0f)},
         {mapX(23.95f), mapY(55.3f)},
         {mapX(23.8f), mapY(54.8f)},
         {mapX(24.0f), mapY(54.2f)},
         {mapX(23.9f), mapY(53.8f)}},
        "Niémen R.", {mapX(23.45f), 774.0f}, "rNiemen", tAdv - 0.2f);
  river({{mapX(28.6f), mapY(54.9f)},
         {mapX(28.45f), mapY(54.5f)},
         {mapX(28.6f), mapY(54.1f)},
         {mapX(28.4f), mapY(53.7f)}},
        "Bérézina R.", {mapX(28.2f), mapY(54.72f)}, "rBerez", tAdv - 0.15f);
  river({{mapX(31.2f), mapY(54.05f)},
         {mapX(30.9f), mapY(54.5f)},
         {mapX(31.05f), mapY(54.95f)},
         {mapX(30.7f), mapY(55.4f)}},
        "Dniéper R.", {mapX(30.95f), mapY(54.35f)}, "rDniepr", tAdv - 0.1f);
  river({{mapX(36.6f), mapY(56.05f)},
         {mapX(36.9f), mapY(55.7f)},
         {mapX(37.3f), mapY(55.45f)}},
        "Moskowa R.", {mapX(36.35f), mapY(55.95f)}, "rMoskowa", tAdv - 0.05f);

  // --- THE ADVANCE ------------------------------------------------------
  // The red-brown is a SEPARATE STONE from the black, so it is very
  // slightly out of register. One translate, and it is the single most
  // convincing "this is a lithograph" cue on the plate.
  auto redStone = box().inset(0).translateX(0.4f).translateY(-0.3f);
  // The zones read the 12.6% morph the way every other band on the sheet
  // does: through the width law, at paint. Nothing here re-describes when
  // it moves.
  redStone.child(advanceZones());
  // the stone took unevenly: a very low-amplitude speckle in the zone
  // colour, NOT a gradient (the Commons p10/p90 are two units apart)
  redStone.child(box()
                     .inset(0)
                     .fill(tintSpeckle.material())
                     .blend(SkBlendMode::kMultiply)
                     .opacity(0.06f)
                     .cache(Cache::Texture)
                     .key("tintwander"));
  g.child(std::move(redStone));

  // --- THE RETREAT ------------------------------------------------------
  g.child(bandElement(polyline(plate.retEast), profileOf(plate.retEast),
                      kInkDeep, "retEast", beat(tRet, tRet + 0.9f)));
  g.child(bandElement(polyline(plate.retPolotzk), profileOf(plate.retPolotzk),
                      kInkDeep, "retPol", beat(tRet + 0.7f, tRet + 1.0f)));
  g.child(bandElement(polyline(plate.retWest), profileOf(plate.retWest),
                      kInkDeep, "retWest", beat(tRet + 0.85f, tRet + 1.7f)));
  g.child(bandElement(polyline(plate.retNorth), profileOf(plate.retNorth),
                      kInkDeep, "retNorth", beat(tRet + 1.5f, tRet + 1.8f)));

  // The arithmetic of the splits, as a footnote row along the bottom of
  // the map panel — five identities, all exact, on numbers Minard
  // engraved, and they need no measurement at all.
  {
    const char* ident[] = {"422 − 22 = 400", "400 − 60 = 340",
                           "20 + 30 = 50  (Bobr)",
                           // one caption, split across adjacent literals
                           // NOLINTNEXTLINE(bugprone-suspicious-missing-comma)
                           "50 − 28 = 22,000 in four days  (the "
                           "Berezina)",
                           "4 + 6 = 10 recrossed"};
    float x = kFrameL + 16;
    for (int i = 0; i < 5; ++i) {
      g.child(text(toUtf8(ident[i]), type(faceUiBold, 9.5f, kBlue))
                  .at({x, kDivNT - 26})
                  .key("ar" + std::to_string(i))
                  .opacity(beat(tAdv + 0.5f + 0.35f * (float)i,
                                tAdv + 0.75f + 0.35f * (float)i)));
      x += 20.0f + 6.4f * (float)std::char_traits<char>::length(ident[i]);
    }
    g.child(text(toUtf8("all five EXACT"), type(faceUiBold, 9.5f, kPass))
                .at({x, kDivNT - 26})
                .key("arok")
                .opacity(beat(tRet + 1.8f, tRet + 2.1f)));
  }

  // --- the engraved numbers --------------------------------------------
  auto numbersFor = [&](const std::vector<Station>& st, const char* tag,
                        float t0, float dt) {
    for (size_t i = 0; i + 1 < st.size(); ++i) {
      if (i > 0 && st[i].men == st[i - 1].men) continue;
      const SkPoint a = stationPt(st[i]), b = stationPt(st[i + 1]);
      g.child(bandNumber({(a.x() + b.x()) * 0.5f, (a.y() + b.y()) * 0.5f},
                         {b.x() - a.x(), b.y() - a.y()}, st[i].men, kNumSize,
                         tag + std::to_string(i), t0 + dt * (float)i));
    }
  };
  numbersFor(plate.advTrunk, "nA", tAdv + 0.4f, 0.07f);
  numbersFor(plate.advNorth, "nB", tAdv + 0.5f, 0.05f);
  numbersFor(plate.advPolotzk, "nC", tAdv + 0.8f, 0.05f);
  numbersFor(plate.retEast, "nD", tRet + 0.2f, 0.06f);
  numbersFor(plate.retWest, "nE", tRet + 0.9f, 0.06f);
  numbersFor(plate.retPolotzk, "nF", tRet + 0.8f, 0.05f);
  // what recrossed the Niemen
  g.child(bandNumber({mapX(23.95f), mapY(54.4f)}, {1, 0}, 10000, kNumSize, "nG",
                     tRet + 1.7f));

  // --- the place names --------------------------------------------------
  for (size_t i = 0; i < plate.cities.size(); ++i) {
    const City& c = plate.cities[i];
    const bool moscou = c.plate == "Moscou";
    // MOSCOU alone is set in spaced roman capitals, and it is the only
    // word on the map that is.
    Element e = moscou
                    ? text(toUtf8("MOSCOU"), type(faceRoman, 13, kInk, 2.2f))
                          .textStroke(0.5f, Fill::color(kInk))
                    : text(toUtf8(c.plate), type(faceItalic, 9.6f, kInk, 0.2f));
    g.child(e.at({mapX(c.lon) + c.dx, mapY(c.lat) + c.dy})
                .key("city" + std::to_string(i))
                .opacity(beat(tAdv + 0.1f + 0.03f * (float)i,
                              tAdv + 0.4f + 0.03f * (float)i)));
  }

  // THE FLOOR, drawn on the plate itself: the blue outline is the width
  // Minard's crayon actually laid at the last treads (5.4 px on the
  // Commons scan = 3.54 px here), against the black band this sketch
  // draws from the rule. The last 4,000 men are 2.6x over.
  {
    const float floorPx = 5.4f * 0.6549f;
    SkPathBuilder fb;
    const float y = mapY(54.4f);
    fb.addRect(SkRect::MakeLTRB(mapX(24.1f), y - floorPx * 0.5f, mapX(25.0f),
                                y + floorPx * 0.5f));
    g.child(box()
                .inset(0)
                .shape(pathFn(fb.detach()))
                .stroke(stroke(0.9f, Fill::color(kBlue)))
                .key("floorink")
                .opacity(beat(tScale + 1.4f, tScale + 1.7f)));
    g.child(text(toUtf8("what the crayon actually laid — 1.57 mm, "
                        "2.6× the rule"),
                 type(faceUi, 9, kBlue))
                .at({mapX(24.1f), y + 10})
                .key("floorinklab")
                .opacity(beat(tScale + 1.5f, tScale + 1.8f)));
  }

  // THE CALIPER LIES. Minard's own bar, read against Minard's own map.
  g.child(text(toUtf8("Minard's own bar reads Kowno→Smolensk as 210 "
                      "lieues = 933 km.  The truth is 520 km.  ×1.79 — "
                      "UNEXPLAINED"),
               type(faceUiBold, 10.0f, kAmber))
              .at({880, 958})
              .key("barlies")
              .opacity(beat(tBar, tBar + 0.4f)));
  g.child(text(toUtf8("hypotheses: the labels are half their true value · "
                      "copied unrescaled from Fezensac · my longitude "
                      "scale is wrong.  None asserted."),
               type(faceUi, 9.0f, hexColor(0xb5761e, 0.9f)))
              .at({880, 972})
              .key("barhyp")
              .opacity(beat(tBar + 0.4f, tBar + 0.8f)));

  // the lieue bar, and its ticks
  g.child(scaleBar(mapX(33.4f), 930.0f, 4.985f * 0.6549f, 50, 5,
                   "Lieues communes de France (Carte de M. de Fezensac)",
                   "nbar", tAdv + 1.7f));
  return g;
}

auto Minard1869::scaleBar(float x, float y, float pxPerUnit, int span, int step,
                          const char* label, const char* key, float t0)
    -> Element {
  auto g = box().inset(0).key(key).opacity(beat(t0, t0 + 0.3f));
  const float w = pxPerUnit * (float)span;
  g.child(box()
              .inset(0)
              .shape(segFn({x, y}, {x + w, y}))
              .stroke(stroke(0.9f, Fill::color(kInk))));
  for (int v = 0; v <= span; v += step) {
    const float tx = x + pxPerUnit * (float)v;
    if (v > 25 && v < span)
      continue;  // the plate's own graduation: 0 5 10 15 20 25 ...... 50
    g.child(box().inset(0).shape(segFn({tx, y - 4}, {tx, y})));
    g.child(text(toUtf8(std::to_string(v)), type(faceNum, 6.5f, kInk))
                .at({tx - 3, y + 2}));
  }
  // the ticks as one stroked path so they are one node
  {
    SkPathBuilder tb;
    for (int v = 0; v <= span; v += step) {
      if (v > 25 && v < span) continue;
      const float tx = x + pxPerUnit * (float)v;
      tb.moveTo(tx, y - 4);
      tb.lineTo(tx, y);
    }
    g.child(box()
                .inset(0)
                .shape(pathFn(tb.detach()))
                .stroke(stroke(0.9f, Fill::color(kInk))));
  }
  g.child(
      text(toUtf8(label), type(faceItalic, 7.5f, kInk, 0.1f)).at({x, y - 18}));
  return g;
}

auto Minard1869::temperaturePanel() -> Element {
  auto g = box().inset(0);
  g.child(text(toUtf8("TABLEAU GRAPHIQUE de la température en degrés "
                      "du thermomètre de Réaumur au dessous de "
                      "zéro."),
               type(faceScript, 13, kInk, 0.3f))
              .at({300, kDivNT + 4})
              .key("tempTitle")
              .opacity(beat(tTemp, tTemp + 0.3f)));

  // the INVERTED axis: 0 at the top, 30 degrés at the bottom
  for (int r = 0; r <= 30; r += 5) {
    const float y = tempY((float)-r);
    g.child(box()
                .inset(0)
                .shape(segFn({kFrameL, y}, {kFrameR - 34, y}))
                .stroke(spans::upTo(beat(tTemp + 0.1f + 0.03f * (float)r,
                                         tTemp + 0.45f + 0.03f * (float)r)),
                        stroke(0.4f, Fill::color(hexColor(0x4e4436, 0.45f))))
                .key("taxis" + std::to_string(r)));
    g.child(text(toUtf8(r == 30 ? "30 degrés" : std::to_string(r)),
                 type(faceNum, 7, kInk))
                .at({kFrameR - 30, y - 4})
                .key("tlab" + std::to_string(r))
                .opacity(beat(tTemp + 0.15f + 0.03f * (float)r,
                              tTemp + 0.5f + 0.03f * (float)r)));
  }

  // the curve, and the fine ticks hatched UNDER it (not a fill)
  std::vector<SkPoint> curve;
  curve.reserve(std::size(plate.temps));
  for (const Temp& t : plate.temps)
    curve.push_back({mapX(t.lon), tempY(t.reaumur)});
  SkPathBuilder cb;
  for (size_t i = 0; i < curve.size(); ++i)
    i == 0 ? cb.moveTo(curve[i]) : cb.lineTo(curve[i]);
  const SkPath curvePath = cb.detach();
  g.child(box()
              .inset(0)
              .shape(pathFn(curvePath))
              .stroke(stroke(1.2f, Fill::color(kInk)))
              // right to left, the way the retreat runs
              .mask(by::edge(180.0f, beat(tTemp + 0.4f, tTemp + 1.1f)))
              .key("tcurve"));
  // the hatched underside: short ticks hanging off the curve
  g.child(custom("thatch",
                 [curvePath](SkCanvas& c, const PaintContext&) {
                   SkPaint p;
                   p.setAntiAlias(true);
                   p.setStyle(SkPaint::kStroke_Style);
                   p.setStrokeWidth(0.55f);
                   p.setColor4f(hexColor(0x38301f, 0.9f), nullptr);
                   SkContourMeasureIter it(curvePath, false);
                   while (sk_sp<SkContourMeasure> m = it.next()) {
                     const float len = m->length();
                     // the loop walks a distance; the accumulated float is
                     // the position
                     // NOLINTNEXTLINE(clang-analyzer-security.FloatLoopCounter,bugprone-float-loop-counter)
                     for (float d = 0; d < len; d += 3.0f) {
                       SkPoint q;
                       SkVector tn;
                       if (!m->getPosTan(d, &q, &tn)) continue;
                       c.drawLine(q.x(), q.y(), q.x() - 1.4f, q.y() + 5.0f, p);
                     }
                   }
                 })
              .inset(0)
              .cache(Cache::Texture)
              .key("thatch")
              .opacity(beat(tTemp + 0.6f, tTemp + 1.2f)));

  // THE DROPLINES. Nine of them, from the retreat band down through the
  // divider into the graph. They are the joint between the two panels
  // and they are the whole design. Nothing declares that the two panels
  // share an abscissa: the lock is that both call the same mapX(lon).
  for (size_t i = 0; i < plate.temps.size(); ++i) {
    const float x = mapX(plate.temps[i].lon);
    SkPathBuilder d;
    d.moveTo(x, mapY(54.3f));
    d.lineTo(x, tempY(plate.temps[i].reaumur));
    PathFormat f;
    f.width = 0.7f;
    // the rule fades as it crosses the panel divider
    f.strokeFill = Paint::linearUnit({0, 0}, {0, 1},
                                     {{0.0f, hexColor(0x4e4436, 0.80f)},
                                      {0.66f, hexColor(0x4e4436, 0.22f)},
                                      {1.0f, hexColor(0x4e4436, 0.75f)}});
    g.child(box()
                .inset(0)
                .shape(pathFn(d.detach()))
                .stroke(spans::upTo(beat(tTemp + 0.25f + 0.05f * (float)i,
                                         tTemp + 0.55f + 0.05f * (float)i)),
                        f)
                .key("drop" + std::to_string(i)));

    // the annotation, as engraved. 8bre / 9bre / Xbre are October /
    // November / December — the old Roman-calendar notation, and a
    // caption that "corrects" Xbre to 10bre is wrong twice over.
    g.child(text(toUtf8(plate.temps[i].label), type(faceNum, 7.4f, kInk, 0.1f))
                .at({x - 26, tempY(plate.temps[i].reaumur) + 5})
                .key("tann" + std::to_string(i))
                .opacity(beat(tTemp + 0.5f + 0.06f * (float)i,
                              tTemp + 0.8f + 0.06f * (float)i)));
  }
  // the undated −11°, and its two independent recoveries
  g.child(text(toUtf8("24 novembre  (derived, days column)"),
               type(faceUi, 8, kBlue))
              .at({mapX(29.2f) - 26, tempY(-11) + 16})
              .key("recov1")
              .opacity(beat(tTemp + 1.35f, tTemp + 1.6f)));
  g.child(text(toUtf8("25 novembre  (derived, lon interpolation)"),
               type(faceUi, 8, kBlue))
              .at({mapX(29.2f) - 26, tempY(-11) + 27})
              .key("recov2")
              .opacity(beat(tTemp + 1.5f, tTemp + 1.75f)));

  g.child(text(toUtf8("Les Cosaques passent au galop\nle Niémen gelé."),
               type(faceScript, 10, kInk, 0.1f))
              .at({kFrameL + 20, kDivNT + 40})
              .key("cosaques")
              .opacity(beat(tTemp + 1.1f, tTemp + 1.5f)));
  return g;
}

auto Minard1869::imprints() -> Element {
  auto g = box().inset(0);
  g.child(text(toUtf8("Autog. par Regnier, 8. Pas. Sᵗᵉ Marie Sᵗ "
                      "Gᵃᵉᵐ à Paris."),
               type(faceItalic, 7, kInk))
              .at({kFrameL + 6, kFrameB + 6})
              .key("imp1")
              .opacity(beat(1.0f, 1.3f)));
  g.child(
      text(toUtf8("Imp. Lith. Regnier et Dourdet"), type(faceItalic, 7, kInk))
          .at({kFrameR - 140, kFrameB + 6})
          .key("imp2")
          .opacity(beat(1.0f, 1.3f)));
  return g;
}
