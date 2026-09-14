#include "ChaucerAstrolabe.h"

auto ChaucerAstrolabe::card(const data::Json& page, Element content)
    -> Element {
  // The head is `kit::sheet`'s — the title, the gloss under it and the rule
  // under both — and it FLOWS, so a two-line gloss pushes its own rule down
  // and the content after it. The two headed lines are the classes of their
  // own names, which the sheet on this plate's root states.
  return kit::sheet({.title = page["title"],
                     .subtitle = page["subtitle"],
                     .marginX = 16,
                     .marginTop = 11,
                     .marginBottom = 12,
                     .subtitleGap = 5,
                     .contentGap = 13,
                     .ground = Fill::color(hexColor(0xe8dcc2, 0.62f)),
                     .rule = Fill::color(hexColor(0x241c15, 0.28f))},
                    std::move(content))
      .stroke(stroke(1.0f, Fill::color(hexColor(0x241c15, 0.24f)),
                     PathFormat::Align::Inner));
}

auto ChaucerAstrolabe::rack() -> Element {
  // The commentary is two columns of cards, and a card says only how tall
  // it is: the one that closes a column takes what is left of it. Nothing
  // inside a card carries a canvas coordinate.
  return kit::at(kRackX, kRackY, kRackW, kRackH)
      .row()
      .gap(30)
      .children({box().width(kNarrow).gap(kCardGap).children(
                     {projectionPanel(), familiesPanel(), backPanel().grow(1)}),
                 box().width(kWide).gap(kCardGap).children(
                     {specCard(), starPanel(), chaucerPanel(),
                      zodiacPanel().grow(1)})});
}

auto ChaucerAstrolabe::projectionPanel() -> Element {
  const data::Json& page = doc["projection"];
  const SkPoint c = kSectionC;
  const float rr = kSectionR;
  const float chord = rr * std::cos(kEps * kD);
  const float rise = rr * std::sin(kEps * kD);
  const Fill hair = Fill::color(hexColor(0x241c15, 0.42f));
  // the section over the reading it answers
  Element section = box().height(kSectionH).shrink(0).children(
      {// the sphere seen in section, and the plane of the equator it
       // is struck onto — the plane of projection, which reaches past
       // the sphere because the plate does
       ring(c, rr, stroke(1.6f, Fill::color(hexColor(0x241c15, 0.75f)))),
       kit::line({.length = 2 * rr + 156,
                  .thickness = 2,
                  .fill = Fill::color(hexColor(0x241c15, 0.6f))})
           .at({c.fX - rr - 78, c.fY - 1}),
       // the two tropics, as the chords they are in section
       each(std::array{-1.0f, 1.0f},
            [&](float s) {
              return kit::line(
                         {.length = 2 * chord, .thickness = 1.4f, .fill = hair})
                  .at({c.fX - chord, c.fY - s * rise});
            }),
       // the poles: the north one a point on the rim, the south one the
       // EYE every ring on the plate is seen from
       dot({c.fX, c.fY - rr}, 3.4f, Fill::currentInk()),
       dot({c.fX, c.fY + rr}, 5.0f, Fill::color(kRubric)),
       text(page["north"]).at({c.fX + 14, c.fY - rr - 10}),
       text(page["south"]).styleClass("gloss").at({c.fX + 10, c.fY + rr + 2}),
       slot("projray")});
  return card(page, box().children({std::move(section), slot("projread")}));
}

auto ChaucerAstrolabe::projRay() -> Element {
  const SkPoint c = kSectionC;
  const float rr = kSectionR;
  const float dec = projDec.value();
  const SkPoint P{c.fX + rr * std::cos(dec * kD),
                  c.fY - rr * std::sin(dec * kD)};
  const SkPoint S{c.fX, c.fY + rr};
  // The equator projects onto ITSELF, so the section's sphere radius is the
  // equator's radius on the plate: the landing distance is rr·r(δ)/R_eq —
  // and the same number falls out of the straight line from S through P,
  // since cos δ/(1 + sin δ) IS tan((90−δ)/2). Two derivations, one point.
  const SkPoint Lp{c.fX + rr * rOfDec(dec) / kReq, c.fY};
  SkPathBuilder pb;
  pb.moveTo(S);
  pb.lineTo(Lp.fX + (Lp.fX - S.fX) * 0.06f, Lp.fY + (Lp.fY - S.fY) * 0.06f);
  return box().inset(0).children(
      {pathFigure(pb.detach(), 4)
           .fill(Fill::none())
           .stroke(
               PathFormat{.width = 1.5f,
                          .strokeFill = Fill::color(hexColor(0x2f6f9c, 0.9f)),
                          .dashIntervals = {6.0f, 4.0f}}),
       dot(P, 5.0f, Fill::color(kTrace)), dot(Lp, 5.0f, Fill::color(kRubric)),
       kit::line({.length = 40,
                  .thickness = 2,
                  .column = true,
                  .fill = Fill::color(hexColor(0x8c2f22, 0.55f))})
           .at({Lp.fX - 1, c.fY - 20})});
}

auto ChaucerAstrolabe::projRead() -> Element {
  const float dec = projDec.value();
  // The two lines the walk answers, and the three the projection's own
  // identity is stated in — the notes are the document's.
  return box().gap(4).styleClass("figure").children(
      {text(kit::formatted("δ = %+7.3f°", dec)),
       text(kit::formatted("r = R_eq·tan((90−δ)/2) = %.6f R", rOfDec(dec))),
       each(doc["projection"]["notes"].items(),
            [](const data::Json& n) { return text(n).styleClass("note"); })});
}

auto ChaucerAstrolabe::familiesPanel() -> Element {
  const data::Json& page = doc["families"];
  // Every curve on the plate is a circle, and the four families differ
  // only in what they are struck about. Each face is one cell: the circles
  // in units of its own radius, its name over them and its rule under.
  const float r = 54;
  const auto family = [r](const data::Json& row, std::size_t i) {
    Element face =
        box()
            .width(2 * r)
            .height(2 * r)
            .shape(shapes::circle())
            .clip(true)
            .fill(Fill::color(hexColor(0xf6efdd)))
            .stroke(stroke(1.2f, Fill::color(hexColor(0x241c15, 0.5f)),
                           PathFormat::Align::Inner));
    // one circle of the family, in units of the face's radius
    const auto put = [&](float mcx, float mcy, float mr, float w, float a) {
      face.children({ring({r + mcx * r, r - mcy * r}, mr * r,
                          stroke(w, Fill::color(hexColor(0x241c15, a))))});
    };
    // the straight member of a family, drawn where one has one
    const auto spoke = [&](float top, float length) {
      face.children({kit::line({.length = length,
                                .thickness = 1,
                                .column = true,
                                .fill = Fill::color(hexColor(0x241c15, 0.72f))})
                         .at({r - 0.5f, top})});
    };
    if (i == 0) {
      for (float radius : {1.0f, kReq, kRcan}) put(0, 0, radius, 1.3f, 0.85f);
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
      spoke(0, 2 * r);
    } else {
      for (int k = 1; k <= 11; ++k)
        if (const std::optional<path::PlaneCircle> c = seasonalLine(k))
          put(c->centre.x, c->centre.y, c->radius, 0.8f, 0.6f);
      spoke(r, r);
    }
    return sketch::kit::caption(0, row["name"], row["formula"],
                                box()
                                    .height(2 * r + 4)
                                    .alignItems(Align::Center)
                                    .children({std::move(face)}));
  };
  return card(page,
              sketch::kit::panelGrid(
                  {.cells = listOf<Element>(
                       page["rows"].items(),
                       [&, n = std::size_t{0}](const data::Json& row) mutable {
                         return family(row, n++);
                       }),
                   .columns = 2,
                   .gap = 12,
                   .rowGap = 16}));
}

auto ChaucerAstrolabe::backPanel() -> Element {
  const data::Json& page = doc["back"];
  const SkPoint c{(kNarrow - 32) * 0.5f, 172};
  const float r = 156;
  const float s = r * 0.50f;  // the shadow square's own half-side
  const Fill engraved = Fill::color(hexColor(0x3a2a10, 0.6f));
  // The back's engraving is the limb face at 9, so the ring labels say only
  // what differs from it.
  Element face = box()
                     .height(2 * r + 24)
                     .styleClass("engrave")
                     .children({dot(c, r, brass(0.44f))
                                    .foreground(styles::BevelEmboss{
                                        .depth = 2,
                                        .size = 4,
                                        .angleDeg = 125,
                                        .highlight = hexColor(0xffe9b0, 0.45f),
                                        .shadow = hexColor(0x2a1d08, 0.5f)})});

  // Four quadrants of 90° altitude scale (I.7–8): 181 rules every 2°, every
  // fifth of them heavier and reaching further in.
  //
  // TWO NODES, not 181. This was a node per rule, each carrying its own
  // bounding box and its own captured SkPath — 181 layouts, 181
  // reconciliations and 181 pictures for a ladder that never moves. It is
  // one path with N contours per stroke weight, and it needs two nodes only
  // because a node has one stroke width.
  //
  // The light pass SKIPS every fifth mark (a degenerate span), because the
  // two weights share a colour at 0.6 alpha: drawn over each other the
  // fifths would composite to 0.84 and print darker than the plate.
  const path::Frame limb{.centre = c, .radius = r, .zero = path::Zero::East};
  const auto ladder = [&](const shapes::Ticks& spec, float width) {
    face.children({pathFigure(shapes::ticks(limb, spec), 2)
                       .fill(Fill::none())
                       .stroke(stroke(width, engraved))});
  };
  ladder({.divisions = 180,
          .sweep = 360.0f,
          .closed = true,
          .mark = {0.925f, 0.96f},
          .classify =
              [](int i, shapes::Span sp) {
                return i % 5 == 0 ? shapes::Span{sp.inner, sp.inner} : sp;
              }},
         0.7f);
  ladder({.divisions = 36,
          .sweep = 360.0f,
          .closed = true,
          .mark = {0.90f, 0.96f}},
         1.1f);
  for (float rr : {0.90f, 0.96f, 0.855f, 0.78f, 0.70f})
    face.children({ring(c, r * rr, groove(r * rr, 1.2f, 0.6f, 0.3f))});

  // the calendar and zodiac rings — Chaucer I.10 gives the month lengths
  float acc = 0;
  for (int m = 0; m < 12; ++m) {
    const float days = (float)kMonthDays[m];
    const float a0 = -90.0f + acc / 365.0f * 360.0f;
    const float a1 = -90.0f + (acc + days) / 365.0f * 360.0f;
    acc += days;
    SkPathBuilder pb;
    pb.moveTo(arrange::onEllipse(c, {r * 0.78f, r * 0.78f}, a0 * kD));
    pb.lineTo(arrange::onEllipse(c, {r * 0.855f, r * 0.855f}, a0 * kD));
    const float azm =
        arrange::along(-90.0f, 360.0f, (size_t)m, 12, arrange::Turn::Closed) +
        15.0f;
    face.children(
        {pathFigure(pb.detach(), 2)
             .fill(Fill::none())
             .stroke(stroke(1.1f, Fill::color(hexColor(0x3a2a10, 0.7f)))),
         text(kMonths[m])
             .centerAt(arrange::onEllipse(c, {r * 0.817f, r * 0.817f},
                                          (a0 + a1) * 0.5f * kD)),
         text(std::string(kSigns[(m + 9) % 12]).substr(0, 3))
             .ink(hexColor(0x33240c, 0.7f))
             .centerAt(
                 arrange::onEllipse(c, {r * 0.74f, r * 0.74f}, azm * kD))});
  }

  // the shadow square: umbra recta and umbra versa, 12 divisions each (I.12)
  face.children(
      {box()
           .rect(SkRect::MakeXYWH(c.fX - s, c.fY, 2 * s, s))
           .fill(Fill::none())
           .stroke(stroke(1.4f, Fill::color(hexColor(0x3a2a10, 0.75f)))),
       text(page["umbraRecta"])
           .ink(hexColor(0x33240c, 0.8f))
           .centerAt({c.fX - s * 0.52f, c.fY + s * 0.86f}),
       text(page["umbraVersa"])
           .ink(hexColor(0x33240c, 0.8f))
           .centerAt({c.fX + s * 0.52f, c.fY + s * 0.86f})});
  for (int i = 1; i < 12; ++i) {
    const float t = (float)i / 12.0f;
    const float reach = s * (i % 3 == 0 ? 0.34f : 0.20f);
    face.children(
        {kit::line({.length = reach,
                    .thickness = 0.8f,
                    .column = true,
                    .fill = engraved})
             .at({c.fX - s + 2 * s * t, c.fY}),
         kit::line({.length = reach, .thickness = 0.8f, .fill = engraved})
             .at({c.fX - s, c.fY + s * t}),
         kit::line({.length = reach, .thickness = 0.8f, .fill = engraved})
             .at({c.fX + s - reach, c.fY + s * t})});
  }

  // the alidade, swung to 25° 30′ — the measurement II.3 starts from
  const Transition swing = ramp(tChaucer * 1000 + 200, 900, ease::outBack());
  face.children(
      {kit::at(c.fX - r * 0.97f, c.fY - 5, 2 * r * 0.97f, 10)
           .transformOrigin(0.5f, 0.5f)
           .rotate(animate(from(0.0f).to(-25.5f), swing))
           .fill(brass(0.76f))
           .foreground(stroke(1.0f, Fill::color(hexColor(0x2a1d08, 0.6f))))
           .background(shadow(hexColor(0x2a1d08, 0.45f), {2, 3}, 5)),
       each(std::array{-1.0f, 1.0f},
            [&](float side) {
              return kit::at(c.fX + side * r * 0.90f - 5, c.fY - 16, 10, 32)
                  .transformOriginPx({5.0f - side * r * 0.90f, 16})
                  .rotate(animate(from(0.0f).to(-25.5f), swing))
                  .fill(brass(0.80f))
                  .foreground(
                      stroke(1.0f, Fill::color(hexColor(0x2a1d08, 0.6f))));
            }),
       dot(c, 8, brass(0.82f))});

  return card(page,
              box().children(
                  {std::move(face),
                   text(page["reading"])
                       .styleClass("gloss")
                       .block({.alignment = weave::TextAlignment::kCenter})}));
}

auto ChaucerAstrolabe::specCard() -> Element {
  const data::Json& page = doc["spec"];
  // What the object is, as a table of a name and what answers it; then the
  // two obliquities side by side, and what the difference between them
  // costs the plate.
  return card(page,
              box().gap(9).children(
                  {sketch::kit::table(
                       listOf<sketch::kit::Row>(
                           page["rows"].items(),
                           [](const data::Json& row) -> sketch::kit::Row {
                             return {{row["key"], row["value"]}};
                           }),
                       {.columns = {{.width = 96}, {}}}),
                   kit::line({.fill = Fill::color(hexColor(0x241c15, 0.22f))}),
                   box().styleClass("figure").children(
                       {each(page["obliquity"].items(),
                             [](const data::Json& n) { return text(n); })}),
                   text(page["note"]).styleClass("gloss")}));
}

auto ChaucerAstrolabe::starPanel() -> Element {
  const data::Json& page = doc["stars"];
  // Where a star lands is r = R_eq·tan((90−δ)/2), so the twelve readings
  // and the strip beside them are one mapping: the table's last column and
  // the plot's abscissa are the same number.
  const auto radius = [](const Star& s) { return (double)rOfDec(s.dec1326); };
  const auto pointer = [](const Star& s, std::size_t i) {
    return box()
        .width(7)
        .height(7)
        .shape(shapes::circle())
        .fill(Fill::color(i == 3 ? kRubric : kInk));
  };
  // The head each column carries, so the reading and the words over it
  // are ranged by one arrangement rather than by a hand-spaced line.
  const std::vector<sketch::kit::Document::Line> heads = doc.run(page["heads"]);
  return card(
      page, box().gap(6).children(
                {box().row().gap(14).grow(1).children(
                     {sketch::kit::table(
                          listOf<sketch::kit::Row>(
                              kStars,
                              [&](const Star& s) -> sketch::kit::Row {
                                return {{s.name, s.modern,
                                         kit::formatted("%8.3f", s.ra1326),
                                         kit::formatted("%+8.3f", s.dec1326),
                                         kit::formatted("%.5f", radius(s))}};
                              }),
                          {.columns = {{heads[0].words, 116},
                                       {heads[1].words, 88},
                                       {heads[2].words, 56, true},
                                       {heads[3].words, 58, true},
                                       {heads[4].words, 52, true}}}),
                      // the strip: Cancer, the equator and Capricorn ruled
                      // across it, and one star per row against them
                      sketch::kit::plot(
                          "stars",
                          {.x = {.domain = {0.18, 1.0}},
                           .y = {.domain = {(double)kStars.size() - 0.4, -0.6}},
                           .pad = 6},
                          {sketch::kit::rules({.x = {kRcan, kReq, 1.0}}),
                           sketch::kit::marks(kStars, pointer, {.x = radius})})
                          .grow(1)}),
                 text(page["note"]).styleClass("gloss")}));
}

auto ChaucerAstrolabe::chaucerPanel() -> Element {
  const data::Json& page = doc["chaucer"];
  return card(page,
              box().gap(10).children(
                  {text(page["quote"]).styleClass("quote"), slot("chaucer")}));
}

auto ChaucerAstrolabe::chaucerBody() -> Element {
  // The worked example, line by line, in the face a call is set in; its
  // delta in the rubric, and the verdict under it in the running voice.
  return box().gap(3).styleClass("figure").children(
      {text(chaucerH), text(chaucerA), text(chaucerDelta).ink(kRubric),
       text(doc["chaucer"]["verdict"])
           .styleClass("gloss")
           .ink(kInk)
           .margin(0, 6, 0, 0)});
}

auto ChaucerAstrolabe::zodiacPanel() -> Element {
  const data::Json& page = doc["zodiac"];
  // The projection is not uniform along the ecliptic ring: Capricorn is
  // 2.26× wider on it than Cancer. The twelve spans are read off the ring
  // itself, so the bars and the rule at 30° are the same statement.
  std::array<double, 12> span{};
  for (int i = 0; i < 12; ++i) {
    const float a0 = ringAngle((float)(i * 30));
    float a1 = ringAngle((float)((i + 1) * 30));
    if (a1 < a0) a1 += 360.0f;
    span[(size_t)i] = (double)(a1 - a0);
  }
  const auto value = [](double v) { return v; };
  // a sign wider than an unprojected ring's 30° is the rubric's
  const auto bar = [&span](std::size_t i) {
    return box()
        .fill(Fill::color(span[i] > 30 ? hexColor(0x8c2f22, 0.72f)
                                       : hexColor(0x241c15, 0.62f)))
        .scaleY(rise(ramp(tYear * 1000 + (float)i * 45, 520, ease::outBack())))
        .transformOrigin(0.5f, 1.0f);
  };
  const float band = (kWide - 32) / 12.0f;
  return card(
      page,
      box().gap(4).children(
          {sketch::kit::plot(
               "zodiac",
               {.x = {.transform = data::Transform::Band,
                      .steps = 12,
                      .padding = 0.20},
                .y = {.domain = {0, 46}}},
               {sketch::kit::rules({.y = {30.0}}),
                sketch::kit::bands(span, {.y = value, .part = bar}),
                sketch::kit::marks(
                    span,
                    [](double v, std::size_t) {
                      return text(kit::formatted("%.1f", v))
                          .styleClass("plotTick");
                    },
                    {.y = value,
                     .anchor = {.down = Align::End, .offset = {0, -3}}}),
                sketch::kit::axis(
                    {.line = false,
                     .reach = 0,
                     .tickLine =
                         [](double v) {
                           return sketch::kit::tickLabel(
                               std::string(kSigns[(size_t)v]).substr(0, 3));
                         }}),
                sketch::kit::label(page["reference"], 0, 30,
                                   {.anchor = {.across = Align::Start,
                                               .down = Align::Start}})})
               .grow(1),
           // the live marker: which sign the sun stands in, under its band
           box()
               .width(band - 6)
               .height(3)
               .fill(Fill::color(kTrace))
               .translateX(bind(&signMark).scale(band))}));
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
  const data::Json& page = doc["masthead"];
  return sketch::kit::titleCard(
             {.title = {page["title"]},
              .subtitle = {page["subtitle"]},
              .notes = {{.words = page["note"],
                         .ink = Fill::color(hexColor(0x6b5a44))}},
              .ruled = true})
      .left(64)
      .top(44)
      .width(kW - 128);
}

auto ChaucerAstrolabe::readout() -> Element {
  const int hh = (int)std::floor(latHours);
  const float mm = (latHours - (float)hh) * 60.0f;
  const int n = ((int)std::lround(hourAngle.value() / 15.0f) + 24 * 4) % 24;
  const int letter = (n == 0 ? 24 : n);
  // Six readings of one number each, the two that answer WHEN set brighter
  // than the four that answer where the sky stands.
  const std::array<std::string, 6> value{
      kit::formatted("%02d:%04.1f", hh, mm),
      kit::formatted("%+8.3f°", hourAngle.value()),
      kit::formatted("%+7.3f°", sunAlt.value()),
      kit::formatted("λ %6.2f°", sunLam.value()),
      std::string(kLetters[letter - 1]) + "  (" + std::to_string(letter) + ")",
      sunAlt.value() > 0 ? std::string("— day")
                         : std::string("night ") + std::to_string(nightHour)};
  return kit::at(92, 1334, kW - 184, 60)
      .row()
      .gap(26)
      .alignItems(Align::Baseline)
      .children({each(doc["readout"].items(), [&](const data::Json& name,
                                                  std::size_t i) {
        return box().gap(1).children(
            {text(name).styleClass("dial"),
             text(value[i]).styleClass(i == 0 || i == 4 ? "time" : "readout")});
      })});
}
