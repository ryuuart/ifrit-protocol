#include "Minard1869.h"

auto Minard1869::hand(const std::string& named) const -> sk_sp<SkTypeface> {
  if (named == "ui") return faceUi;
  if (named == "bold") return faceUiBold;
  if (named == "italic") return faceItalic;
  if (named == "num") return faceNum;
  if (named == "roman") return faceRoman;
  return nullptr;
}

auto Minard1869::engraved(const Lettering& line) const -> Element {
  weave::Type st{.size = line.size, .track = line.track};
  if (!line.face.empty()) st.face = hand(line.face);
  Element e = text(line.words)
                  .font(st)
                  .at({line.x, line.y})
                  .styleClass(line.style)
                  .key(line.key);
  if (line.width > 0) e.width(line.width);
  const float t1 = line.t1 > line.t0 ? line.t1 : line.t0 + 0.4f;
  // A hand written across the margin is WIPED on rather than faded in:
  // the pen travels, so the beat runs along the words.
  return line.wipe ? e.mask(by::edge(0.0f, beat(line.t0, t1)))
                   : e.opacity(beat(line.t0, t1));
}

auto Minard1869::lettering(const data::Json& run, float dy, float t0) const
    -> std::vector<Element> {
  return each(run.items(), [this, dy, t0](const data::Json& n) -> Element {
    Lettering line = ::lettering(n, dy);
    line.t0 += t0;
    line.t1 = line.t1 > 0 ? line.t1 + t0 : 0;
    return engraved(line);
  });
}

auto Minard1869::doc() const -> const data::Json& {
  static const data::Json none;
  return plate.words ? *plate.words : none;
}

auto Minard1869::word(const char* panel, const char* named) const
    -> std::string {
  return std::string(doc()[panel][named].text());
}

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
  // the two printed frames are ONE rule around each panel, drawn as a
  // double rule the way the plate cuts them; the map | temperature
  // divider is a single one
  struct Printed {
    SkRect at;
    const char* key;
    float t0, t1;
    int parallels;
  };
  const std::array<Printed, 3> ruled{
      {{{kFrameL, kFrameT, kFrameR, kDivHN}, "frameH", 0.25f, 1.1f, 2},
       {{kFrameL, kDivHN + 4, kFrameR, kFrameB}, "frameN", 0.35f, 1.2f, 2},
       {{kFrameL, kDivNT, kFrameR, kDivNT}, "divNT", 0.4f, 1.2f, 1}}};
  return box().inset(0).children(
      {each(ruled, [this](const Printed& p) -> Element {
        return inked(rectPath(p.at.left(), p.at.top(), p.at.right(),
                              p.at.bottom()),
                     lines::Line{.width = p.parallels == 2 ? 1.1f : 1.0f,
                                 .fill = Fill::color(kInk),
                                 .parallels = p.parallels,
                                 .gap = 3.0f},
                     p.t0, p.t1)
            .key(p.key);
      })});
}

auto Minard1869::provenance() -> Element {
  // The two donation stamps: a ruled oval with the words inside it,
  // stamped on with the overshoot an inked press leaves.
  auto stamp = [this](const data::Json& n) {
    const float r = (float)n["r"].number(26.0);
    const float t0 = (float)n["t0"].number();
    return box()
        .width(2 * r)
        .height(1.32f * r)
        .at({(float)n["x"].number() - r, (float)n["y"].number() - r * 0.66f})
        .shape(shapes::circle())
        .stroke(stroke(1.5f, Fill::color(kStampRed)))
        .children({text(n["words"])
                       .font({.face = faceRoman,
                              .size = 7.5f,
                              .color = kStampRed,
                              .track = 0.3f})
                       .at({r * 0.35f, r * 0.42f})})
        .key(std::string(n["key"].text()))
        .scale(animate(from(0.0f).to(1.0f),
                       ramp(t0 * 1000, 420, ch::EaseOutBack())))
        .opacity(beat(t0, t0 + 0.2f));
  };
  const data::Json& stamps = doc()["stamps"];
  // Minard's own hand across the top margin, and the two stamps. This is
  // the part of the object that makes it HIS copy.
  return box()
      .inset(0)
      .ink(kManuscript)
      .children({lettering(doc()["provenance"], 0.0f, 0.0f),
                 each(stamps.items(), stamp)});
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
  // The jaws: two cheeks the width of the reading apart, and the beam
  // that closes them.
  SkPathBuilder jaw;
  jaw.moveTo(r.x - 16, r.y - r.halfPx);
  jaw.lineTo(r.x + 16, r.y - r.halfPx);
  jaw.moveTo(r.x - 16, r.y + r.halfPx);
  jaw.lineTo(r.x + 16, r.y + r.halfPx);
  jaw.moveTo(r.x + 12, r.y - r.halfPx);
  jaw.lineTo(r.x + 12, r.y + r.halfPx);
  // The reading, ranged down the sheet's left margin under the jaws: four
  // lines of one instrument, so they stand in one column at one gap.
  return box()
      .inset(0)
      .key("caliperGrp")
      .opacity(&calAlpha)
      .ink(kBlue)
      .children(
          {inked(jaw.detach(), stroke(2.0f, Fill::currentInk()))
               .background(
                   shadow(hexColor(0x000000, 0.30f), {1.5f, 2.0f}, 3.0f))
               .key("jaw1"),
           box()
               .column()
               .at({kFrameL + 10, 686.0f})
               .width(420)
               .gap(4)
               .font({.face = faceUi})
               .key("calread")
               .children(
                   {text(kit::formatted("%.2f mm", r.mm))
                        .font({.face = faceUiBold, .size = 17}),
                    text(kit::formatted("÷ %.0f = %.4f mm / 10.000", r.men,
                                        r.mm / (r.men / 10000.0f)))
                        .font({.size = 9.5f}),
                    text(std::string(r.where) + "\n" +
                         word("caliper", "provenance"))
                        .font({.size = 9, .color = hexColor(0x2f6f9c, 0.9f)}),
                    text(word("caliper", "claim"))
                        .font({.face = faceUiBold,
                               .size = 10,
                               .color = kClaimRed})})});
}

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

  Lettering shore = ::lettering(doc()["hannibal"]["sea"]);
  shore.t0 += tHann;
  // Keyed: the two cooked paths are the whole of what the program closes
  // over, and both are a function of the coast this file states once.
  // Coast-parallel hatching whose spacing GROWS away from the shore is
  // geometry::path::parallel called once per ring, so the pen only has to
  // clip to the water and walk them.
  return box().inset(0).children(
      {pen(
           "seahatch",
           [sea, rings](Pen& p) {
             p.noFill();
             p.strokeWeight(0.5f);
             p.clip([&p, &sea] { p.shape(sea); });
             for (size_t i = 0; i < rings.size(); ++i) {
               p.stroke(hexColor(0x4e4436, 0.55f - 0.062f * (float)i));
               p.shape(rings[i]);
             }
           },
           Cache::Texture)
           .inset(0)
           .key("seahatch")
           .opacity(beat(tHann + 0.25f, tHann + 1.0f)),
       // the shore itself, engraved on top
       inked(line, stroke(1.1f, Fill::color(kInk)), tHann, tHann + 0.5f)
           .key("coast"),
       engraved(shore)});
}

auto Minard1869::lehmann(const std::vector<std::array<float, 4>>& ridges,
                         float x0, float y0, float x1, float y1,
                         const char* key, float t0) -> Element {
  // Lehmann hachures (Johann Georg Lehmann, 1799): strokes down the line
  // of steepest descent, black-to-white ratio proportional to slope — all
  // white at 0°, all black at 45°. Every stroke's direction, length,
  // weight and alpha come from the local gradient of a synthetic field of
  // gaussian ridges, so this is a FIELD rather than a repeated motif and
  // patterns::stripes cannot express it.
  //
  // Keyed on the caller's own name for the field, which is what names the
  // ridge table and the four bounds the program closes over.
  // copying the captures can fail only on allocation
  // NOLINTNEXTLINE(bugprone-exception-escape)
  return pen(
             key,
             [ridges, x0, y0, x1, y1](Pen& p) {
               auto height = [&](float x, float y) {
                 float h = 0;
                 for (const auto& r : ridges) {
                   const float dx = (x - r[0]) / r[2];
                   const float dy = (y - r[1]) / r[3];
                   h += std::exp(-(dx * dx + dy * dy));
                 }
                 return h;
               };
               p.noFill();
               const float pitch = 4.6f;
               // the loops walk a distance; the accumulated float is the
               // position
               // NOLINTNEXTLINE(clang-analyzer-security.FloatLoopCounter,bugprone-float-loop-counter)
               for (float y = y0; y < y1; y += pitch) {
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
                   p.strokeWeight(0.45f + 0.75f * k);
                   p.stroke({kInkThin.fR, kInkThin.fG, kInkThin.fB,
                             0.30f + 0.62f * k});
                   p.line(x - ux * len * 0.5f, y - uy * len * 0.5f,
                          x + ux * len * 0.5f, y + uy * len * 0.5f);
                 }
               }
             },
             Cache::Texture)
      .inset(0)
      .key(key)
      .opacity(beat(t0, t0 + 0.55f));
}

auto Minard1869::river(const std::vector<SkPoint>& pts, float width,
                       SkColor4f colour, const char* key, float t0) -> Element {
  return inked(smooth(pts), stroke(width, Fill::color(colour)), t0, t0 + 0.4f)
      .key(key);
}

auto Minard1869::hannibalPanel() -> Element {
  struct Water {
    std::vector<SkPoint> pts;
    const char* key;
    float t0;
  };
  const std::array<Water, 5> rivers{
      {{{{206, 190}, {198, 240}, {212, 280}, {200, 322}}, "ebre", 0.9f},
       {{{796, 150}, {806, 210}, {790, 268}, {800, 320}, {780, 358}},
        "rhone",
        0.95f},
       {{{910, 236}, {930, 268}, {952, 286}}, "isere", 1.0f},
       {{{1040, 300}, {1010, 330}, {980, 350}}, "arc", 1.0f},
       {{{1160, 430}, {1200, 452}, {1250, 462}}, "po", 1.05f}}};

  // The place names, in three hands: spaced roman capitals for a region,
  // a fine sloped italic for a town or a river, and the same italic in a
  // lighter ink for a people.
  auto place = [this](const Place& p, size_t i) {
    const weave::Type st =
        p.kind == 0 ? weave::Type{.face = faceRoman, .size = 11, .track = 2.6f}
        : p.kind == 3
            ? weave::Type{.face = faceItalic,
                          .size = 10,
                          .color = hexColor(0x4e4436),
                          .track = 1.2f}
            : weave::Type{.face = faceItalic, .size = 9, .track = 0.2f};
    return text(p.name)
        .font(st)
        .at({p.x, p.y})
        .key("hp" + std::to_string(i))
        .opacity(beat(tHann + 1.0f + 0.012f * (float)i,
                      tHann + 1.4f + 0.012f * (float)i));
  };

  // THE BAND — brush::Ribbon on the width Profile seam, over the plate's
  // own strengths. This is the primitive the whole sheet is made of.
  const SkPath spine = polylineH(plate.hannibal);
  const WidthProfile prof = profileOfH(plate.hannibal);
  // the ten numbers, written ACROSS the zones ("écrits en travers"),
  // Orient::Tangent on a cross-segment
  std::vector<Element> numbers;
  for (size_t i = 0; i + 1 < plate.hannibal.size(); ++i) {
    if (i > 0 && plate.hannibal[i].men == plate.hannibal[i - 1].men) continue;
    numbers.push_back(
        bandNumber({plate.hannibal[i].x, plate.hannibal[i].y},
                   {plate.hannibal[i + 1].x - plate.hannibal[i].x,
                    plate.hannibal[i + 1].y - plate.hannibal[i].y},
                   plate.hannibal[i].men, 8.2f, "hn" + std::to_string(i),
                   tHann + 0.7f + 0.06f * (float)i));
  }

  return box().inset(0).children(
      {lettering(doc()["hannibal"]["titles"], 0.0f, tHann), hannibalSea(),
       // The Pyrenees and the Alps, hachured by Lehmann's rule.
       lehmann({{{562, 302, 58, 15}}, {{604, 288, 50, 13}}}, 440, 250, 720, 350,
               "pyrenees", tHann + 0.55f),
       lehmann({{{1046, 336, 52, 30}},
                {{1086, 382, 62, 34}},
                {{1002, 296, 40, 22}},
                {{1128, 432, 54, 30}},
                {{1064, 476, 72, 26}},
                {{1176, 392, 44, 24}}},
               900, 220, 1290, 570, "alps", tHann + 0.7f),
       each(rivers,
            [this](const Water& w) {
              return river(w.pts, 0.8f, hexColor(0x4e4436, 0.8f), w.key,
                           tHann + w.t0);
            }),
       bandElement(spine, prof, kZone, "hband",
                   beat(tHann + 0.55f, tHann + 1.6f)),
       std::move(numbers), each(plate.places, place),
       // The LEGEND BOX — top right, and it is the second, independent
       // statement of the same scale rule.
       legendBox(),
       // the scale bar, in the panel's OWN lieue (4,560 m — not the
       // Russian panel's 4,444.8 m; the two panels use different lieues)
       scaleBar(640, 500, 14.09f / 3.0f, 30, 5, word("hannibal", "bar"), "hbar",
                tHann + 1.45f),
       // the compass arrow in the Mediterranean
       inked(segment({846, 486}, {900, 424}),
             lines::presets::arrow(1.2f, Fill::color(kInk), 9.0f), tHann + 1.5f,
             tHann + 1.75f)
           .key("compass")});
}

auto Minard1869::legendBox() -> Element {
  const float w = 344, h = 108;
  const data::Json& legend = doc()["hannibal"]["legend"];
  return box()
      .rect(SkRect::MakeXYWH(1010, 52, w, h))
      .shape(pathFn(rectPath(0, 0, w, h)))
      .stroke(stroke(1.0f, Fill::color(kInk)))
      .key("legendbox")
      .opacity(beat(tHann + 1.5f, tHann + 1.8f))
      .children({text(legend["heading"]).font({.size = 14}).at({140, 4}),
                 box()
                     .column()
                     .at({8, 24})
                     .width(w - 16)
                     .gap(6.4f)
                     .font({.size = 9.6f, .track = 0.05f})
                     .children({each(legend["lines"].items(),
                                     [](const data::Json& n) {
                                       return text(std::string(n.text()));
                                     })})});
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
  // a whole style, because the run is measured in it before it is placed
  const weave::TextStyle style = weave::textStyle(
      {.face = faceNum, .size = size, .color = kInk, .track = 0.2f});
  float runLen = 0;
  float slack = size * 0.3f;  // metrics-free fallback, same shape
  if (fonts) {
    runLen = runPens(french(men), style, *fonts).back();
    slack = metrics(style, *fonts).capSlack();
  }
  const float half = std::max(bandPx(men) * 0.5f, runLen * 0.5f) + slack;
  const SkPoint a{at.x() - n.x() * half, at.y() - n.y() * half};
  const SkPoint b{at.x() + n.x() * half, at.y() + n.y() * half};
  return text(french(men), style)
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
  // The advance: one trunk and two branches, each the same band brush
  // over its own strength law, revealed from the Niemen eastward the way
  // the army walked it.
  struct Leg {
    const std::vector<Station>* st;
    const char* key;
    float t0, t1;
  };
  const std::array<Leg, 3> legs{{{&plate.advTrunk, "advTrunk", 0.0f, 1.6f},
                                 {&plate.advNorth, "advNorth", 0.35f, 0.8f},
                                 {&plate.advPolotzk, "advPol", 0.55f, 1.2f}}};
  return box().inset(0).children({each(legs, [this](const Leg& l) {
    return bandElement(polyline(*l.st), profileOf(*l.st), kZone, l.key,
                       beat(tAdv + l.t0, tAdv + l.t1));
  })});
}

auto Minard1869::napoleonPanel() -> Element {
  auto g = box().inset(0);
  // the legend as a PARAGRAPH, which is what it is — not a key.
  g.children(
      {lettering(doc()["napoleon"]["titles"], kDivHN, tLegend),
       each(plate.legend, [this](const std::string& line, size_t i) {
         const float t0 = tLegend + 0.25f + 0.16f * (float)i;
         return text(line)
             .font({.size = 9.8f, .track = 0.02f})
             .at({i == 3 ? 148.0f : 128.0f, kDivHN + 58 + 14.6f * (float)i})
             .key("nleg" + std::to_string(i))
             .mask(by::edge(0.0f, beat(t0, t0 + 0.3f)));
       })});

  // the rivers of the Russian panel, each with its name in the sloped hand
  struct Water {
    std::vector<SkPoint> pts;
    const char* label;
    SkPoint at;
    const char* key;
    float t0;
  };
  const std::array<Water, 4> rivers{{{{{mapX(23.7f), mapY(56.0f)},
                                       {mapX(23.95f), mapY(55.3f)},
                                       {mapX(23.8f), mapY(54.8f)},
                                       {mapX(24.0f), mapY(54.2f)},
                                       {mapX(23.9f), mapY(53.8f)}},
                                      "Niémen R.",
                                      {mapX(23.45f), 774.0f},
                                      "rNiemen",
                                      -0.2f},
                                     {{{mapX(28.6f), mapY(54.9f)},
                                       {mapX(28.45f), mapY(54.5f)},
                                       {mapX(28.6f), mapY(54.1f)},
                                       {mapX(28.4f), mapY(53.7f)}},
                                      "Bérézina R.",
                                      {mapX(28.2f), mapY(54.72f)},
                                      "rBerez",
                                      -0.15f},
                                     {{{mapX(31.2f), mapY(54.05f)},
                                       {mapX(30.9f), mapY(54.5f)},
                                       {mapX(31.05f), mapY(54.95f)},
                                       {mapX(30.7f), mapY(55.4f)}},
                                      "Dniéper R.",
                                      {mapX(30.95f), mapY(54.35f)},
                                      "rDniepr",
                                      -0.1f},
                                     {{{mapX(36.6f), mapY(56.05f)},
                                       {mapX(36.9f), mapY(55.7f)},
                                       {mapX(37.3f), mapY(55.45f)}},
                                      "Moskowa R.",
                                      {mapX(36.35f), mapY(55.95f)},
                                      "rMoskowa",
                                      -0.05f}}};
  // --- THE ADVANCE ------------------------------------------------------
  // The red-brown is a SEPARATE STONE from the black, so it is very
  // slightly out of register. One translate, and it is the single most
  // convincing "this is a lithograph" cue on the plate.
  //
  // The zones read the 12.6% morph the way every other band on the sheet
  // does: through the width law, at paint. Nothing here re-describes when
  // it moves. The stone also took unevenly: a very low-amplitude speckle
  // in the zone colour, NOT a gradient (the Commons p10/p90 are two units
  // apart).
  g.children(
      {each(rivers,
            [this](const Water& w) {
              const float t0 = tAdv + w.t0;
              return box().inset(0).children(
                  {river(w.pts, 0.7f, hexColor(0x4e4436, 0.85f), w.key, t0),
                   text(w.label)
                       .font({.face = faceItalic,
                              .size = 8,
                              .color = hexColor(0x4e4436),
                              .track = 0.6f})
                       .at(w.at)
                       .key(std::string(w.key) + "L")
                       .opacity(beat(t0 + 0.2f, t0 + 0.5f))});
            }),
       box().inset(0).translateX(0.4f).translateY(-0.3f).children(
           {advanceZones(), box()
                                .inset(0)
                                .fill(tintSpeckle.material())
                                .blend(SkBlendMode::kMultiply)
                                .opacity(0.06f)
                                .cache(Cache::Texture)
                                .key("tintwander")})});

  // --- THE RETREAT ------------------------------------------------------
  struct Leg {
    const std::vector<Station>* st;
    const char* key;
    float t0, t1;
  };
  const std::array<Leg, 4> back{{{&plate.retEast, "retEast", 0.0f, 0.9f},
                                 {&plate.retPolotzk, "retPol", 0.7f, 1.0f},
                                 {&plate.retWest, "retWest", 0.85f, 1.7f},
                                 {&plate.retNorth, "retNorth", 1.5f, 1.8f}}};
  g.children({each(back, [this](const Leg& l) {
    return bandElement(polyline(*l.st), profileOf(*l.st), kInkDeep, l.key,
                       beat(tRet + l.t0, tRet + l.t1));
  })});

  // The arithmetic of the splits, as a footnote row along the bottom of
  // the map panel — five identities, all exact, on numbers Minard
  // engraved, and they need no measurement at all. A row at one gap, so
  // nothing here counts characters to place the next one.
  const data::Json& napoleon = doc()["napoleon"];
  g.children({box()
                  .row()
                  .at({kFrameL + 16, kDivNT - 26})
                  .gap(20)
                  .font({.face = faceUiBold, .size = 9.5f})
                  .ink(kBlue)
                  .key("identities")
                  .children({each(napoleon["identities"].items(),
                                  [this](const data::Json& n, size_t i) {
                                    const float t0 =
                                        tAdv + 0.5f + 0.35f * (float)i;
                                    return text(std::string(n.text()))
                                        .key("ar" + std::to_string(i))
                                        .opacity(beat(t0, t0 + 0.25f));
                                  }),
                             text(word("napoleon", "identitiesVerdict"))
                                 .ink(kPass)
                                 .key("arok")
                                 .opacity(beat(tRet + 1.8f, tRet + 2.1f))})});

  // --- the engraved numbers --------------------------------------------
  auto numbersFor = [&](const std::vector<Station>& st, const char* tag,
                        float t0, float dt) {
    for (size_t i = 0; i + 1 < st.size(); ++i) {
      if (i > 0 && st[i].men == st[i - 1].men) continue;
      const SkPoint a = stationPt(st[i]), b = stationPt(st[i + 1]);
      g.children(
          {bandNumber({(a.x() + b.x()) * 0.5f, (a.y() + b.y()) * 0.5f},
                      {b.x() - a.x(), b.y() - a.y()}, st[i].men, kNumSize,
                      tag + std::to_string(i), t0 + dt * (float)i)});
    }
  };
  numbersFor(plate.advTrunk, "nA", tAdv + 0.4f, 0.07f);
  numbersFor(plate.advNorth, "nB", tAdv + 0.5f, 0.05f);
  numbersFor(plate.advPolotzk, "nC", tAdv + 0.8f, 0.05f);
  numbersFor(plate.retEast, "nD", tRet + 0.2f, 0.06f);
  numbersFor(plate.retWest, "nE", tRet + 0.9f, 0.06f);
  numbersFor(plate.retPolotzk, "nF", tRet + 0.8f, 0.05f);
  // what recrossed the Niemen
  // --- the place names --------------------------------------------------
  // MOSCOU alone is set in spaced roman capitals, and it is the only word
  // on the map that is.
  g.children(
      {bandNumber({mapX(23.95f), mapY(54.4f)}, {1, 0}, 10000, kNumSize, "nG",
                  tRet + 1.7f),
       each(plate.cities, [this](const City& c, size_t i) {
         const bool moscou = c.plate == "Moscou";
         Element e =
             moscou ? text("MOSCOU")
                          .font({.face = faceRoman, .size = 13, .track = 2.2f})
                          .textStroke(0.5f, Fill::currentInk())
                    : text(c.plate).font(
                          {.face = faceItalic, .size = 9.6f, .track = 0.2f});
         return e.at({mapX(c.lon) + c.dx, mapY(c.lat) + c.dy})
             .key("city" + std::to_string(i))
             .opacity(beat(tAdv + 0.1f + 0.03f * (float)i,
                           tAdv + 0.4f + 0.03f * (float)i));
       })});

  // THE FLOOR, drawn on the plate itself: the blue outline is the width
  // Minard's crayon actually laid at the last treads (5.4 px on the
  // Commons scan = 3.54 px here), against the black band this sketch
  // draws from the rule. The last 4,000 men are 2.6x over.
  {
    const float floorPx = 5.4f * 0.6549f;
    const float y = mapY(54.4f);
    SkPathBuilder fb;
    fb.addRect(SkRect::MakeLTRB(mapX(24.1f), y - floorPx * 0.5f, mapX(25.0f),
                                y + floorPx * 0.5f));
    g.children(
        {inked(fb.detach(), stroke(0.9f, Fill::color(kBlue)))
             .key("floorink")
             .opacity(beat(tScale + 1.4f, tScale + 1.7f)),
         box()
             .inset(0)
             .ink(kBlue)
             .children({lettering(napoleon["notes"], y + 10, tScale + 1.5f)})
             .translateX(mapX(24.1f))});
  }

  // THE CALIPER LIES. Minard's own bar, read against Minard's own map.
  // the lieue bar, and its ticks
  g.children(
      {box()
           .inset(0)
           .styleSheet(weave::StyleSheet{
               {"amberInk", weave::Type{.color = kAmber}},
               {"amberQuiet", weave::Type{.color = hexColor(0xb5761e, 0.9f)}}})
           .children({lettering(napoleon["bar.remarks"], 0.0f, tBar)}),
       scaleBar(mapX(33.4f), 930.0f, 4.985f * 0.6549f, 50, 5,
                word("napoleon", "bar"), "nbar", tAdv + 1.7f)});
  return g;
}

auto Minard1869::scaleBar(float x, float y, float pxPerUnit, int span, int step,
                          const Utf8& label, const char* key, float t0)
    -> Element {
  // A graduated bar. `pxPerUnit` is px per lieue, `span` the last label,
  // `step` the label interval. The plate's own graduation runs 0 5 10 15
  // 20 25 ...... 50, so the middle of the ladder is left blank and only
  // the ends are numbered. Built by hand rather than stamped along a
  // contour: each graduation carries its own number, and a stamped
  // element cannot know which sample it is.
  std::vector<int> marks;
  for (int v = 0; v <= span; v += step)
    if (v <= 25 || v >= span) marks.push_back(v);
  // the ticks as one stroked path so they are one node
  SkPathBuilder tb;
  for (int v : marks) {
    const float tx = x + pxPerUnit * (float)v;
    tb.moveTo(tx, y - 4);
    tb.lineTo(tx, y);
  }
  const float w = pxPerUnit * (float)span;
  return box()
      .inset(0)
      .key(key)
      .opacity(beat(t0, t0 + 0.3f))
      .children(
          {inked(segment({x, y}, {x + w, y}), stroke(0.9f, Fill::color(kInk))),
           inked(tb.detach(), stroke(0.9f, Fill::color(kInk))),
           each(marks,
                [this, x, y, pxPerUnit](int v) -> Element {
                  return text(std::to_string(v))
                      .font({.face = faceNum, .size = 6.5f})
                      .at({x + pxPerUnit * (float)v - 3, y + 2});
                }),
           text(label)
               .font({.face = faceItalic, .size = 7.5f, .track = 0.1f})
               .at({x, y - 18})});
}

auto Minard1869::temperaturePanel() -> Element {
  auto g = box().inset(0);
  g.children({engraved({.words = word("temperature", "title"),
                        .x = 300,
                        .y = kDivNT + 4,
                        .size = 13,
                        .track = 0.3f,
                        .t0 = tTemp,
                        .t1 = tTemp + 0.3f,
                        .key = "tempTitle"})});

  // the INVERTED axis: 0 at the top, 30 degrés at the bottom
  const std::array<int, 7> degrees{0, 5, 10, 15, 20, 25, 30};
  g.children({each(degrees, [this](int r) {
    const float y = tempY((float)-r);
    const float t0 = tTemp + 0.1f + 0.03f * (float)r;
    return box().inset(0).children(
        {inked(segment({kFrameL, y}, {kFrameR - 34, y}),
               stroke(0.4f, Fill::color(hexColor(0x4e4436, 0.45f))), t0,
               t0 + 0.35f)
             .key("taxis" + std::to_string(r)),
         text(r == 30 ? "30 degrés" : std::to_string(r))
             .font({.face = faceNum, .size = 7})
             .at({kFrameR - 30, y - 4})
             .key("tlab" + std::to_string(r))
             .opacity(beat(t0 + 0.05f, t0 + 0.4f))});
  })});

  // the curve, and the fine ticks hatched UNDER it (not a fill)
  std::vector<SkPoint> curve;
  curve.reserve(std::size(plate.temps));
  for (const Temp& t : plate.temps)
    curve.push_back({mapX(t.lon), tempY(t.reaumur)});
  SkPathBuilder cb;
  for (size_t i = 0; i < curve.size(); ++i)
    i == 0 ? cb.moveTo(curve[i]) : cb.lineTo(curve[i]);
  const SkPath curvePath = cb.detach();
  // the hatched underside: short ticks hanging off the curve
  // THE DROPLINES. Nine of them, from the retreat band down through the
  // divider into the graph. They are the joint between the two panels
  // and they are the whole design. Nothing declares that the two panels
  // share an abscissa: the lock is that both call the same mapX(lon).
  //
  // The annotation beside each is as engraved. 8bre / 9bre / Xbre are
  // October / November / December — the old Roman-calendar notation, and
  // a caption that "corrects" Xbre to 10bre is wrong twice over.
  g.children({inked(curvePath, stroke(1.2f, Fill::color(kInk)))
                  // right to left, the way the retreat runs
                  .mask(by::edge(180.0f, beat(tTemp + 0.4f, tTemp + 1.1f)))
                  .key("tcurve"),
              pen(
                  "thatch",
                  [curvePath](Pen& p) {
                    p.noFill();
                    p.stroke(hexColor(0x38301f, 0.9f));
                    p.strokeWeight(0.55f);
                    SkContourMeasureIter it(curvePath, false);
                    while (sk_sp<SkContourMeasure> m = it.next()) {
                      const float len = m->length();
                      // the loop walks a distance; the accumulated float
                      // is the position
                      // NOLINTNEXTLINE(clang-analyzer-security.FloatLoopCounter,bugprone-float-loop-counter)
                      for (float d = 0; d < len; d += 3.0f) {
                        SkPoint q;
                        SkVector tn;
                        if (!m->getPosTan(d, &q, &tn)) continue;
                        p.line(q.x(), q.y(), q.x() - 1.4f, q.y() + 5.0f);
                      }
                    }
                  },
                  Cache::Texture)
                  .inset(0)
                  .key("thatch")
                  .opacity(beat(tTemp + 0.6f, tTemp + 1.2f)),
              each(plate.temps, [this](const Temp& t, size_t i) {
                const float x = mapX(t.lon);
                SkPathBuilder d;
                d.moveTo(x, mapY(54.3f));
                d.lineTo(x, tempY(t.reaumur));
                // the rule fades as it crosses the panel divider
                PathFormat f{.width = 0.7f,
                             .strokeFill = Paint::linearUnit(
                                 {0, 0}, {0, 1},
                                 {{0.0f, hexColor(0x4e4436, 0.80f)},
                                  {0.66f, hexColor(0x4e4436, 0.22f)},
                                  {1.0f, hexColor(0x4e4436, 0.75f)}})};
                return box().inset(0).children(
                    {inked(d.detach(), f, tTemp + 0.25f + 0.05f * (float)i,
                           tTemp + 0.55f + 0.05f * (float)i)
                         .key("drop" + std::to_string(i)),
                     text(t.label)
                         .font({.face = faceNum, .size = 7.4f, .track = 0.1f})
                         .at({x - 26, tempY(t.reaumur) + 5})
                         .key("tann" + std::to_string(i))
                         .opacity(beat(tTemp + 0.5f + 0.06f * (float)i,
                                       tTemp + 0.8f + 0.06f * (float)i))});
              })});

  // the undated −11°, and its two independent recoveries
  const data::Json& temperature = doc()["temperature"];
  g.children(
      {box()
           .column()
           .at({mapX(29.2f) - 26, tempY(-11) + 16})
           .width(260)
           .gap(3)
           .font({.face = faceUi, .size = 8, .color = kBlue})
           .children({each(temperature["recoveries"].items(),
                           [this](const data::Json& n, size_t i) -> Element {
                             const float t0 = tTemp + 1.35f + 0.15f * (float)i;
                             return text(std::string(n.text()))
                                 .opacity(beat(t0, t0 + 0.25f));
                           })}),
       lettering(temperature["notes"], 0.0f, tTemp)});
  return g;
}

auto Minard1869::imprints() -> Element {
  const data::Json& imprints = doc()["imprints"];
  return box().inset(0).children(
      {each(imprints.items(), [this](const data::Json& n, size_t i) {
        const bool left = n["at"].text() == "left";
        return text(n["words"])
            .font({.face = faceItalic, .size = 7})
            .at({left ? kFrameL + 6 : kFrameR - 140, kFrameB + 6})
            .key("imp" + std::to_string(i))
            .opacity(beat(1.0f, 1.3f));
      })});
}
