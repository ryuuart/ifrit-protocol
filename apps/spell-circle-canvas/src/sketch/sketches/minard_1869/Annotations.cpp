#include "Minard1869.h"

auto Minard1869::sheet() -> Element {
  // the plate is engraved in the script hand and printed in one ink; a
  // line in another face or ink says so
  return box()
      .rect(SkRect::MakeXYWH(kSheetX, kSheetY, kSheetW, kSheetH))
      .background(shadow(hexColor(0x000000, 0.55f), {6, 10}, 26))
      .font({.face = faceScript})
      .ink(kInk)
      .children({paperGround(), frames(), hannibalPanel(), napoleonPanel(),
                 temperaturePanel(), imprints(), provenance(),
                 box()
                     .inset(0)
                     .fill(Fill::color(hexColor(0x120f0b)))
                     .opacity(&dimAmt)
                     .key("dim")})
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
      .children({slot("caliper")})
      .key("sheet")
      .opacity(beat(0.0f, 0.6f));
}

auto Minard1869::card(const data::Json& spec, float t0, Element body)
    -> Element {
  // A card's lines are the interface face in the audit's grey unless one
  // says otherwise: a finding in blue, a claim in red, a verdict in the
  // card's own ink. The plate, the head and the rule under it are the
  // kit's panel; the body stands in the panel's own well.
  const std::string key(spec["key"].text());
  return kit::panel(
             {.title = std::string(spec["title"].text()),
              .rule = Fill::color(kCardInk),
              .gap = 13,
              .body = kit::Well{.ground = Fill::color(kCard),
                                .padding = 18,
                                .paddingY = 12,
                                .clip = false,
                                .keyline = Fill::color(hexColor(0xcfc6b4))}},
             std::move(body))
      .rect(SkRect::MakeXYWH(kAuditX, (float)spec["y"].number(), kAuditW,
                             (float)spec["h"].number()))
      .styleSheet(cardSheet)
      .font({.face = faceUi})
      .ink(kGrey)
      .key(key)
      .opacity(beat(t0, t0 + 0.4f))
      .translateY(bind(&T).window(t0, t0 + 0.4f).invert().scale(14));
}

auto Minard1869::cardScale(const data::Json& said) -> Element {
  const float slope = 3.828f, intercept = -0.19f;
  // WHAT THE TWO AXES MEAN: men across, band px up. The fitted line is a
  // FUNCTION of the men, the eleven measured treads are marks of the
  // table, and the four words in the field are labels — so nothing here
  // turns a strength into a pixel.
  const sketch::kit::Plot field{
      .x = {.domain = {0, 440000}}, .y = {.domain = {0, 180}}, .pad = 6};
  const auto tread = [this](const Measured&, size_t i) -> Element {
    return box()
        .width(7.2f)
        .height(7.2f)
        .shape(shapes::circle())
        .fill(Fill::currentInk())
        .key("tread" + std::to_string(i))
        .opacity(beat(tScale + 0.6f + 0.09f * (float)i,
                      tScale + 0.8f + 0.09f * (float)i));
  };
  // THE TWO RULES, DRAWN PROPORTIONAL: each length IS its millimetre
  // value, so the 12.6% is a length rather than a caption.
  const float rx = 650, rwUnit = 268;
  auto ruleRow = [&](const data::Json& n, size_t i) -> Element {
    const float y = 118 + 56.0f * (float)i;
    const float t0 = tScale + (float)n["t0"].number();
    const SkColor4f col = n["class"].text() == "claim" ? kClaimRed : kBlue;
    return box()
        .inset(0)
        .ink(col)
        .key(std::string(n["key"].text()))
        .opacity(beat(t0, t0 + 0.3f))
        .children({box()
                       .inset(0)
                       .shape(segFn({rx, y},
                                    {rx + rwUnit * (float)n["mm"].number(), y}))
                       .stroke(spans::upTo(beat(t0, t0 + 0.3f)),
                               stroke(2.0f, Fill::currentInk())),
                   text(n["value"])
                       .font({.face = faceUiBold, .size = 17})
                       .at({rx, y - 26}),
                   text(n["what"]).font({.size = 10}).at({rx, y + 6})});
  };
  return box().inset(0).children(
      {sketch::kit::plot(
           "fit", field,
           {sketch::kit::axis({.of = sketch::kit::Axis::X, .numbers = false}),
            sketch::kit::axis({.of = sketch::kit::Axis::Y, .numbers = false}),
            sketch::kit::trace(
                [slope, intercept](double men) {
                  return intercept + slope * men / 10000.0;
                },
                {.pen = {.width = 1.6f}, .styleClass = "measured"}),
            sketch::kit::marks(plate.treads, tread,
                               {.x = &Measured::men,
                                .y = &Measured::mm,
                                .styleClass = "measured"}),
            sketch::kit::label(
                std::string(said["fit"].text()), 10000, 176,
                {.anchor = {.across = Align::Start, .down = Align::Start},
                 .styleClass = "measured"}),
            sketch::kit::label(
                std::string(said["proportional"].text()), 10000, 154,
                {.anchor = {.across = Align::Start, .down = Align::Start}}),
            sketch::kit::label(
                "men →", 440000, 0,
                {.anchor = {.across = Align::End, .down = Align::Start}}),
            sketch::kit::label(
                "px", 0, 180,
                {.anchor = {.across = Align::End, .down = Align::End}})})
           .rect(SkRect::MakeXYWH(50, 36, 560, 218)),
       each(said["rules"].items(), ruleRow),
       // the tick at one millimetre, so the two lengths are read against
       // the same origin
       box()
           .inset(0)
           .shape(segFn({rx + rwUnit, 110}, {rx + rwUnit, 182}))
           .stroke(PathFormat{.width = 1.0f,
                              .strokeFill = Fill::color(kGrey),
                              .dashIntervals = {3, 3}})
           .key("ruleTick")
           .opacity(beat(tScale + 2.0f, tScale + 2.3f)),
       lettering(said["notes"], 0.0f, tScale)});
}

auto Minard1869::cardFloor(const data::Json& said) -> Element {
  const std::vector<Measured>& pts = plate.floorPts;
  // THE DOMAIN IS THE LOGARITHM. The floor is a fact about the smallest
  // strengths, which crowd into the last twentieth of a linear axis, so
  // the abscissa is log10(men).
  const sketch::kit::Plot field{
      .x = {.domain = {3.5, 5.05}}, .y = {.domain = {3.0, 11.5}}, .pad = 4};
  // The measured staircase, read as a function of the logarithm: the
  // treads are a step law, so the walk between two of them holds the
  // wider one's width — which is what a crayon does.
  const auto laid = [&pts](double logMen) {
    // the widest tread at or under the sample: a crayon holds one width
    // until the next reading takes over, which is what a step law is
    double px = pts.empty() ? 0.0 : pts.back().mm, widest = -1e9;
    for (const Measured& m : pts) {
      const double at = std::log10((double)m.men);
      if (at <= logMen && at > widest) {
        widest = at;
        px = m.mm;
      }
    }
    return px;
  };
  const auto dot = [this](const Measured&, size_t i) -> Element {
    return box()
        .width(6)
        .height(6)
        .shape(shapes::circle())
        .fill(Fill::currentInk())
        .styleClass(i >= 8 ? "amber" : "")
        .key("fp" + std::to_string(i))
        .opacity(beat(tScale + 1.0f + 0.05f * (float)i,
                      tScale + 1.2f + 0.05f * (float)i));
  };
  const std::array<float, 4> ticked{4000, 10000, 30000, 100000};
  return box().inset(0).children(
      {sketch::kit::plot(
           "floor", field,
           {sketch::kit::axis(
                {.of = sketch::kit::Axis::X,
                 .ticks = {std::log10(4000.0), std::log10(10000.0),
                           std::log10(30000.0), std::log10(100000.0)},
                 .tickLine =
                     [&ticked](double v) {
                       const size_t i = v < 3.7   ? 0
                                        : v < 4.2 ? 1
                                        : v < 4.7 ? 2
                                                  : 3;
                       return sketch::kit::tickLabel(french(ticked[i]));
                     }}),
            // the crayon floor: the width below which no line was laid
            sketch::kit::rules({.y = {3.83}, .styleClass = "grey"}),
            sketch::kit::trace(laid, {.pen = {.width = 1.6f},
                                      .samples = 300,
                                      .styleClass = "measured"}),
            sketch::kit::marks(pts, dot,
                               {.x =
                                    [](const Measured& m) {
                                      return std::log10(
                                          std::max((double)m.men, 1000.0));
                                    },
                                .y = &Measured::mm,
                                .styleClass = "measured"}),
            sketch::kit::label(std::string(said["slope"].text()),
                               std::log10(30000.0), 11.4,
                               {.anchor = {.down = Align::Start}})})
           .rect(SkRect::MakeXYWH(50, 36, 470, 96)),
       lettering(said["notes"], 0.0f, tScale + 0.8f)});
}

auto Minard1869::cardGeo(const data::Json& said) -> Element {
  // THE INSET MAP: Minard's cities as dots against the gazetteer's
  // positions as crosses, with the residual exaggerated eightfold so a
  // five-kilometre error is visible at all. The frame says what the two
  // axes mean — degrees of longitude across, degrees of latitude up — and
  // every dot, cross, vector and label reads the same mapping.
  constexpr float exagg = 8.0f;
  const sketch::kit::Plot field{
      .x = {.domain = {23.5, 38.5}}, .y = {.domain = {53.6, 56.4}}, .pad = 8};
  // the route itself, so the dots read as a campaign and not a scatter
  const auto route = [this](const sketch::kit::Plot& f) {
    const std::vector<Station>* legs[] = {&plate.advTrunk, &plate.retEast,
                                          &plate.retWest};
    return box()
        .absolute()
        .inset(0)
        .styleClass("route")
        .shape([this, f, legs](SkSize field) {
          SkPathBuilder rt;
          for (const std::vector<Station>* v : legs)
            for (size_t i = 0; i < v->size(); ++i) {
              const SkPoint q = f.at((*v)[i].lon, (*v)[i].lat, field);
              i == 0 ? rt.moveTo(q) : rt.lineTo(q);
            }
          return rt.detach();
        })
        .stroke(spans::upTo(beat(tGeo, tGeo + 0.5f)),
                stroke(1.4f, Fill::currentInk()));
  };
  // the residual vectors and the crosses at their far ends, one recording
  // each because a segment is not one of the family's marks
  const auto residuals = [this](bool crosses) {
    return [this, crosses](const sketch::kit::Plot& f) {
      return box()
          .absolute()
          .inset(0)
          .styleClass(crosses ? "cross" : "vector")
          .shape([this, f, crosses](SkSize field) {
            SkPathBuilder p;
            for (const City& c : plate.cities) {
              const SkPoint m = f.at(c.lon, c.lat, field);
              const SkPoint r = f.at(c.lon + (c.rlon - c.lon) * exagg,
                                     c.lat + (c.rlat - c.lat) * exagg, field);
              if (!crosses) {
                p.moveTo(m);
                p.lineTo(r);
                continue;
              }
              p.moveTo(r.x() - 3, r.y());
              p.lineTo(r.x() + 3, r.y());
              p.moveTo(r.x(), r.y() - 3);
              p.lineTo(r.x(), r.y() + 3);
            }
            return p.detach();
          })
          .stroke(spans::upTo(beat(tGeo + (crosses ? 0.2f : 0.5f),
                                   tGeo + (crosses ? 0.6f : 1.1f))),
                  stroke(crosses ? 1.0f : 0.8f, Fill::currentInk()));
    };
  };
  const auto city = [this](const City& c, size_t i) -> Element {
    const bool out = cityKm(c) > 20.0f;
    const float side = out ? 8.0f : 5.2f;
    return box()
        .width(side)
        .height(side)
        .shape(shapes::circle())
        .fill(Fill::currentInk())
        .styleClass(out ? "amber" : "")
        .key("gc" + std::to_string(i))
        .opacity(beat(tGeo + 0.1f + 0.02f * (float)i,
                      tGeo + 0.35f + 0.02f * (float)i));
  };
  // the histogram of the 20 residuals, in five-kilometre bins, against
  // the digitisation quantum standing behind it
  std::array<double, 8> bins{};
  for (const City& c : plate.cities)
    bins[(size_t)std::min(7, (int)(cityKm(c) / 5.0f))] += 1.0;
  const auto column = [this](size_t i, double) -> Element {
    return box()
        .fill(Fill::currentInk())
        .styleClass(i >= 4 ? "amber" : "")
        .scale(animate(from(0.0f).to(1.0f),
                       ramp((tGeo + 1.0f) * 1000 + 60.0f * (float)i, 320)))
        .transformOrigin(0.5f, 1.0f)
        .opacity(beat(tGeo + 1.0f + 0.06f * (float)i,
                      tGeo + 1.2f + 0.06f * (float)i));
  };
  return box().inset(0).children(
      {sketch::kit::plot(
           "geo", field,
           {route, residuals(false), residuals(true),
            sketch::kit::marks(plate.cities, city,
                               {.x = &City::lon, .y = &City::lat}),
            // the three cities the residual is worst at name themselves
            sketch::kit::marks(plate.cities,
                               [this](const City& c, size_t i) -> Element {
                                 if (cityKm(c) <= 20.0f) return box();
                                 return text(c.plate)
                                     .font({.face = faceUiBold, .size = 10})
                                     .key("gcl" + std::to_string(i))
                                     .opacity(beat(tGeo + 1.6f, tGeo + 1.9f));
                               },
                               {.x = &City::lon,
                                .y = &City::lat,
                                .anchor = {.across = Align::Start,
                                           .down = Align::End,
                                           .offset = {7, -6}},
                                .styleClass = "amber"}),
            sketch::kit::label(
                std::string(said["exaggeration"].text()), 23.5, 53.6,
                {.anchor = {.across = Align::Start, .down = Align::End}})})
           .rect(SkRect::MakeXYWH(30, 34, 560, 132)),
       sketch::kit::plot(
           "hist",
           {.x = {.transform = data::Transform::Band,
                  .steps = 8,
                  .padding = 0.12},
            .y = {.domain = {0, 9}}},
           {// the digitisation quantum, as a grey band behind the columns
            sketch::kit::rules({.x = {6.41 / 5.0 - 0.5},
                                .pen = {.width = 12.0f},
                                .styleClass = "grey"}),
            sketch::kit::bands(
                bins, {.y = [](double v) { return v; }, .part = column}),
            sketch::kit::label(std::string(said["quantum"].text()),
                               6.41 / 5.0 - 0.5, 8.4,
                               {.anchor = {.across = Align::Start,
                                           .down = Align::Start,
                                           .offset = {10, 0}}})})
           .rect(SkRect::MakeXYWH(630, 34, 250, 100)),
       lettering(said["notes"], 0.0f, tGeo)});
}

auto Minard1869::cardLegs(const data::Json& said) -> Element {
  // TEN LEG RATIOS AGAINST 1.00, each a row: the name, then a bar grown
  // out of the unit rule — left where Minard squeezed the leg, right
  // where he stretched it — and the ratio after it. A deviation from a
  // centre is the one column reading the plot family draws no band for,
  // so the rows are the card's own.
  const float bx = 250, by = 42, bw = 480, rowH = 12.2f;
  const float mid = bx + bw * 0.5f;
  return box().inset(0).children(
      {inked(segment({mid, by - 4}, {mid, by + rowH * 10 + 4}),
             stroke(1.0f, Fill::color(kCardInk))),
       each(plate.legs,
            [&](const Leg& l, size_t i) -> Element {
              const float y = by + rowH * (float)i;
              const bool bad = l.ratio < 0.7f || l.ratio > 1.3f;
              const float dx = (l.ratio - 1.0f) * bw * 0.62f;
              const float t0 = tDistort + 0.1f + 0.05f * (float)i;
              return box()
                  .inset(0)
                  .styleClass(bad ? "amberInk" : "measured")
                  .opacity(beat(t0, t0 + 0.2f))
                  .children({text(l.name).font({.size = 9.5f}).at({60, y - 2}),
                             box()
                                 .rect(SkRect::MakeXYWH(
                                     dx < 0 ? mid + dx : mid, y,
                                     std::max(std::fabs(dx), 1.0f), 7))
                                 .fill(Fill::currentInk())
                                 .scale(animate(
                                     from(0.0f).to(1.0f),
                                     ramp(t0 * 1000, 420, ch::EaseOutBack())))
                                 .transformOrigin(dx < 0 ? 1.0f : 0.0f, 0.5f),
                             text(kit::formatted("%.3f", l.ratio))
                                 .font({.size = 9.5f})
                                 .at({bx + bw + 20, y - 2})});
            }),
       // Two lines of 10 pt under ten rows is what the card's 206 holds:
       // set the note any lower and the second line's baseline falls past
       // the card edge and the sentence is cut in half.
       lettering(said["notes"], 0.0f, tDistort)});
}

auto Minard1869::cardReaumur(const data::Json& said) -> Element {
  // THE NINE ENGRAVED READINGS, converted twice: five columns each at its
  // own width, the three numeric ones set in the figure register. The one
  // undated reading says so in its own cell.
  std::vector<sketch::kit::Row> rows;
  rows.push_back({.cells = wordsOf(said["columns"])});
  for (size_t i = 0; i < plate.temps.size(); ++i) {
    const Temp& t = plate.temps[i];
    rows.push_back(
        {.cells = {t.label + (i == 4 ? std::string(said["undated"].text())
                                     : std::string()),
                   kit::formatted("%.0f", t.reaumur),
                   kit::formatted("%.2f", t.reaumur * 1.25f),
                   kit::formatted("%.2f", t.reaumur * 2.25f + 32.0f),
                   i == 0 ? std::string("—") : std::to_string(t.daysSincePrev)},
         .key = "rc" + std::to_string(i)});
  }
  return box().inset(0).children(
      {sketch::kit::table(std::move(rows),
                          {.columns = {{.width = 250},
                                       {.width = 80, .figure = true},
                                       {.width = 80, .figure = true},
                                       {.width = 90, .figure = true},
                                       {}}})
           .at({40, 34})
           .width(540)
           .key("reaumurTable")
           .staggerChildren(50ms)
           .opacity(beat(tReaumur, tReaumur + 0.3f)),
       // the equations, the caption reproductions still get wrong, and
       // the two campaigns that are the reason the panels share a sheet
       lettering(said["notes"], 0.0f, tReaumur)});
}

auto Minard1869::auditColumn() -> Element {
  // THE AUDIT IS ANOTHER WORLD and it has another theme: clean paper,
  // crisp rules, the interface face, and the blue a measurement is
  // printed in as its figure colour. Every kit component under these
  // five cards reads it, so a table and a plot on a card are dressed by
  // the card's own look and not by the desk's.
  const sketch::kit::Provide look(cardLook);
  const data::Json& cards = doc()["cards"];
  // The five cards, in the order the argument runs: the scale, the floor
  // it breaks at, the geography it keeps, the geography it does not, and
  // the thermometer nobody reads.
  const std::array<float, 5> beats{tScale, tScale + 0.8f, tGeo, tDistort,
                                   tReaumur};
  return box().inset(0).children(
      {card(cards[0], beats[0], cardScale(cards[0])),
       card(cards[1], beats[1], cardFloor(cards[1])),
       card(cards[2], beats[2], cardGeo(cards[2])),
       card(cards[3], beats[3], cardLegs(cards[3])),
       card(cards[4], beats[4], cardReaumur(cards[4]))});
}

auto Minard1869::titleStrip() -> Element {
  const data::Json& head = doc()["head"];
  std::vector<sketch::kit::Line> notes;
  for (const data::Json& n : head["notes"].items())
    notes.push_back(
        {.words = std::string(n["words"].text()),
         .ink = Fill::color(n["ink"].text() == "blue" ? hexColor(0x2f6f9c)
                                                      : hexColor(0xb5761e))});
  return box()
      .rect(SkRect::MakeXYWH(48, 28, 2464, 80))
      .children({sketch::kit::titleCard(
                     {.title = {std::string(head["title"].text())},
                      .subtitle = {std::string(head["subtitle"].text())},
                      .notes = std::move(notes)})
                     .inset(0)});
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
